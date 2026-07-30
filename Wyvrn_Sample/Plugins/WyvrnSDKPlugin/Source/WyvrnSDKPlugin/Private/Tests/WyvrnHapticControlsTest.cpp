// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "WyvrnErrors.h"
#include "WyvrnSDKPluginBPLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The three haptic controls are PS5-only, but their Blueprint nodes exist on every
 * platform (UHT forbids a UFUNCTION inside a preprocessor block, so they cannot be
 * compiled out per platform). A shared settings menu will therefore call them on PC.
 * This pins down that doing so is inert and safe: no backend, no module lookup, no
 * dereference - just a "not supported" result and a defined out-parameter.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWyvrnHapticControlsTest,
	"Wyvrn.Haptics.HapticControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWyvrnHapticControlsTest::RunTest(const FString& Parameters)
{
#if defined(PLATFORM_PS5) && PLATFORM_PS5
	const int32 ExpectedResult = RZRESULT_SUCCESS;
#else
	const int32 ExpectedResult = RZRESULT_NOT_SUPPORTED;
#endif

	// Every setter, including values that would be out of range on PS5. Reaching the
	// next line at all is most of the point: these must not crash off PS5.
	TestEqual(TEXT("SetHapticsEnabled(false)"), UWyvrnSDKPluginBPLibrary::SetHapticsEnabled(false), ExpectedResult);
	TestEqual(TEXT("SetHapticsEnabled(true)"), UWyvrnSDKPluginBPLibrary::SetHapticsEnabled(true), ExpectedResult);
	TestEqual(TEXT("SetVibrationGain(0)"), UWyvrnSDKPluginBPLibrary::SetVibrationGain(0), ExpectedResult);
	TestEqual(TEXT("SetVibrationGain(100)"), UWyvrnSDKPluginBPLibrary::SetVibrationGain(100), ExpectedResult);
	TestEqual(TEXT("SetVibrationGain(-5000)"), UWyvrnSDKPluginBPLibrary::SetVibrationGain(-5000), ExpectedResult);
	TestEqual(TEXT("SetVibrationGain(MAX_int32)"), UWyvrnSDKPluginBPLibrary::SetVibrationGain(MAX_int32), ExpectedResult);
	TestEqual(TEXT("SetAdaptiveTriggersEnabled(false)"), UWyvrnSDKPluginBPLibrary::SetAdaptiveTriggersEnabled(false), ExpectedResult);
	TestEqual(TEXT("SetAdaptiveTriggersEnabled(true)"), UWyvrnSDKPluginBPLibrary::SetAdaptiveTriggersEnabled(true), ExpectedResult);

	// Getters must always write their out-parameter, so a Blueprint never reads
	// uninitialised memory off the pin even on an unsupported platform. Seed each with
	// a value the implementation would never choose, so a missing write is visible.
	bool bHaptics = true;
	TestEqual(TEXT("GetHapticsEnabled result"), UWyvrnSDKPluginBPLibrary::GetHapticsEnabled(bHaptics), ExpectedResult);

	int32 Gain = -1;
	TestEqual(TEXT("GetVibrationGain result"), UWyvrnSDKPluginBPLibrary::GetVibrationGain(Gain), ExpectedResult);
	TestTrue(TEXT("GetVibrationGain always writes a value in range"), Gain >= 0 && Gain <= 100);

	bool bTriggers = true;
	TestEqual(TEXT("GetAdaptiveTriggersEnabled result"), UWyvrnSDKPluginBPLibrary::GetAdaptiveTriggersEnabled(bTriggers), ExpectedResult);

#if !(defined(PLATFORM_PS5) && PLATFORM_PS5)
	// Off PS5 the contract is specific: inert, and reporting "no haptics here" rather
	// than a stale enabled state a settings widget might echo back as a tick box.
	TestFalse(TEXT("haptics report disabled off PS5"), bHaptics);
	TestFalse(TEXT("adaptive triggers report disabled off PS5"), bTriggers);
	TestEqual(TEXT("gain reports unattenuated off PS5"), Gain, 100);
#endif

	// The nodes must actually be reachable from Blueprint on this platform - that is
	// the whole reason they are declared unconditionally rather than compiled out.
	const TCHAR* const NodeNames[] = {
		TEXT("SetHapticsEnabled"), TEXT("GetHapticsEnabled"),
		TEXT("SetVibrationGain"), TEXT("GetVibrationGain"),
		TEXT("SetAdaptiveTriggersEnabled"), TEXT("GetAdaptiveTriggersEnabled"),
	};
	for (const TCHAR* NodeName : NodeNames)
	{
		const UFunction* Function = UWyvrnSDKPluginBPLibrary::StaticClass()->FindFunctionByName(FName(NodeName));
		if (!TestNotNull(*FString::Printf(TEXT("%s is registered with the reflection system"), NodeName), Function))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s is BlueprintCallable"), NodeName), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
		TestTrue(*FString::Printf(TEXT("%s is static"), NodeName), Function->HasAnyFunctionFlags(FUNC_Static));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
