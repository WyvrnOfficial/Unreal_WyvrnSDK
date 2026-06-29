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
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "UObject/UObjectGlobals.h"

#include <atomic>

DEFINE_LOG_CATEGORY(LogWyvrnHaptics);

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
	explicit FWyvrnHapticRenderer(FWyvrnRuntimeData&& InData)
		: Data(MoveTemp(InData))
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
		}
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

			// Drain queued game-thread triggers every tick (~120 Hz) so events dispatch promptly.
			FString EventName;
			while (EventQueue.Dequeue(EventName))
			{
				if (const FWyvrnRuntimeCommand* Command = Data.FindCommand(EventName))
				{
					Pool->PlayCommand(*Command, TimeSeconds);
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
	FWyvrnRuntimeData Data;
	TQueue<FString, EQueueMode::Mpsc> EventQueue;
	FEvent* WakeEvent = nullptr;
	TUniquePtr<FInterhapticsRuntime> Runtime;
	TUniquePtr<FHapticVoicePool> Pool;
	std::atomic<bool> bReady{ false };
	std::atomic<bool> bStop{ false };
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

	Renderer = MakeUnique<FWyvrnHapticRenderer>(MoveTemp(RuntimeData));
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

	if (Renderer.IsValid())
	{
		Renderer->Enqueue(EventName);
	}
}

#endif // PLATFORM_PS5
