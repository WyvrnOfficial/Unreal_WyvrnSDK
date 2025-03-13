// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "IWyvrnSDKPlugin.h"
#include "WyvrnSDKPluginPrivatePCH.h"
#include "WyvrnSDKPluginBPLibrary.h"

#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)

#include "WyvrnAPI.h"
#include "WyvrnErrors.h"

#include "Windows/AllowWindowsPlatformTypes.h" 

using namespace WyvrnSDK;

DEFINE_LOG_CATEGORY(LogWyvrnPlugin);

class FWyvrnSDKPlugin : public IWyvrnSDKPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FWyvrnSDKPlugin, WyvrnSDKPlugin )


void FWyvrnSDKPlugin::StartupModule()
{
#if defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE
	//UE_LOG(LogWyvrnPlugin, Log, TEXT("FWyvrnSDKPlugin::StartupModule()"));
#endif

	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)

#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)
	WyvrnAPI::InitAPI();
#endif
}


void FWyvrnSDKPlugin::ShutdownModule()
{
#if defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE
	//UE_LOG(LogWyvrnPlugin, Log, TEXT("FWyvrnSDKPlugin::ShutdownModule()"));
#endif

	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)

	if (WyvrnAPI::GetIsInitializedAPI())
	{
		UWyvrnSDKPluginBPLibrary::WyvrnSDKUnInit();
		WyvrnAPI::UninitAPI();
	}
	
#endif
}


#include "Windows/HideWindowsPlatformTypes.h"

#endif
