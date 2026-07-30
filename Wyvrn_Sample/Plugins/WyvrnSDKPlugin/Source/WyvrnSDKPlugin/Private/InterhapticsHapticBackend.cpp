// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "InterhapticsHapticBackend.h"

#if defined(PLATFORM_PS5) && PLATFORM_PS5

#include "HapticVoicePool.h"
#include "InterhapticsRuntime.h"
#include "WyvrnHapticData.h"
#include "WyvrnHapticRuntime.h"
#include "WyvrnHapticsLog.h"
#include "WyvrnHapticsStats.h"

#include "Containers/Queue.h"
#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "UObject/UObjectGlobals.h"

#include <atomic>

DEFINE_LOG_CATEGORY(LogWyvrnHaptics);

// Verbose per-event tracing toggle for the whole SetEventName -> render-worker path.
// OFF by default; enable on the devkit console (or a config [ConsoleVariables] block) with:
//     wyvrn.Haptics.VerboseLog 1
static TAutoConsoleVariable<int32> CVarWyvrnHapticsVerboseLog(
	TEXT("wyvrn.Haptics.VerboseLog"),
	0,
	TEXT("Trace each WYVRN haptic event through every stage of the PS5 path:\n")
	TEXT("  game-thread SetEventName -> backend -> render-worker queue -> command lookup -> voice playback.\n")
	TEXT("  0: off (default)\n")
	TEXT("  1: on"),
	ECVF_Default);

// Queried from both the game thread (enqueue side) and the render worker (drain side).
bool WyvrnHapticsVerboseLoggingEnabled()
{
	return CVarWyvrnHapticsVerboseLog.GetValueOnAnyThread() != 0;
}

DECLARE_CYCLE_STAT(TEXT("Render Loop"), STAT_WyvrnHaptics_RenderLoop, STATGROUP_WyvrnHaptics);
DECLARE_CYCLE_STAT(TEXT("Set Event (enqueue)"), STAT_WyvrnHaptics_SetEvent, STATGROUP_WyvrnHaptics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Voices"), STAT_WyvrnHaptics_ActiveVoices, STATGROUP_WyvrnHaptics);

namespace
{
	// HAR material ids loaded per effect, giving polyphony and voice stealing.
	constexpr int32 kVoicesPerEffect = 4;

	// Baked event map produced by the editor importer (OutputContentPath default /Game/Wyvrn).
	const TCHAR* const kDataAssetPath = TEXT("/Game/Wyvrn/WyvrnHapticData.WyvrnHapticData");

	// The worker wakes every ~8 ms (~120 Hz) to drain queued triggers, and renders at ~60 Hz
	// gated on elapsed time so the rate holds. The wait is an FEvent timeout in MILLISECONDS:
	// FPlatformProcess::SleepNoStats(seconds) was observed sleeping ~1000x too long on PS5,
	// stalling the worker to ~0.1 Hz. Tune kRenderIntervalSeconds if the feel needs it.
	constexpr uint32 kQueueWaitMs           = 8;
	constexpr double kRenderIntervalSeconds = 1.0 / 60.0;
}

/**
 * Owns the HAR runtime + voice pool and runs them on its own thread. The game thread
 * only enqueues event names; this thread drains them and renders, so HAR is only ever
 * touched here — single-threaded, hence no locking. Init/Run/Exit all run on the worker.
 */
class FWyvrnHapticRenderer : public FRunnable
{
public:
	FWyvrnHapticRenderer(FWyvrnRuntimeData&& InData, bool bInHapticsEnabled, int32 InGain0To100, bool bInTriggersEnabled)
		: Data(MoveTemp(InData))
		, PendingVibrationGain(InGain0To100)
		, bPendingHapticsEnabled(bInHapticsEnabled)
		, bPendingTriggersEnabled(bInTriggersEnabled)
	{
		// Created on the game thread so Stop() can wake the loop for a prompt exit.
		WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
	}

	virtual ~FWyvrnHapticRenderer() override
	{
		if (WakeEvent != nullptr)
		{
			FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
			WakeEvent = nullptr;
		}
	}

	/** Game thread: hand an event to the worker (lock-free). Dropped until the worker is ready. */
	void Enqueue(const FString& EventName)
	{
		if (bReady.load(std::memory_order_acquire))
		{
			EventQueue.Enqueue(EventName);
			WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Enqueue]: '%s' queued for render worker (game thread)."), *EventName);
		}
		else
		{
			WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Enqueue]: '%s' DROPPED — render worker not ready yet."), *EventName);
		}
	}

	/**
	 * Game thread: hand the three haptic controls to the worker (lock-free). Each is
	 * last-write-wins, so dragging a settings slider coalesces instead of queueing,
	 * and each is applied independently and idempotently - a torn read across the
	 * three just converges on the next tick. Unlike Enqueue these are NOT dropped
	 * before the worker is ready: Init() applies whatever is pending once HAR is up.
	 */
	void SetPendingHapticsEnabled(bool bEnabled)
	{
		bPendingHapticsEnabled.store(bEnabled, std::memory_order_relaxed);
		Wake();
	}

	void SetPendingVibrationGain(int32 Gain0To100)
	{
		PendingVibrationGain.store(Gain0To100, std::memory_order_relaxed);
		Wake();
	}

	void SetPendingTriggersEnabled(bool bEnabled)
	{
		bPendingTriggersEnabled.store(bEnabled, std::memory_order_relaxed);
		Wake();
	}

	bool IsReady() const { return bReady.load(std::memory_order_acquire); }

	virtual bool Init() override
	{
		Runtime = MakeUnique<FInterhapticsRuntime>();
		if (!Runtime->Initialize())
		{
			// HAR PRX absent or failed to load: stay inert (Run returns immediately).
			Runtime.Reset();
			UE_LOG(LogWyvrnHaptics, Log, TEXT("WyvrnSDK: Interhaptics runtime unavailable; haptics inert."));
			return true;
		}

		Pool = MakeUnique<FHapticVoicePool>(*Runtime, kVoicesPerEffect);
		UE_LOG(LogWyvrnHaptics, Log, TEXT("WyvrnSDK: preloading %d command(s)."), Data.Commands.Num());
		Pool->Preload(Data);

		// Re-assert the controls: HAR's Quit() destroys the manager holding the global
		// intensity and Init() builds a fresh one back at 1.0, so without this the
		// vibration gain silently reverts to full on every backend bring-up.
		ApplyPendingSettings();

		LastSeconds = FPlatformTime::Seconds();
		bReady.store(true, std::memory_order_release);
		UE_LOG(LogWyvrnHaptics, Log, TEXT("WyvrnSDK: Interhaptics render worker initialized."));
		return true;
	}

	virtual uint32 Run() override
	{
		if (!Runtime.IsValid() || !Pool.IsValid())
		{
			return 0; // inert: nothing loaded
		}

		double RenderAccumulator = 0.0;

		while (!bStop.load(std::memory_order_acquire))
		{
			// FPlatformTime::Seconds() returns real seconds directly. (Cycles64() * GetSecondsPerCycle()
			// is wrong by ~1000x on PS5 — Cycles64 ticks far faster than GetSecondsPerCycle assumes —
			// which raced TimeSeconds ahead and fed HAR garbage time.)
			const double Now = FPlatformTime::Seconds();
			const double Delta = Now - LastSeconds;
			LastSeconds = Now;
			TimeSeconds += Delta;
			RenderAccumulator += Delta;

			// Pick up control changes from the game thread before anything renders.
			ApplyPendingSettings();

			// Drain queued game-thread triggers every tick (~120 Hz) so events dispatch promptly.
			FString EventName;
			while (EventQueue.Dequeue(EventName))
			{
				WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Worker]: received '%s' from queue (worker thread)."), *EventName);
				if (!bAppliedHapticsEnabled)
				{
					// The backend already refuses to enqueue while haptics are off; this
					// catches events that were in flight when they were switched off.
					WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Worker]: '%s' SKIPPED — haptics are off."), *EventName);
					continue;
				}
				if (const FWyvrnRuntimeCommand* Command = Data.FindCommand(EventName))
				{
					WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Worker]: '%s' matched a command (%d effect(s)); dispatching at t=%.3fs."),
						*EventName, Command->Effects.Num(), TimeSeconds);
					Pool->PlayCommand(*Command, TimeSeconds);
				}
				else
				{
					WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Worker]: '%s' matched NO command — unknown event, ignored."), *EventName);
				}
			}

			// Reclaim / arbitrate / render at HAR's native frame rate, time-gated so the rate
			// holds despite sleep jitter.
			if (RenderAccumulator >= kRenderIntervalSeconds)
			{
				RenderAccumulator -= kRenderIntervalSeconds;
				if (RenderAccumulator > kRenderIntervalSeconds)
				{
					RenderAccumulator = 0.0; // drop backlog after a hitch rather than burst-render
				}

				SCOPE_CYCLE_COUNTER(STAT_WyvrnHaptics_RenderLoop);
				Pool->Tick(TimeSeconds);
				SET_DWORD_STAT(STAT_WyvrnHaptics_ActiveVoices, Pool->GetActiveVoiceCount());
				Runtime->Render(TimeSeconds);
			}

			// Timed wait in milliseconds — reliable on PS5, unlike SleepNoStats(seconds).
			WakeEvent->Wait(kQueueWaitMs);
		}
		return 0;
	}

	virtual void Stop() override
	{
		bStop.store(true, std::memory_order_release);
		if (WakeEvent != nullptr)
		{
			WakeEvent->Trigger(); // wake the loop so it exits promptly
		}
	}

	virtual void Exit() override
	{
		// Worker thread: tear HAR down here so it is single-threaded end to end.
		// Reset the pool before the runtime: the pool holds a reference to the runtime.
		bReady.store(false, std::memory_order_release);
		Pool.Reset();
		if (Runtime.IsValid())
		{
			Runtime->Shutdown();
			Runtime.Reset();
		}
	}

private:
	void Wake()
	{
		if (WakeEvent != nullptr)
		{
			WakeEvent->Trigger(); // apply now rather than at the end of the ~8 ms wait
		}
	}

	/** Worker thread only: push any changed control into HAR, skipping redundant calls. */
	void ApplyPendingSettings()
	{
		if (!Runtime.IsValid())
		{
			return;
		}

		const int32 WantedGain = PendingVibrationGain.load(std::memory_order_relaxed);
		if (WantedGain != AppliedVibrationGain)
		{
			AppliedVibrationGain = WantedGain;
			const float Scalar = WyvrnVibrationGainToScalar(WantedGain);
			Runtime->SetGlobalIntensity(Scalar);
			WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Worker]: vibration gain %d/100 (factor %.2f)."), WantedGain, Scalar);
		}

		if (!Pool.IsValid())
		{
			return;
		}

		// Triggers before the master switch: if both changed in the same tick, an
		// enable followed by a switch-off still ends with the pad released.
		const bool bWantedTriggers = bPendingTriggersEnabled.load(std::memory_order_relaxed);
		if (bWantedTriggers != bAppliedTriggersEnabled)
		{
			bAppliedTriggersEnabled = bWantedTriggers;
			Pool->SetAdaptiveTriggersEnabled(bWantedTriggers);
		}

		const bool bWantedEnabled = bPendingHapticsEnabled.load(std::memory_order_relaxed);
		if (bWantedEnabled != bAppliedHapticsEnabled)
		{
			bAppliedHapticsEnabled = bWantedEnabled;
			WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Worker]: haptics %s."), bWantedEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
			if (!bWantedEnabled)
			{
				// Off is a hard off: stop every voice and release both triggers. New
				// events stay skipped until haptics are switched back on.
				Pool->StopAll();
			}
		}
	}

	FWyvrnRuntimeData Data;
	TQueue<FString, EQueueMode::Mpsc> EventQueue;
	FEvent* WakeEvent = nullptr;
	TUniquePtr<FInterhapticsRuntime> Runtime;
	TUniquePtr<FHapticVoicePool> Pool;
	std::atomic<bool> bReady{ false };
	std::atomic<bool> bStop{ false };
	// Written by the game thread, consumed by the worker; last write wins.
	std::atomic<int32> PendingVibrationGain{ 100 };
	std::atomic<bool> bPendingHapticsEnabled{ true };
	std::atomic<bool> bPendingTriggersEnabled{ true };
	// Worker only: what has actually been pushed. -1 forces the first gain apply
	// through (HAR comes up at 1.0 and must be re-asserted); the bools start at the
	// default-on state, which is exactly the state a freshly initialised HAR is in.
	int32 AppliedVibrationGain = -1;
	bool bAppliedHapticsEnabled = true;
	bool bAppliedTriggersEnabled = true;
	double TimeSeconds = 0.0;
	double LastSeconds = 0.0;
};

FInterhapticsHapticBackend::FInterhapticsHapticBackend() = default;

FInterhapticsHapticBackend::~FInterhapticsHapticBackend()
{
	Shutdown();
}

bool FInterhapticsHapticBackend::Initialize()
{
	if (Thread != nullptr)
	{
		return true;
	}

	// Snapshot the baked data on the GAME THREAD (UObject access is not thread-safe);
	// the worker then runs purely on the self-contained POD copy.
	FWyvrnRuntimeData RuntimeData;
	if (UWyvrnHapticData* DataAsset = LoadObject<UWyvrnHapticData>(nullptr, kDataAssetPath))
	{
		RuntimeData = FWyvrnRuntimeData::Build(*DataAsset);
	}
	else
	{
		UE_LOG(LogWyvrnHaptics, Warning, TEXT("WyvrnSDK: haptic data '%s' not found; no events will play."), kDataAssetPath);
	}

	// Seed the worker from the cached controls so values set before InitSDK still apply.
	Renderer = MakeUnique<FWyvrnHapticRenderer>(MoveTemp(RuntimeData), bHapticsEnabled, VibrationGain, bAdaptiveTriggersEnabled);
	Thread = FRunnableThread::Create(Renderer.Get(), TEXT("WyvrnHapticRenderer"), 0, TPri_AboveNormal);
	if (Thread == nullptr)
	{
		Renderer.Reset();
		UE_LOG(LogWyvrnHaptics, Warning, TEXT("WyvrnSDK: failed to start the haptic render worker."));
		return false;
	}

	UE_LOG(LogWyvrnHaptics, Log, TEXT("WyvrnSDK: Interhaptics haptic backend started."));
	return true;
}

void FInterhapticsHapticBackend::Shutdown()
{
	if (Thread != nullptr)
	{
		// Kill(true): Stop() -> wait for Run() to exit -> Exit() tears down HAR on the
		// worker -> join. So HAR teardown stays on the worker thread.
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}
	Renderer.Reset();
}

bool FInterhapticsHapticBackend::IsInitialized() const
{
	return Renderer.IsValid() && Renderer->IsReady();
}

void FInterhapticsHapticBackend::SetEventName(const FString& EventName)
{
	SCOPE_CYCLE_COUNTER(STAT_WyvrnHaptics_SetEvent);

	if (!bHapticsEnabled)
	{
		// Switched off skips the event outright rather than playing it silently: no
		// queue traffic, no voice churn, no HAR synthesis for something nobody can
		// feel. Note this is the master switch only - a vibration gain of 0 still
		// plays the event, because it may carry a Stiffness track that arms a trigger.
		WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Backend]: SetEventName('%s') SKIPPED — haptics are off."), *EventName);
		return;
	}

	WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Backend]: SetEventName('%s') -> forwarding to render worker."), *EventName);

	if (Renderer.IsValid())
	{
		Renderer->Enqueue(EventName);
	}
	else
	{
		WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Backend]: SetEventName('%s') DROPPED — no render worker."), *EventName);
	}
}

void FInterhapticsHapticBackend::SetHapticsEnabled(bool bEnabled)
{
	bHapticsEnabled = bEnabled;

	WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Backend]: haptics -> %s%s."),
		bEnabled ? TEXT("ON") : TEXT("OFF"), Renderer.IsValid() ? TEXT("") : TEXT(" (cached; no render worker yet)"));

	if (Renderer.IsValid())
	{
		Renderer->SetPendingHapticsEnabled(bEnabled);
	}
}

bool FInterhapticsHapticBackend::AreHapticsEnabled() const
{
	return bHapticsEnabled;
}

void FInterhapticsHapticBackend::SetVibrationGain(int32 Gain0To100)
{
	// Clamped here as well as at the Blueprint boundary, so the cache always holds the
	// value that will actually be applied whichever way the backend was reached.
	VibrationGain = WyvrnClampVibrationGain(Gain0To100);

	WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Backend]: vibration gain -> %d/100%s."),
		VibrationGain, Renderer.IsValid() ? TEXT("") : TEXT(" (cached; no render worker yet)"));

	if (Renderer.IsValid())
	{
		Renderer->SetPendingVibrationGain(VibrationGain);
	}
}

int32 FInterhapticsHapticBackend::GetVibrationGain() const
{
	return VibrationGain;
}

void FInterhapticsHapticBackend::SetAdaptiveTriggersEnabled(bool bEnabled)
{
	bAdaptiveTriggersEnabled = bEnabled;

	WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [Backend]: adaptive triggers -> %s%s."),
		bEnabled ? TEXT("ON") : TEXT("OFF"), Renderer.IsValid() ? TEXT("") : TEXT(" (cached; no render worker yet)"));

	if (Renderer.IsValid())
	{
		Renderer->SetPendingTriggersEnabled(bEnabled);
	}
}

bool FInterhapticsHapticBackend::AreAdaptiveTriggersEnabled() const
{
	return bAdaptiveTriggersEnabled;
}

#endif // PLATFORM_PS5
