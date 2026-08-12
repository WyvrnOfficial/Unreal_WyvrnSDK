// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "WyvrnHapsFile.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWyvrnHapsFileTest,
	"Wyvrn.Haptics.HapsFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWyvrnHapsFileTest::RunTest(const FString& Parameters)
{
	// A .haps with an authored Stiffness track: one melody holding one note whose
	// amplitude envelope maps trigger travel to resistance. Real .haps v5 shape:
	// authored stiffness notes carry all their scalars INSIDE m_hapticEffect
	// (unlike vibration notes, which keep m_gain/m_length at the note level).
	const FString WithStiffness = TEXT("{ \"m_version\": \"5\", \"m_stiffness\": { \"m_gain\": 1.0, \"m_loop\": 0, \"m_melodies\": [ { \"m_gain\": 1.0, \"m_mute\": 0, \"m_notes\": [ { \"m_hapticEffect\": { \"m_amplitudeModulation\": { \"m_extrapolationStrategy\": 1, \"m_keyframes\": [ { \"m_time\": 0.0, \"m_value\": 0.0 }, { \"m_time\": 1.0, \"m_value\": 1.0 } ] }, \"m_gain\": 1, \"m_length\": 1, \"m_priority\": 1, \"m_startingPoint\": 0 } } ] } ], \"m_signalEvaluationMethod\": 3 }, \"m_vibration\": { \"m_melodies\": [] } }");
	TestTrue(TEXT("stiffness melody with a note is detected"), FWyvrnHapsFile::HasStiffnessTrack(WithStiffness));

	// The common case: the m_stiffness block exists but its melody list is empty.
	const FString EmptyMelodies = TEXT("{ \"m_stiffness\": { \"m_gain\": 1.0, \"m_loop\": 0, \"m_melodies\": [], \"m_signalEvaluationMethod\": 3 }, \"m_vibration\": { \"m_melodies\": [ { \"m_notes\": [ { \"m_gain\": 1.0 } ] } ] } }");
	TestFalse(TEXT("empty stiffness melody list is not a track"), FWyvrnHapsFile::HasStiffnessTrack(EmptyMelodies));

	// Authoring tools can leave an empty layer: a melody with zero notes.
	const FString EmptyNotes = TEXT("{ \"m_stiffness\": { \"m_melodies\": [ { \"m_gain\": 1.0, \"m_mute\": 0, \"m_notes\": [] } ] } }");
	TestFalse(TEXT("stiffness melody without notes is not a track"), FWyvrnHapsFile::HasStiffnessTrack(EmptyNotes));

	// .haps v6: no "m_" prefix and no melodies/notes — the stiffness stream is one
	// interpolation curve. Shape taken from a real v6 export (Interhaptics Composer).
	const FString V6WithStiffness = TEXT("{ \"version\": \"6\", \"gain\": 1.0, \"stiffness\": { \"gain\": 1.0, \"interpolation_function\": \"Cubic\", \"keyframes\": [ { \"position\": 0.0, \"slope\": 0.0, \"value\": 0.0 }, { \"position\": 1.0, \"slope\": 0.0, \"value\": 0.96 } ] }, \"vibration\": { \"melodies\": [ { \"notes\": [] } ], \"transients\": [] } }");
	TestTrue(TEXT("v6 stiffness curve with keyframes is detected"), FWyvrnHapsFile::HasStiffnessTrack(V6WithStiffness));

	// Same empty-layer rule as v5: the block exists but nothing was authored into it.
	const FString V6EmptyKeyframes = TEXT("{ \"version\": \"6\", \"stiffness\": { \"gain\": 1.0, \"interpolation_function\": \"Cubic\", \"keyframes\": [] }, \"vibration\": { \"melodies\": [ { \"notes\": [ { \"gain\": 1.0 } ] } ] } }");
	TestFalse(TEXT("v6 stiffness with no keyframes is not a track"), FWyvrnHapsFile::HasStiffnessTrack(V6EmptyKeyframes));

	const FString V6NoKeyframesField = TEXT("{ \"version\": \"6\", \"stiffness\": { \"gain\": 1.0 } }");
	TestFalse(TEXT("v6 stiffness without a keyframes field is not a track"), FWyvrnHapsFile::HasStiffnessTrack(V6NoKeyframesField));

	// A v6 vibration-only effect (the common case) must stay false: no stiffness block.
	const FString V6VibrationOnly = TEXT("{ \"version\": \"6\", \"gain\": 1.0, \"vibration\": { \"melodies\": [ { \"notes\": [ { \"gain\": 1.0 } ] } ], \"transients\": [] } }");
	TestFalse(TEXT("v6 vibration-only effect is not a track"), FWyvrnHapsFile::HasStiffnessTrack(V6VibrationOnly));

	// Vibration-only content must not count, and neither may malformed JSON.
	const FString NoStiffnessBlock = TEXT("{ \"m_vibration\": { \"m_melodies\": [ { \"m_notes\": [ { \"m_gain\": 1.0 } ] } ] } }");
	TestFalse(TEXT("no m_stiffness block is not a track"), FWyvrnHapsFile::HasStiffnessTrack(NoStiffnessBlock));
	TestFalse(TEXT("malformed JSON is not a track"), FWyvrnHapsFile::HasStiffnessTrack(TEXT("{ not json")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
