// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "WyvrnHapticRuntime.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWyvrnVibrationGainTest,
	"Wyvrn.Haptics.VibrationGain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWyvrnVibrationGainTest::RunTest(const FString& Parameters)
{
	// The two ends of the range are the contract: 100 is HAR's documented base value
	// (a factor of exactly 1, i.e. untouched), and 0 is genuine silence.
	TestEqual(TEXT("100 maps to a factor of 1"), WyvrnVibrationGainToScalar(100), 1.0f);
	TestEqual(TEXT("0 maps to a factor of 0"), WyvrnVibrationGainToScalar(0), 0.0f);
	TestEqual(TEXT("50 maps to a factor of 0.5"), WyvrnVibrationGainToScalar(50), 0.5f);

	// HAR clamps only the low end and has no ceiling of its own, so the upper clamp is
	// ours: a factor above 1 scales the per-band amplitude past full scale and clips.
	TestEqual(TEXT("above the range clamps to 1"), WyvrnVibrationGainToScalar(250), 1.0f);
	TestEqual(TEXT("below the range clamps to 0"), WyvrnVibrationGainToScalar(-5), 0.0f);
	TestEqual(TEXT("MAX_int32 clamps to 1"), WyvrnVibrationGainToScalar(MAX_int32), 1.0f);
	TestEqual(TEXT("MIN_int32 clamps to 0"), WyvrnVibrationGainToScalar(MIN_int32), 0.0f);

	// The cached gain is the clamped one, so a Get never reports a value that was
	// never applied - hence clamping has to be idempotent.
	TestEqual(TEXT("in-range values pass through"), WyvrnClampVibrationGain(37), 37);
	TestEqual(TEXT("clamping is idempotent"), WyvrnClampVibrationGain(WyvrnClampVibrationGain(250)), 100);
	TestEqual(TEXT("clamping is idempotent below the range"), WyvrnClampVibrationGain(WyvrnClampVibrationGain(-250)), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
