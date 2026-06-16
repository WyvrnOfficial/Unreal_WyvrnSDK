// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_PS5

#include "IInterhapticsRuntime.h"

/**
 * PS5 concrete IInterhapticsRuntime. Binds the Interhaptics HAR engine (HAR.prx)
 * and the DualSense provider (DualSenseProvider.prx) by loading both PRX modules
 * at runtime and resolving their extern "C" entry points.
 *
 * When the modules are absent (or any entry point fails to resolve) the runtime
 * stays inert: Initialize() returns false, IsAvailable() returns false, and every
 * call is a no-op. This lets the title ship and run without the HAR PRX present.
 */
class FInterhapticsRuntime final : public IInterhapticsRuntime
{
public:
	FInterhapticsRuntime();
	virtual ~FInterhapticsRuntime();

	// IInterhapticsRuntime
	virtual bool Initialize() override;
	virtual void Shutdown() override;
	virtual bool IsAvailable() const override;
	virtual int32 AddMaterial(const FString& MaterialJson) override;
	virtual void SetIntensity(int32 MaterialId, float Intensity) override;
	virtual void SetLoop(int32 MaterialId, int32 NumLoops) override;
	virtual void AddTarget(int32 MaterialId, EWyvrnHapticTarget Target) override;
	virtual void Play(int32 MaterialId) override;
	virtual void Stop(int32 MaterialId) override;
	virtual void StopAll() override;
	virtual double GetLength(int32 MaterialId) const override;
	virtual void Render(double TimeSeconds) override;

private:
	// Holds the PS5 module handles and resolved function pointers, keeping the
	// PS5 SDK headers out of this header.
	struct FImpl;
	TUniquePtr<FImpl> Impl;

	bool bAvailable = false;
};

#endif // PLATFORM_PS5
