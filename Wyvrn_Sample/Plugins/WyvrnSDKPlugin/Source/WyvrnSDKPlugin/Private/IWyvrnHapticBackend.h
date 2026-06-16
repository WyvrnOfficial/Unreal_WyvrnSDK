// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Platform haptic backend selected by WyvrnAPI. Windows routes events to the
 * RzChromatic DLL (Synapse-hosted); PS5 routes them to the Interhaptics HAR
 * runtime on the DualSense.
 */
class IWyvrnHapticBackend
{
public:
	virtual ~IWyvrnHapticBackend() = default;

	/**
	 * Brings the backend up. Returns false when the platform runtime is
	 * unavailable (e.g. the PRX libraries are missing), in which case the SDK
	 * behaves as a no-op.
	 */
	virtual bool Initialize() = 0;
	virtual void Shutdown() = 0;
	virtual bool IsInitialized() const = 0;

	/** Fires a named WYVRN event (External_Command_ID). */
	virtual void SetEventName(const FString& EventName) = 0;
};

#if defined(PLATFORM_PS5) && PLATFORM_PS5
/**
 * Returns the PS5 haptic backend owned by the WyvrnSDKPlugin module, or null if
 * the module is not loaded. Defined in WyvrnSDKPlugin.cpp.
 */
IWyvrnHapticBackend* GetWyvrnHapticBackend();
#endif
