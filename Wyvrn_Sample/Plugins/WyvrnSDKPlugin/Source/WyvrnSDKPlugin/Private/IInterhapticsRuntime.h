// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WyvrnHapticTypes.h"

/**
 * Abstraction over the Interhaptics HAR runtime. The real implementation binds
 * HAR.prx / DualSenseProvider.prx on PS5; tests substitute a mock. Every event
 * is keyed by the material id returned from AddMaterial (HAR's _hMaterialID).
 */
class IInterhapticsRuntime
{
public:
	virtual ~IInterhapticsRuntime() = default;

	/** HAR Init() + ProviderInit(). Returns false when the runtime is unavailable. */
	virtual bool Initialize() = 0;
	virtual void Shutdown() = 0;
	virtual bool IsAvailable() const = 0;

	/** AddHM: loads a material from its JSON, returning its id or a negative value on failure. */
	virtual int32 AddMaterial(const FString& MaterialJson) = 0;

	/** SetEventIntensity. */
	virtual void SetIntensity(int32 MaterialId, float Intensity) = 0;
	/** SetEventLoop: 0/1 = one shot, < 0 = infinite. */
	virtual void SetLoop(int32 MaterialId, int32 NumLoops) = 0;
	/** AddTargetToEvent. The implementation maps the region + lateral side to provider endpoints. */
	virtual void AddTarget(int32 MaterialId, EWyvrnHapticTarget Region, EWyvrnHapticSide Side) = 0;
	/**
	 * Plays the event anchored to TimeSeconds (the same clock fed to Render()).
	 * Restarts the event if it is already playing. HAR renders an event at
	 * (current time - play offset), so the implementation passes -TimeSeconds as the
	 * offset; otherwise the effect renders at the absolute engine time, past its end.
	 */
	virtual void Play(int32 MaterialId, double TimeSeconds) = 0;
	/** StopEvent. */
	virtual void Stop(int32 MaterialId) = 0;
	/** StopAllEvents. */
	virtual void StopAll() = 0;
	/** GetVibrationLength, in seconds. */
	virtual double GetLength(int32 MaterialId) const = 0;

	/** ComputeAllEvents(Time) followed by ProviderRenderHaptics(). Call once per frame. */
	virtual void Render(double TimeSeconds) = 0;
};
