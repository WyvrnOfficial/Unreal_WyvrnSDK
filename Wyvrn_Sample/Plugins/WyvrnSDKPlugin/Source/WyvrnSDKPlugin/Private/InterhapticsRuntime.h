// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if defined(PLATFORM_PS5) && PLATFORM_PS5

#include "IInterhapticsRuntime.h"

/**
 * PS5 concrete IInterhapticsRuntime. Calls the Interhaptics HAR engine (HAR.prx)
 * and the DualSense provider (Provider_DualSensePS5.prx) directly; their entry
 * points are bound through the *_stub_weak.a import libraries, and the loader
 * resolves the PRX from /app0/sce_module/.
 *
 * Inert when the PRX are absent (weak import => the title still launches) or when
 * the plugin was built without the stubs (WITH_INTERHAPTICS_HAR == 0): Initialize()
 * returns false, IsAvailable() returns false, and every call is a no-op.
 */
class FInterhapticsRuntime final : public IInterhapticsRuntime
{
public:
	FInterhapticsRuntime();
	virtual ~FInterhapticsRuntime() override;

	// IInterhapticsRuntime
	virtual bool Initialize() override;
	virtual void Shutdown() override;
	virtual bool IsAvailable() const override;
	virtual int32 AddMaterial(const FString& MaterialJson) override;
	virtual void SetIntensity(int32 MaterialId, float Intensity) override;
	virtual void SetGlobalIntensity(float Intensity) override;
	virtual void SetLoop(int32 MaterialId, int32 NumLoops) override;
	virtual void AddTarget(int32 MaterialId, EWyvrnHapticTarget Region, EWyvrnHapticSide Side) override;
	virtual void Play(int32 MaterialId, double TimeSeconds) override;
	virtual void Stop(int32 MaterialId) override;
	virtual void StopAll() override;
	virtual double GetLength(int32 MaterialId) const override;
	virtual void Render(double TimeSeconds) override;
	virtual bool StartTriggerEffect(int32 MaterialId, bool bLeftTrigger) override;
	virtual void StopTriggerEffect(bool bLeftTrigger) override;

private:
	// Adaptive-trigger provider exports, resolved with GetDllExport at Initialize
	// rather than through the weak stubs so the checked-in *_stub_weak.a import
	// libraries keep working whether or not their provider vintage exported these.
	// Null (=> no-op) when the loaded PRX predates the exports.
	typedef int (*FStartTriggerEffectFn)(int Id, bool bIsLeft);
	typedef int (*FStopTriggerEffectFn)(bool bIsLeft);
	FStartTriggerEffectFn StartTriggerEffectFn = nullptr;
	FStopTriggerEffectFn StopTriggerEffectFn = nullptr;

	/**
	 * Last value handed to SetGlobalIntensity. Remembered because HAR applies the
	 * global intensity to the Stiffness envelope as well as to vibration, and the
	 * general haptics gain is deliberately vibration-only: StartTriggerEffect
	 * neutralises the intensity for the duration of the arm and restores it from here.
	 */
	float GlobalIntensity = 1.0f;

	bool bAvailable = false;
};

#endif // PLATFORM_PS5
