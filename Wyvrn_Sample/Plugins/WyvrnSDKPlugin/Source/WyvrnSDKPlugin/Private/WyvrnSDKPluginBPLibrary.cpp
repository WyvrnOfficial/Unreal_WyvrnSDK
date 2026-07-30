// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnSDKPluginBPLibrary.h" //___HACK_UE4_VERSION_4_16_OR_GREATER
#include "WyvrnSDKPluginPrivatePCH.h"
//#include "WyvrnSDKPluginBPLibrary.h" //___HACK_UE4_VERSION_4_15_OR_LESS

#include "WyvrnAPI.h"
#include "WyvrnErrors.h"
#include <string>

#if defined(PLATFORM_PS5) && PLATFORM_PS5
#include "IWyvrnHapticBackend.h"
#include "WyvrnHapticRuntime.h"
#include "WyvrnHapticsLog.h"
#endif


DEFINE_LOG_CATEGORY(LogWyvrnBlueprintLibrary);


#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)

#include "Misc/Paths.h"
#include "Windows/AllowWindowsPlatformTypes.h" 


using namespace WyvrnSDK;

bool UWyvrnSDKPluginBPLibrary::_sInitialized = false;

#endif

//UWyvrnSDKPluginBPLibrary::UWyvrnSDKPluginBPLibrary(const FPostConstructInitializeProperties& PCIP) //___HACK_UE4_VERSION_4_8_OR_LESS
//	: Super(PCIP) //___HACK_UE4_VERSION_4_8_OR_LESS
UWyvrnSDKPluginBPLibrary::UWyvrnSDKPluginBPLibrary(const FObjectInitializer& ObjectInitializer) //___HACK_UE4_VERSION_4_9_OR_GREATER
	: Super(ObjectInitializer) //___HACK_UE4_VERSION_4_9_OR_GREATER
{
#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)
	// Load module
	IWyvrnSDKPlugin::Get();
#endif
}

int32 UWyvrnSDKPluginBPLibrary::WyvrnSDKInitSDK(const FWyvrnSDKAppInfoType& appInfo)
{
#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)
	if (!WyvrnAPI::GetIsInitializedAPI())
	{
		//Expected scenario: Wyvrn SDK is not installed or out of date
		//UE_LOG(LogWyvrnBlueprintLibrary, Error, TEXT("UWyvrnSDKPluginBPLibrary: API is not initialized!"));
		return -1;
	}
	if (!_sInitialized)
	{
		WyvrnSDK::APPINFOTYPE coreAppInfo = {};

		std::wstring title = TCHAR_TO_WCHAR(*appInfo.Title);
		wcscpy_s(coreAppInfo.Title, 256, title.c_str());

		std::wstring desc = TCHAR_TO_WCHAR(*appInfo.Description);
		wcscpy_s(coreAppInfo.Description, 1024, desc.c_str());

		std::wstring name = TCHAR_TO_WCHAR(*appInfo.Author_Name);
		wcscpy_s(coreAppInfo.Author.Name, 256, name.c_str());

		std::wstring contact = TCHAR_TO_WCHAR(*appInfo.Author_Contact);
		wcscpy_s(coreAppInfo.Author.Contact, 256, contact.c_str());

		//appInfo.SupportedDevice = 
		//    0x01 | // Keyboards
		//    0x02 | // Mice
		//    0x04 | // Headset
		//    0x08 | // Mousepads
		//    0x10 | // Keypads
		//    0x20   // ChromaLink devices
		//    ;
		//coreAppInfo.SupportedDevice = appInfo.SupportedDevice;
		coreAppInfo.Category = appInfo.Category;

		// Init the SDK
		long result = WyvrnAPI::CoreInitSDK(&coreAppInfo);

		if (result == RZRESULT_SUCCESS)
		{
			_sInitialized = true;
		}

		return result;
	}
	else
	{
		return -1;
	}
#elif defined(PLATFORM_PS5) && PLATFORM_PS5
	if (IWyvrnHapticBackend* Backend = GetWyvrnHapticBackend())
	{
		return Backend->Initialize() ? 0 : -1;
	}
	return -1;
#else
	return -1;
#endif
}

int32 UWyvrnSDKPluginBPLibrary::WyvrnSDKUnInit()
{
#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)
	if (!WyvrnAPI::GetIsInitializedAPI())
	{
		return -1;
	}
	// Stop all animations
	// UnInit the SDK
	//UE_LOG(LogWyvrnBlueprintLibrary, Log, TEXT("UWyvrnSDKPluginBPLibrary:: Uninit"));
	if (_sInitialized)
	{
		RZRESULT result = WyvrnAPI::CoreUnInit();
		_sInitialized = false;
		return result;
	}
	else
	{
		return -1;
	}
#elif defined(PLATFORM_PS5) && PLATFORM_PS5
	if (IWyvrnHapticBackend* Backend = GetWyvrnHapticBackend())
	{
		Backend->Shutdown();
	}
	return 0;
#else
	return -1;
#endif
}

int32 UWyvrnSDKPluginBPLibrary::SetEventName(const FString& name)
{
#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)
	if (!WyvrnAPI::GetIsInitializedAPI())
	{
		return -1;
	}
	return WyvrnAPI::CoreSetEventName(TCHAR_TO_WCHAR(*name));
#elif defined(PLATFORM_PS5) && PLATFORM_PS5
	WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [BP]: SetEventName('%s') requested (game thread)."), *name);
	IWyvrnHapticBackend* Backend = GetWyvrnHapticBackend();
	if (Backend == nullptr || !Backend->IsInitialized())
	{
		WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [BP]: SetEventName('%s') DROPPED — backend %s."),
			*name, (Backend == nullptr) ? TEXT("unavailable") : TEXT("not initialized"));
		return -1;
	}
	Backend->SetEventName(name);
	return 0;
#else
	return -1;
#endif
}

bool UWyvrnSDKPluginBPLibrary::IsInitialized()
{
#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)
	return _sInitialized;
#elif defined(PLATFORM_PS5) && PLATFORM_PS5
	const IWyvrnHapticBackend* Backend = GetWyvrnHapticBackend();
	return Backend != nullptr && Backend->IsInitialized();
#else
	return false;
#endif
}

int32 UWyvrnSDKPluginBPLibrary::SetHapticsEnabled(bool enabled)
{
#if defined(PLATFORM_PS5) && PLATFORM_PS5
	WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [BP]: SetHapticsEnabled(%s) requested (game thread)."), enabled ? TEXT("true") : TEXT("false"));

	IWyvrnHapticBackend* Backend = GetWyvrnHapticBackend();
	if (Backend == nullptr)
	{
		WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [BP]: SetHapticsEnabled DROPPED — backend unavailable."));
		return RZRESULT_INVALID;
	}

	// Deliberately NOT gated on IsInitialized(): the backend caches the setting and
	// the render worker applies it once HAR comes up, so a title can set it at startup.
	Backend->SetHapticsEnabled(enabled);
	return RZRESULT_SUCCESS;
#else
	// These are PS5-only levers: RzChromatic has no equivalent, so there is
	// deliberately nothing to drive on Windows/Xbox.
	(void)enabled;
	return RZRESULT_NOT_SUPPORTED;
#endif
}

int32 UWyvrnSDKPluginBPLibrary::GetHapticsEnabled(bool& outEnabled)
{
#if defined(PLATFORM_PS5) && PLATFORM_PS5
	const IWyvrnHapticBackend* Backend = GetWyvrnHapticBackend();
	if (Backend == nullptr)
	{
		outEnabled = false;
		return RZRESULT_INVALID;
	}

	outEnabled = Backend->AreHapticsEnabled();
	return RZRESULT_SUCCESS;
#else
	outEnabled = false; // no haptics to enable off PS5
	return RZRESULT_NOT_SUPPORTED;
#endif
}

int32 UWyvrnSDKPluginBPLibrary::SetVibrationGain(int32 gain)
{
#if defined(PLATFORM_PS5) && PLATFORM_PS5
	const int32 Clamped = WyvrnClampVibrationGain(gain);
	WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [BP]: SetVibrationGain(%d -> %d) requested (game thread)."), gain, Clamped);

	IWyvrnHapticBackend* Backend = GetWyvrnHapticBackend();
	if (Backend == nullptr)
	{
		WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [BP]: SetVibrationGain DROPPED — backend unavailable."));
		return RZRESULT_INVALID;
	}

	Backend->SetVibrationGain(Clamped);
	return RZRESULT_SUCCESS;
#else
	(void)gain;
	return RZRESULT_NOT_SUPPORTED;
#endif
}

int32 UWyvrnSDKPluginBPLibrary::GetVibrationGain(int32& outGain)
{
#if defined(PLATFORM_PS5) && PLATFORM_PS5
	const IWyvrnHapticBackend* Backend = GetWyvrnHapticBackend();
	if (Backend == nullptr)
	{
		outGain = 100;
		return RZRESULT_INVALID;
	}

	outGain = Backend->GetVibrationGain();
	return RZRESULT_SUCCESS;
#else
	outGain = 100; // unattenuated: the gain has no meaning off PS5
	return RZRESULT_NOT_SUPPORTED;
#endif
}

int32 UWyvrnSDKPluginBPLibrary::SetAdaptiveTriggersEnabled(bool enabled)
{
#if defined(PLATFORM_PS5) && PLATFORM_PS5
	WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [BP]: SetAdaptiveTriggersEnabled(%s) requested (game thread)."), enabled ? TEXT("true") : TEXT("false"));

	IWyvrnHapticBackend* Backend = GetWyvrnHapticBackend();
	if (Backend == nullptr)
	{
		WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [BP]: SetAdaptiveTriggersEnabled DROPPED — backend unavailable."));
		return RZRESULT_INVALID;
	}

	Backend->SetAdaptiveTriggersEnabled(enabled);
	return RZRESULT_SUCCESS;
#else
	(void)enabled;
	return RZRESULT_NOT_SUPPORTED;
#endif
}

int32 UWyvrnSDKPluginBPLibrary::GetAdaptiveTriggersEnabled(bool& outEnabled)
{
#if defined(PLATFORM_PS5) && PLATFORM_PS5
	const IWyvrnHapticBackend* Backend = GetWyvrnHapticBackend();
	if (Backend == nullptr)
	{
		outEnabled = false;
		return RZRESULT_INVALID;
	}

	outEnabled = Backend->AreAdaptiveTriggersEnabled();
	return RZRESULT_SUCCESS;
#else
	outEnabled = false; // no adaptive triggers off PS5
	return RZRESULT_NOT_SUPPORTED;
#endif
}

#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)

#include "Windows/HideWindowsPlatformTypes.h"

#endif
