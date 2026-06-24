// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "InterhapticsHapticBackend.h"

#if defined(PLATFORM_PS5) && PLATFORM_PS5

#include "WyvrnHapticData.h"
#include "WyvrnHapticTypes.h"
#include "WyvrnHapticsLog.h"

#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY(LogWyvrnHaptics);

namespace
{
	// HAR material ids loaded per effect, giving polyphony and voice stealing.
	constexpr int32 kVoicesPerEffect = 4;

	// Baked event map produced by the editor importer (OutputContentPath default /Game/Wyvrn).
	const TCHAR* const kDataAssetPath = TEXT("/Game/Wyvrn/WyvrnHapticData.WyvrnHapticData");
}

FInterhapticsHapticBackend::FInterhapticsHapticBackend() = default;

FInterhapticsHapticBackend::~FInterhapticsHapticBackend()
{
	Shutdown();
}

bool FInterhapticsHapticBackend::Initialize()
{
	if (bInitialized)
	{
		return true;
	}

	Runtime = MakeUnique<FInterhapticsRuntime>();
	if (!Runtime->Initialize())
	{
		// HAR PRX absent or failed to load: stay inert (no data load, no ticker, no playback).
		Runtime.Reset();
		UE_LOG(LogWyvrnHaptics, Log, TEXT("WyvrnSDK: Interhaptics runtime unavailable; haptics inert."));
		return false;
	}

	// Load the baked event map. Missing data is non-fatal: the backend runs but plays nothing.
	Data.Reset(LoadObject<UWyvrnHapticData>(nullptr, kDataAssetPath));
	if (!Data.IsValid())
	{
		UE_LOG(LogWyvrnHaptics, Warning, TEXT("WyvrnSDK: haptic data '%s' not found; no events will play."), kDataAssetPath);
	}

	Pool = MakeUnique<FHapticVoicePool>(*Runtime, kVoicesPerEffect);
	if (Data.IsValid())
	{
		UE_LOG(LogWyvrnHaptics, Log, TEXT("WyvrnSDK: loaded haptic data with %d command(s); preloading effects."), Data->Commands.Num());
		Pool->Preload(*Data);
	}

	CurrentTimeSeconds = 0.0;
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FInterhapticsHapticBackend::Tick));

	bInitialized = true;
	UE_LOG(LogWyvrnHaptics, Log, TEXT("WyvrnSDK: Interhaptics haptic backend initialized."));
	return true;
}

void FInterhapticsHapticBackend::Shutdown()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	// Reset the pool before the runtime: the pool holds a reference to the runtime.
	Pool.Reset();

	if (Runtime.IsValid())
	{
		Runtime->Shutdown();
		Runtime.Reset();
	}

	Data.Reset();
	CurrentTimeSeconds = 0.0;
	bInitialized = false;
}

bool FInterhapticsHapticBackend::IsInitialized() const
{
	return bInitialized;
}

void FInterhapticsHapticBackend::SetEventName(const FString& EventName)
{
	if (!bInitialized || !Data.IsValid() || !Pool.IsValid())
	{
		return;
	}

	if (const FWyvrnHapticCommand* Command = Data->FindCommand(EventName))
	{
		Pool->PlayCommand(*Command, CurrentTimeSeconds);
	}
}

bool FInterhapticsHapticBackend::Tick(float DeltaTime)
{
	CurrentTimeSeconds += DeltaTime;
	// Reclaim finished voices and re-arbitrate BEFORE rendering, so a dominant event
	// ending this frame un-ducks the voice it was masking in the same frame (no 1-frame gap).
	if (Pool.IsValid())
	{
		Pool->Tick(CurrentTimeSeconds);
	}
	if (Runtime.IsValid())
	{
		Runtime->Render(CurrentTimeSeconds);
	}
	return true;
}

#endif // PLATFORM_PS5
