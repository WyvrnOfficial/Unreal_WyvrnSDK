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

	// Render cadence of the worker (~125 Hz). Decoupled from the game frame rate.
	constexpr float kRenderIntervalSeconds = 0.008f;
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

		LastCycles = FPlatformTime::Cycles64();
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

		while (!bStop.load(std::memory_order_acquire))
		{
			{
				SCOPE_CYCLE_COUNTER(STAT_WyvrnHaptics_RenderLoop);

				const uint64 Now = FPlatformTime::Cycles64();
				TimeSeconds += (Now - LastCycles) * FPlatformTime::GetSecondsPerCycle();
				LastCycles = Now;

				// Apply queued game-thread triggers, then reclaim / arbitrate / render.
				FString EventName;
				while (EventQueue.Dequeue(EventName))
				{
					if (const FWyvrnRuntimeCommand* Command = Data.FindCommand(EventName))
					{
						Pool->PlayCommand(*Command, TimeSeconds);
					}
				}

				Pool->Tick(TimeSeconds);
				SET_DWORD_STAT(STAT_WyvrnHaptics_ActiveVoices, Pool->GetActiveVoiceCount());
				Runtime->Render(TimeSeconds);
			}

			FPlatformProcess::SleepNoStats(kRenderIntervalSeconds);
		}
		return 0;
	}

	virtual void Stop() override { bStop.store(true, std::memory_order_release); }

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
	TUniquePtr<FInterhapticsRuntime> Runtime;
	TUniquePtr<FHapticVoicePool> Pool;
	std::atomic<bool> bReady{ false };
	std::atomic<bool> bStop{ false };
	double TimeSeconds = 0.0;
	uint64 LastCycles = 0;
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
