// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "InterhapticsRuntime.h"

#if defined(PLATFORM_PS5) && PLATFORM_PS5

#include "InterhapticsHARTypes.h"
#include "WyvrnHapticTypes.h"
#include "WyvrnHapticsLog.h"
#include "WyvrnHapticsStats.h"

// Defined by Build.cs: 1 when the HAR + provider import stubs are linked, 0 otherwise.
#ifndef WITH_INTERHAPTICS_HAR
#define WITH_INTERHAPTICS_HAR 0
#endif

#if WITH_INTERHAPTICS_HAR

namespace
{
	using WyvrnSDK::HAR::FCommandData;
	using WyvrnSDK::HAR::EOperator;
	using WyvrnSDK::HAR::EGroupID;
	using WyvrnSDK::HAR::ELateralFlag;
}

// HAR engine (HAR.prx) + DualSense provider (Provider_DualSensePS5.prx) entry points.
// extern "C", resolved at link time against the *_stub_weak.a import libraries.
// Declared locally so the third-party SDK headers are not vendored; the signatures
// mirror InterhapticsEngine[Internal].h and InterhapticsProvider_DualSensePS5.h.
extern "C"
{
	bool   Init();
	void   Quit();
	int    AddHM(const char* Content);
	void   PlayEvent(int MaterialId, double VibrationOffset, double TextureOffset, double StiffnessOffset);
	void   StopEvent(int MaterialId);
	void   StopAllEvents();
	void   AddTargetToEventMarshal(int MaterialId, FCommandData* Targets, int Size);
	void   ComputeAllEvents(double CurrentTime);
	void   SetEventIntensity(int MaterialId, double Intensity);
	void   SetEventLoop(int MaterialId, int NumLoops);
	double GetVibrationLength(int MaterialId);

	bool ProviderInit();
	bool ProviderClean();
	void ProviderRenderHaptics();
}

#endif // WITH_INTERHAPTICS_HAR

DECLARE_CYCLE_STAT(TEXT("HAR Render"), STAT_WyvrnHaptics_Render, STATGROUP_WyvrnHaptics);

FInterhapticsRuntime::FInterhapticsRuntime() = default;

FInterhapticsRuntime::~FInterhapticsRuntime()
{
	Shutdown();
}

bool FInterhapticsRuntime::Initialize()
{
	if (bAvailable)
	{
		return true;
	}

#if WITH_INTERHAPTICS_HAR
	// Load the delay-loaded PRX through UE's module loader (it resolves where they staged).
	// A null handle means the PRX is absent, so the runtime stays inert; a valid handle binds
	// the delay-load imports, making the direct calls below safe.
	void* const HarHandle = FPlatformProcess::GetDllHandle(TEXT("HAR.prx"));
	UE_LOG(LogWyvrnHaptics, Log, TEXT("Interhaptics: GetDllHandle('HAR.prx') -> %p"), HarHandle);
	if (HarHandle == nullptr)
	{
		UE_LOG(LogWyvrnHaptics, Warning, TEXT("Interhaptics: HAR.prx not loadable; haptics inert."));
		return false;
	}

	void* const ProviderHandle = FPlatformProcess::GetDllHandle(TEXT("Provider_DualSensePS5.prx"));
	UE_LOG(LogWyvrnHaptics, Log, TEXT("Interhaptics: GetDllHandle('Provider_DualSensePS5.prx') -> %p"), ProviderHandle);
	if (ProviderHandle == nullptr)
	{
		UE_LOG(LogWyvrnHaptics, Warning, TEXT("Interhaptics: Provider_DualSensePS5.prx not loadable; haptics inert."));
		return false;
	}

	// The adaptive-trigger exports are newer than the rest of the provider surface;
	// resolve them dynamically so an older PRX degrades to no-op triggers instead of
	// failing the whole runtime.
	StartTriggerEffectFn = reinterpret_cast<FStartTriggerEffectFn>(FPlatformProcess::GetDllExport(ProviderHandle, TEXT("startTriggerEffect")));
	StopTriggerEffectFn = reinterpret_cast<FStopTriggerEffectFn>(FPlatformProcess::GetDllExport(ProviderHandle, TEXT("stopTriggerEffect")));
	UE_LOG(LogWyvrnHaptics, Log, TEXT("Interhaptics: adaptive trigger exports start=%p stop=%p%s"),
		reinterpret_cast<void*>(StartTriggerEffectFn), reinterpret_cast<void*>(StopTriggerEffectFn),
		(StartTriggerEffectFn != nullptr && StopTriggerEffectFn != nullptr) ? TEXT("") : TEXT(" (missing; adaptive triggers inert)"));
	if (StartTriggerEffectFn == nullptr || StopTriggerEffectFn == nullptr)
	{
		// Never run with only one of the pair: a started effect that can't be stopped
		// would pin the trigger's resistance for the rest of the session.
		StartTriggerEffectFn = nullptr;
		StopTriggerEffectFn = nullptr;
	}

	const bool bEngineInit = Init();
	const bool bProviderInit = ProviderInit();
	UE_LOG(LogWyvrnHaptics, Log, TEXT("Interhaptics: Init()=%d ProviderInit()=%d"), bEngineInit, bProviderInit);
	if (!bProviderInit)
	{
		UE_LOG(LogWyvrnHaptics, Warning, TEXT("Interhaptics: ProviderInit() failed; haptics inert."));
		return false;
	}

	bAvailable = true;
	UE_LOG(LogWyvrnHaptics, Log, TEXT("Interhaptics HAR runtime initialized."));
	return true;
#else
	UE_LOG(LogWyvrnHaptics, Log, TEXT("Interhaptics: built without HAR stubs; haptics inert."));
	return false;
#endif
}

void FInterhapticsRuntime::Shutdown()
{
#if WITH_INTERHAPTICS_HAR
	if (bAvailable)
	{
		ProviderClean();
		Quit();
	}
#endif
	StartTriggerEffectFn = nullptr;
	StopTriggerEffectFn = nullptr;
	bAvailable = false;
}

bool FInterhapticsRuntime::IsAvailable() const
{
	return bAvailable;
}

int32 FInterhapticsRuntime::AddMaterial(const FString& MaterialJson)
{
#if WITH_INTERHAPTICS_HAR
	if (bAvailable)
	{
		return AddHM(TCHAR_TO_UTF8(*MaterialJson));
	}
#endif
	return -1;
}

void FInterhapticsRuntime::SetIntensity(int32 MaterialId, float Intensity)
{
#if WITH_INTERHAPTICS_HAR
	if (bAvailable)
	{
		SetEventIntensity(MaterialId, static_cast<double>(Intensity));
	}
#endif
}

void FInterhapticsRuntime::SetLoop(int32 MaterialId, int32 NumLoops)
{
#if WITH_INTERHAPTICS_HAR
	if (bAvailable)
	{
		SetEventLoop(MaterialId, NumLoops);
	}
#endif
}

void FInterhapticsRuntime::AddTarget(int32 MaterialId, EWyvrnHapticTarget Region, EWyvrnHapticSide Side)
{
#if WITH_INTERHAPTICS_HAR
	// The DualSense provider renders the hand region (left + right palm); others are ignored.
	if (bAvailable && Region == EWyvrnHapticTarget::Hand)
	{
		ELateralFlag LateralFlag = ELateralFlag::Global;
		switch (Side)
		{
		case EWyvrnHapticSide::Left:  LateralFlag = ELateralFlag::Left;  break;
		case EWyvrnHapticSide::Right: LateralFlag = ELateralFlag::Right; break;
		default:                      LateralFlag = ELateralFlag::Global; break;
		}

		FCommandData Command{ EOperator::Plus, EGroupID::Hand, LateralFlag };
		AddTargetToEventMarshal(MaterialId, &Command, 1);
	}
#endif
}

void FInterhapticsRuntime::Play(int32 MaterialId, double TimeSeconds)
{
#if WITH_INTERHAPTICS_HAR
	if (bAvailable)
	{
		// HAR computes playback position as (offset + curTime) and does NOT subtract the
		// event's start time, so offset 0 renders at the absolute engine time - well past a
		// ~1-2s effect's end => silence. Passing -TimeSeconds makes the position
		// (curTime - TimeSeconds) = time since the press, so the effect plays from its start.
		// (If HAR is fixed to subtract m_startingTime, this offset should revert to 0.)
		PlayEvent(MaterialId, -TimeSeconds, -TimeSeconds, -TimeSeconds);
	}
#endif
}

void FInterhapticsRuntime::Stop(int32 MaterialId)
{
#if WITH_INTERHAPTICS_HAR
	if (bAvailable)
	{
		StopEvent(MaterialId);
	}
#endif
}

void FInterhapticsRuntime::StopAll()
{
#if WITH_INTERHAPTICS_HAR
	if (bAvailable)
	{
		StopAllEvents();
	}
#endif
}

double FInterhapticsRuntime::GetLength(int32 MaterialId) const
{
#if WITH_INTERHAPTICS_HAR
	if (bAvailable)
	{
		return GetVibrationLength(MaterialId);
	}
#endif
	return 0.0;
}

void FInterhapticsRuntime::Render(double TimeSeconds)
{
#if WITH_INTERHAPTICS_HAR
	if (bAvailable)
	{
		SCOPE_CYCLE_COUNTER(STAT_WyvrnHaptics_Render);
		ComputeAllEvents(TimeSeconds);
		ProviderRenderHaptics();
	}
#endif
}

bool FInterhapticsRuntime::StartTriggerEffect(int32 MaterialId, bool bLeftTrigger)
{
	if (bAvailable && StartTriggerEffectFn != nullptr)
	{
		// The provider forwards scePadSetTriggerEffect's result: negative when the
		// pad rejected the effect (e.g. controller not connected yet).
		return StartTriggerEffectFn(MaterialId, bLeftTrigger) >= 0;
	}
	return false;
}

void FInterhapticsRuntime::StopTriggerEffect(bool bLeftTrigger)
{
	if (bAvailable && StopTriggerEffectFn != nullptr)
	{
		StopTriggerEffectFn(bLeftTrigger);
	}
}

#endif // PLATFORM_PS5
