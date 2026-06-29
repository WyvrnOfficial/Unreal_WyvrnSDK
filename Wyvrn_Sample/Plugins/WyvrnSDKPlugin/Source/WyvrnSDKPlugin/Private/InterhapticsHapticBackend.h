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

private:
	TUniquePtr<FWyvrnHapticRenderer> Renderer;
	FRunnableThread* Thread = nullptr;
};

#endif // PLATFORM_PS5
