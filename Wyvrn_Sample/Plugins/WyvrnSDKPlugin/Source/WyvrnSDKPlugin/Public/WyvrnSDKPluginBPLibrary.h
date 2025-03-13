// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputCoreTypes.h"
#include <map>
#include "IWyvrnSDKPlugin.h"
#include "WyvrnSDKPluginTypes.h"
#include "WyvrnSDKPluginBPLibrary.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogWyvrnBlueprintLibrary, Log, All);


UCLASS()
class WYVRNSDKPLUGIN_API UWyvrnSDKPluginBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

#pragma region Auto sort blueprint methods

	/*
	Initialize the Wyvrn SDK
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "InitSDK", Keywords = "Initialize the WyvrnSDK with AppInfo"), Category = "WyvrnSDK")
	static int32 WyvrnSDKInitSDK(const FWyvrnSDKAppInfoType& appInfo);

	/*
	Name the Wyvrn event to add extras to supplement the event
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetEventName", Keywords = "Name Wyvrn events to add extras"), Category = "WyvrnSDK")
	static int32 SetEventName(const FString& name);

	/*
	Uninitialize the Wyvrn SDK
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "UnInit", Keywords = "Uninitialize the WyvrnSDK"), Category = "WyvrnSDK")
	static int32 WyvrnSDKUnInit();

	/*
	Returns true if the plugin has been initialized. Returns false if the plugin
	is uninitialized.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "IsInitialized", Keywords = "Return true if the blueprint library is initialized"), Category = "WyvrnSDK")
	static bool IsInitialized();

#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)
private:

	static bool _sInitialized;

#endif
};
