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

	// --- PS5 haptic controls -------------------------------------------------------
	// Three independent settings: a master on/off, a vibration strength, and an
	// adaptive-trigger on/off. All may be called before InitSDK and survive
	// InitSDK/UnInit cycles. Each returns RZRESULT_SUCCESS on PS5,
	// RZRESULT_NOT_SUPPORTED on every other platform, or RZRESULT_INVALID on PS5 if
	// the plugin module itself is unavailable.

	/*
	PS5 only. Master haptics on/off.
	Off is a hard off: anything playing stops, both adaptive triggers are released,
	and further events are skipped entirely rather than played inaudibly. Turning it
	back on does not resume what was stopped.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetHapticsEnabled", Keywords = "Enable disable all Wyvrn haptics master on off PS5"), Category = "WyvrnSDK")
	static int32 SetHapticsEnabled(bool enabled);

	/*
	PS5 only. Writes the master haptics on/off state to outEnabled.
	outEnabled is false on platforms without haptics support.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetHapticsEnabled", Keywords = "Get whether Wyvrn haptics are enabled master on off PS5"), Category = "WyvrnSDK")
	static int32 GetHapticsEnabled(bool& outEnabled);

	/*
	PS5 only. Sets the vibration strength, 0..100 (0 = silent, 100 = full strength).
	Values outside the range are clamped.

	Vibration only. It does not touch adaptive-trigger resistance, which keeps the
	strength the effect author designed and has its own on/off. A gain of 0 silences
	vibration but still plays events, because an event may carry a Stiffness track
	that needs to arm a trigger - use SetHapticsEnabled(false) to stop everything.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetVibrationGain", Keywords = "Set the Wyvrn vibration gain intensity strength volume PS5", ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"), Category = "WyvrnSDK")
	static int32 SetVibrationGain(int32 gain);

	/*
	PS5 only. Writes the current vibration gain (0..100) to outGain.
	outGain is set to 100 (unattenuated) on platforms without haptics support.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetVibrationGain", Keywords = "Get the Wyvrn vibration gain intensity strength volume PS5"), Category = "WyvrnSDK")
	static int32 GetVibrationGain(int32& outGain);

	/*
	PS5 only. Turns DualSense adaptive-trigger resistance on or off, independently of
	vibration. Off releases whatever is armed but leaves events playing and vibrating;
	on re-arms whatever is still claimed.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetAdaptiveTriggersEnabled", Keywords = "Enable disable DualSense adaptive triggers resistance stiffness PS5"), Category = "WyvrnSDK")
	static int32 SetAdaptiveTriggersEnabled(bool enabled);

	/*
	PS5 only. Writes the adaptive-trigger on/off state to outEnabled.
	outEnabled is false on platforms without adaptive triggers.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetAdaptiveTriggersEnabled", Keywords = "Get whether DualSense adaptive triggers are enabled PS5"), Category = "WyvrnSDK")
	static int32 GetAdaptiveTriggersEnabled(bool& outEnabled);

#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)
private:

	static bool _sInitialized;

#endif
};
