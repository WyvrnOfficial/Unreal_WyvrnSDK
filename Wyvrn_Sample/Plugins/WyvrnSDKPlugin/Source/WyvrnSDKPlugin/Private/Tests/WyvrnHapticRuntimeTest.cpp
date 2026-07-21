// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "WyvrnHapticData.h"
#include "WyvrnHapticEffect.h"
#include "WyvrnHapticRuntime.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWyvrnRuntimeDataBuildTest,
	"Wyvrn.Haptics.RuntimeDataBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWyvrnRuntimeDataBuildTest::RunTest(const FString& Parameters)
{
	// The worker only sees the POD snapshot, so Build must carry the baked
	// bHasStiffnessTrack flag through — dropping it would silently disable the
	// whole adaptive-trigger feature with the pool tests still green.
	UWyvrnHapticEffect* StiffEffect = NewObject<UWyvrnHapticEffect>(GetTransientPackage());
	StiffEffect->Json = TEXT("{\"stiff\":1}");
	StiffEffect->bHasStiffnessTrack = true;

	UWyvrnHapticEffect* PlainEffect = NewObject<UWyvrnHapticEffect>(GetTransientPackage());
	PlainEffect->Json = TEXT("{\"plain\":1}");

	UWyvrnHapticData* Data = NewObject<UWyvrnHapticData>(GetTransientPackage());
	FWyvrnHapticCommand Command;
	Command.EventName = TEXT("Cmd");
	FWyvrnHapticEvent StiffEvent;
	StiffEvent.Effect = StiffEffect;
	FWyvrnHapticEvent PlainEvent;
	PlainEvent.Effect = PlainEffect;
	FWyvrnHapticEvent StiffAgain;
	StiffAgain.Effect = StiffEffect;
	Command.Effects.Add(StiffEvent);
	Command.Effects.Add(PlainEvent);
	Command.Effects.Add(StiffAgain);
	Data->Commands.Add(Command);

	const FWyvrnRuntimeData Runtime = FWyvrnRuntimeData::Build(*Data);

	TestEqual(TEXT("one command"), Runtime.Commands.Num(), 1);
	if (Runtime.Commands.Num() != 1 || Runtime.Commands[0].Effects.Num() != 3)
	{
		AddError(TEXT("unexpected snapshot shape"));
		return false;
	}

	const TArray<FWyvrnRuntimeEvent>& Effects = Runtime.Commands[0].Effects;
	TestTrue(TEXT("stiffness flag mirrored"), Effects[0].bHasStiffness);
	TestFalse(TEXT("plain effect stays plain"), Effects[1].bHasStiffness);
	TestTrue(TEXT("shared effect keeps the flag"), Effects[2].bHasStiffness);
	TestEqual(TEXT("shared effect shares its EffectId"), Effects[0].EffectId, Effects[2].EffectId);
	TestNotEqual(TEXT("distinct effects get distinct ids"), Effects[0].EffectId, Effects[1].EffectId);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
