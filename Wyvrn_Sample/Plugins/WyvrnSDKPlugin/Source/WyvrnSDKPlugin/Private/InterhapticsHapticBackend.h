// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if defined(PLATFORM_PS5) && PLATFORM_PS5

#include "IWyvrnHapticBackend.h"

class FWyvrnHapticRenderer;
class FRunnableThread;

/**
 * PS5 IWyvrnHapticBackend.
 *
 * The HAR synthesis is expensive and must not run on the game thread, so all HAR
 * access lives on a dedicated render worker (FWyvrnHapticRenderer). The game thread
 * only snapshots the baked data (a UObject read, done once at Initialize), starts
 * and stops the worker, and enqueues event names. HAR is therefore touched by
 * exactly one thread and needs no internal locking.
 *
 * Inert when the baked data is missing or the HAR PRX fail to load: the worker
 * comes up not-ready, IsInitialized() returns false, and SetEventName is dropped.
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
	virtual void SetHapticsEnabled(bool bEnabled) override;
	virtual bool AreHapticsEnabled() const override;
	virtual void SetVibrationGain(int32 Gain0To100) override;
	virtual int32 GetVibrationGain() const override;
	virtual void SetAdaptiveTriggersEnabled(bool bEnabled) override;
	virtual bool AreAdaptiveTriggersEnabled() const override;

private:
	TUniquePtr<FWyvrnHapticRenderer> Renderer;
	FRunnableThread* Thread = nullptr;

	// Authoritative copies of the three controls, game thread only (like Renderer /
	// Thread). They live on the backend rather than the worker because the backend
	// spans the whole module lifetime while the worker is destroyed and rebuilt by
	// every Shutdown()/Initialize() pair - and HAR resets its own global intensity to
	// 1.0 on each Init() - so they must be held somewhere that outlives both and
	// re-asserted on bring-up.
	bool bHapticsEnabled = true;
	int32 VibrationGain = 100;
	bool bAdaptiveTriggersEnabled = true;
};

#endif // PLATFORM_PS5
