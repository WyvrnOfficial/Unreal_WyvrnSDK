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

	// Three independent haptic controls. All are accepted at any time - including
	// before Initialize() - and re-applied to the platform runtime on every bring-up,
	// so they survive an Initialize()/Shutdown() cycle.

	/**
	 * Master switch. Off stops anything playing, releases both adaptive triggers, and
	 * skips further events outright rather than playing them inaudibly. Turning it
	 * back on does not resume what was stopped.
	 */
	virtual void SetHapticsEnabled(bool bEnabled) = 0;
	virtual bool AreHapticsEnabled() const = 0;

	/**
	 * Vibration strength, 0..100 (0 = silent, 100 = unattenuated), clamped by the
	 * implementation. Affects vibration only: 0 does not stop events or triggers.
	 */
	virtual void SetVibrationGain(int32 Gain0To100) = 0;
	/** The gain last accepted, already clamped to 0..100. 100 until set. */
	virtual int32 GetVibrationGain() const = 0;

	/**
	 * DualSense adaptive-trigger resistance on/off, independent of vibration. Off
	 * releases whatever is armed but leaves events playing and vibrating; on re-arms
	 * whatever is still claimed. Resistance is always at the authored strength - the
	 * vibration gain never scales it.
	 */
	virtual void SetAdaptiveTriggersEnabled(bool bEnabled) = 0;
	virtual bool AreAdaptiveTriggersEnabled() const = 0;
};

#if defined(PLATFORM_PS5) && PLATFORM_PS5
/**
 * Returns the PS5 haptic backend owned by the WyvrnSDKPlugin module, or null if
 * the module is not loaded. Defined in WyvrnSDKPlugin.cpp.
 */
IWyvrnHapticBackend* GetWyvrnHapticBackend();
#endif
