// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "InterhapticsRuntime.h"

#if PLATFORM_PS5

#include "InterhapticsHARTypes.h"
#include "WyvrnHapticTypes.h"
#include "WyvrnHapticsLog.h"

#include <kernel.h>

namespace
{
	using namespace WyvrnSDK::HAR;

	// PRX module locations on the packaged title. These must match where Build.cs
	// stages the modules (see the PS5 RuntimeDependencies block). The PRX are built
	// separately from the Interhaptics HAR repo; when absent the runtime stays inert.
	const char* const kHarModulePath = "/app0/sce_module/HAR.prx";
	const char* const kProviderModulePath = "/app0/sce_module/DualSenseProvider.prx";

	// HAR.prx (engine) entry points - all extern "C" in InterhapticsEngine[Internal].h.
	typedef bool   (*FHar_Init)();
	typedef void   (*FHar_Quit)();
	typedef int    (*FHar_AddHM)(const char* /*json*/);
	typedef void   (*FHar_PlayEvent)(int /*id*/, double, double, double);
	typedef void   (*FHar_StopEvent)(int /*id*/);
	typedef void   (*FHar_StopAllEvents)();
	typedef void   (*FHar_AddTargetToEventMarshal)(int /*id*/, const FCommandData* /*targets*/, int /*size*/);
	typedef void   (*FHar_ComputeAllEvents)(double /*time*/);
	typedef void   (*FHar_SetEventIntensity)(int /*id*/, double /*intensity*/);
	typedef void   (*FHar_SetEventLoop)(int /*id*/, int /*numLoops*/);
	typedef double (*FHar_GetVibrationLength)(int /*id*/);

	// DualSenseProvider.prx entry points - extern "C" in InterhapticsProvider_DualSensePS5.h.
	typedef bool (*FProv_ProviderInit)();
	typedef bool (*FProv_ProviderClean)();
	typedef void (*FProv_ProviderRenderHaptics)();

	template <typename FnPtr>
	bool ResolveSymbol(SceKernelModule Module, const char* Symbol, FnPtr& OutPtr)
	{
		void* Addr = nullptr;
		const int Result = sceKernelDlsym(Module, Symbol, &Addr);
		if (Result != 0 || Addr == nullptr)
		{
			UE_LOG(LogWyvrnHaptics, Warning, TEXT("Interhaptics: failed to resolve symbol '%s' (0x%08x)."), ANSI_TO_TCHAR(Symbol), Result);
			OutPtr = nullptr;
			return false;
		}
		OutPtr = reinterpret_cast<FnPtr>(Addr);
		return true;
	}
}

struct FInterhapticsRuntime::FImpl
{
	SceKernelModule HarModule = -1;
	SceKernelModule ProviderModule = -1;

	FHar_Init                    Init = nullptr;
	FHar_Quit                    Quit = nullptr;
	FHar_AddHM                   AddHM = nullptr;
	FHar_PlayEvent               PlayEvent = nullptr;
	FHar_StopEvent               StopEvent = nullptr;
	FHar_StopAllEvents           StopAllEvents = nullptr;
	FHar_AddTargetToEventMarshal AddTargetToEventMarshal = nullptr;
	FHar_ComputeAllEvents        ComputeAllEvents = nullptr;
	FHar_SetEventIntensity       SetEventIntensity = nullptr;
	FHar_SetEventLoop            SetEventLoop = nullptr;
	FHar_GetVibrationLength      GetVibrationLength = nullptr;

	FProv_ProviderInit           ProviderInit = nullptr;
	FProv_ProviderClean          ProviderClean = nullptr;
	FProv_ProviderRenderHaptics  ProviderRenderHaptics = nullptr;
};

FInterhapticsRuntime::FInterhapticsRuntime()
	: Impl(MakeUnique<FImpl>())
{
}

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

	// Load the engine and provider PRX modules. A missing module leaves the runtime inert.
	Impl->HarModule = sceKernelLoadStartModule(kHarModulePath, 0, nullptr, 0, nullptr, nullptr);
	if (Impl->HarModule < 0)
	{
		UE_LOG(LogWyvrnHaptics, Log, TEXT("Interhaptics: HAR module not found at %s; haptics disabled."), ANSI_TO_TCHAR(kHarModulePath));
		return false;
	}

	Impl->ProviderModule = sceKernelLoadStartModule(kProviderModulePath, 0, nullptr, 0, nullptr, nullptr);
	if (Impl->ProviderModule < 0)
	{
		UE_LOG(LogWyvrnHaptics, Log, TEXT("Interhaptics: DualSense provider not found at %s; haptics disabled."), ANSI_TO_TCHAR(kProviderModulePath));
		Shutdown();
		return false;
	}

	bool bResolved = true;
	bResolved &= ResolveSymbol(Impl->HarModule, "Init", Impl->Init);
	bResolved &= ResolveSymbol(Impl->HarModule, "Quit", Impl->Quit);
	bResolved &= ResolveSymbol(Impl->HarModule, "AddHM", Impl->AddHM);
	bResolved &= ResolveSymbol(Impl->HarModule, "PlayEvent", Impl->PlayEvent);
	bResolved &= ResolveSymbol(Impl->HarModule, "StopEvent", Impl->StopEvent);
	bResolved &= ResolveSymbol(Impl->HarModule, "StopAllEvents", Impl->StopAllEvents);
	bResolved &= ResolveSymbol(Impl->HarModule, "AddTargetToEventMarshal", Impl->AddTargetToEventMarshal);
	bResolved &= ResolveSymbol(Impl->HarModule, "ComputeAllEvents", Impl->ComputeAllEvents);
	bResolved &= ResolveSymbol(Impl->HarModule, "SetEventIntensity", Impl->SetEventIntensity);
	bResolved &= ResolveSymbol(Impl->HarModule, "SetEventLoop", Impl->SetEventLoop);
	bResolved &= ResolveSymbol(Impl->HarModule, "GetVibrationLength", Impl->GetVibrationLength);

	bResolved &= ResolveSymbol(Impl->ProviderModule, "ProviderInit", Impl->ProviderInit);
	bResolved &= ResolveSymbol(Impl->ProviderModule, "ProviderClean", Impl->ProviderClean);
	bResolved &= ResolveSymbol(Impl->ProviderModule, "ProviderRenderHaptics", Impl->ProviderRenderHaptics);

	if (!bResolved)
	{
		UE_LOG(LogWyvrnHaptics, Warning, TEXT("Interhaptics: one or more entry points missing; haptics disabled."));
		Shutdown();
		return false;
	}

	// HAR Init() followed by the provider's ProviderInit().
	Impl->Init();
	if (!Impl->ProviderInit())
	{
		UE_LOG(LogWyvrnHaptics, Warning, TEXT("Interhaptics: ProviderInit() failed; haptics disabled."));
		Shutdown();
		return false;
	}

	bAvailable = true;
	UE_LOG(LogWyvrnHaptics, Log, TEXT("Interhaptics HAR runtime initialized."));
	return true;
}

void FInterhapticsRuntime::Shutdown()
{
	if (Impl->ProviderClean != nullptr)
	{
		Impl->ProviderClean();
	}
	if (Impl->Quit != nullptr)
	{
		Impl->Quit();
	}

	if (Impl->ProviderModule >= 0)
	{
		sceKernelStopUnloadModule(Impl->ProviderModule, 0, nullptr, 0, nullptr, nullptr);
	}
	if (Impl->HarModule >= 0)
	{
		sceKernelStopUnloadModule(Impl->HarModule, 0, nullptr, 0, nullptr, nullptr);
	}

	// Reset handles and resolved pointers so a subsequent Initialize() starts clean.
	*Impl = FImpl();
	bAvailable = false;
}

bool FInterhapticsRuntime::IsAvailable() const
{
	return bAvailable;
}

int32 FInterhapticsRuntime::AddMaterial(const FString& MaterialJson)
{
	if (!bAvailable)
	{
		return -1;
	}
	return Impl->AddHM(TCHAR_TO_UTF8(*MaterialJson));
}

void FInterhapticsRuntime::SetIntensity(int32 MaterialId, float Intensity)
{
	if (bAvailable)
	{
		Impl->SetEventIntensity(MaterialId, static_cast<double>(Intensity));
	}
}

void FInterhapticsRuntime::SetLoop(int32 MaterialId, int32 NumLoops)
{
	if (bAvailable)
	{
		Impl->SetEventLoop(MaterialId, NumLoops);
	}
}

void FInterhapticsRuntime::AddTarget(int32 MaterialId, EWyvrnHapticTarget Target)
{
	if (!bAvailable)
	{
		return;
	}

	// The DualSense provider only renders the hand region; other regions are ignored.
	if (Target != EWyvrnHapticTarget::Hand)
	{
		return;
	}

	const FCommandData Command{ EOperator::Plus, EGroupID::Hand, ELateralFlag::Global };
	Impl->AddTargetToEventMarshal(MaterialId, &Command, 1);
}

void FInterhapticsRuntime::Play(int32 MaterialId)
{
	if (bAvailable)
	{
		Impl->PlayEvent(MaterialId, 0.0, 0.0, 0.0);
	}
}

void FInterhapticsRuntime::Stop(int32 MaterialId)
{
	if (bAvailable)
	{
		Impl->StopEvent(MaterialId);
	}
}

void FInterhapticsRuntime::StopAll()
{
	if (bAvailable)
	{
		Impl->StopAllEvents();
	}
}

double FInterhapticsRuntime::GetLength(int32 MaterialId) const
{
	if (!bAvailable)
	{
		return 0.0;
	}
	return Impl->GetVibrationLength(MaterialId);
}

void FInterhapticsRuntime::Render(double TimeSeconds)
{
	if (bAvailable)
	{
		Impl->ComputeAllEvents(TimeSeconds);
		Impl->ProviderRenderHaptics();
	}
}

#endif // PLATFORM_PS5
