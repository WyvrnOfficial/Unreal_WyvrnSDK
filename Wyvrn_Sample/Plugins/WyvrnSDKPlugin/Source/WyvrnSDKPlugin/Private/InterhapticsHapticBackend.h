// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_PS5

#include "IWyvrnHapticBackend.h"
#include "HapticVoicePool.h"
#include "InterhapticsRuntime.h"
#include "Containers/Ticker.h"
#include "UObject/StrongObjectPtr.h"

class UWyvrnHapticData;

/**
 * PS5 IWyvrnHapticBackend. Owns the Interhaptics HAR runtime and a haptic voice
 * pool, loads the baked UWyvrnHapticData event map, and drives HAR rendering
 * from a per-frame ticker. SetEventName resolves the WYVRN command and plays it
 * through the pool.
 *
 * When the HAR PRX are absent the runtime fails to initialize and the backend
 * stays inert: Initialize() returns false, no ticker is registered, and
 * SetEventName is a no-op.
 */
class FInterhapticsHapticBackend final : public IWyvrnHapticBackend
{
public:
	FInterhapticsHapticBackend();
	virtual ~FInterhapticsHapticBackend() override;

	// IWyvrnHapticBackend
	virtual bool Initialize() override;
	virtual void Shutdown() override;
	virtual bool IsInitialized() const override;
	virtual void SetEventName(const FString& EventName) override;

private:
	bool Tick(float DeltaTime);

	TUniquePtr<FInterhapticsRuntime> Runtime;
	TUniquePtr<FHapticVoicePool> Pool;
	TStrongObjectPtr<UWyvrnHapticData> Data;
	FTSTicker::FDelegateHandle TickerHandle;

	// Monotonic clock fed to HAR ComputeAllEvents / the pool; advanced by the ticker.
	double CurrentTimeSeconds = 0.0;
	bool bInitialized = false;
};

#endif // PLATFORM_PS5
