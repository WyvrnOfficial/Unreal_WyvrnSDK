// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_PS5

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
	virtual void SetLoop(int32 MaterialId, int32 NumLoops) override;
	virtual void AddTarget(int32 MaterialId, EWyvrnHapticTarget Target) override;
	virtual void Play(int32 MaterialId, double TimeSeconds) override;
	virtual void Stop(int32 MaterialId) override;
	virtual void StopAll() override;
	virtual double GetLength(int32 MaterialId) const override;
	virtual void Render(double TimeSeconds) override;

private:
	bool bAvailable = false;
};

#endif // PLATFORM_PS5
