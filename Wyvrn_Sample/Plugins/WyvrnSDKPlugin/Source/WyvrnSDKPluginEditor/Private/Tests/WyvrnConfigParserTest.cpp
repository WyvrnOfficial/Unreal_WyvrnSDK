// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "WyvrnConfigParser.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWyvrnConfigParserTest,
	"Wyvrn.Haptics.ConfigParser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWyvrnConfigParserTest::RunTest(const FString& Parameters)
{
	// A command with a Head event (Global, 0.7) and a Hand event (Left, 1.0,
	// VeryHigh priority, Override mixing, infinite loop), plus an interrupt. Uses
	// the real WYVRN.config schema: "Interrupts_Commands", "Mixing", "Spatialization".
	const FString Config = TEXT("{ \"ExternalCommands\": [ { \"External_Command_ID\": \"Effect1\", \"Haptic_Events\": [ { \"Haptic_Effect\": \"Effect1\", \"Loop\": 0, \"Priority\": \"High\", \"Mixing\": \"Merge\", \"Targeting\": [ { \"Gain\": 0.7, \"Spatialization\": \"Global\", \"Target\": \"Head\" } ] }, { \"Haptic_Effect\": \"Effect1\", \"Loop\": -1, \"Priority\": \"VeryHigh\", \"Mixing\": \"Override\", \"Targeting\": [ { \"Gain\": 1.0, \"Spatialization\": \"Left\", \"Target\": \"Hand\" } ] } ], \"Interrupts_Commands\": \"Effect2\" } ] }");

	TArray<FWyvrnParsedCommand> Commands;
	FString Error;
	if (!TestTrue(TEXT("parse succeeds"), FWyvrnConfigParser::Parse(Config, Commands, Error)))
	{
		AddError(Error);
		return false;
	}

	TestEqual(TEXT("one command parsed"), Commands.Num(), 1);
	if (Commands.Num() == 1)
	{
		const FWyvrnParsedCommand& Command = Commands[0];
		TestEqual(TEXT("event name"), Command.EventName, FString(TEXT("Effect1")));
		TestEqual(TEXT("two haptic events"), Command.Effects.Num(), 2);
		TestEqual(TEXT("interrupt command captured"), Command.InterruptCommands.Num(), 1);
		if (Command.InterruptCommands.Num() == 1)
		{
			TestEqual(TEXT("interrupt target name"), Command.InterruptCommands[0], FString(TEXT("Effect2")));
		}
		if (Command.Effects.Num() == 2)
		{
			const FWyvrnParsedEffect& HandEvent = Command.Effects[1];
			TestEqual(TEXT("hand event loops infinitely"), HandEvent.Loop, -1);
			TestTrue(TEXT("hand event priority VeryHigh"), HandEvent.Priority == EWyvrnHapticPriority::VeryHigh);
			TestTrue(TEXT("hand event mixing Override"), HandEvent.Mixing == EWyvrnHapticMixing::Override);
			TestEqual(TEXT("hand event has one targeting"), HandEvent.Targeting.Num(), 1);
			if (HandEvent.Targeting.Num() == 1)
			{
				TestTrue(TEXT("hand targeting spatialization Left"), HandEvent.Targeting[0].Side == EWyvrnHapticSide::Left);
			}
		}
	}

	// Target "All" is HAR's root body-part group and subsumes Hand, so it must survive as
	// a Hand target. Regressing this does not just drop the target: the event loses all
	// targeting, gets discarded, and a command with no other content vanishes entirely.
	const FString AllTargetConfig = TEXT("{ \"ExternalCommands\": [ { \"External_Command_ID\": \"Throw_Trad\", \"Haptic_Events\": [ { \"Haptic_Effect\": \"Throw_Trad\", \"Loop\": 1, \"Mixing\": \"Merge\", \"Targeting\": [ { \"Gain\": 1, \"Spatialization\": \"Global\", \"Target\": \"All\" } ] } ] } ] }");
	TArray<FWyvrnParsedCommand> AllTargetCommands;
	if (TestTrue(TEXT("All-target parse succeeds"), FWyvrnConfigParser::Parse(AllTargetConfig, AllTargetCommands, Error)))
	{
		TestEqual(TEXT("All-target command survives"), AllTargetCommands.Num(), 1);
		if (AllTargetCommands.Num() == 1 && TestEqual(TEXT("All-target event kept"), AllTargetCommands[0].Effects.Num(), 1))
		{
			const FWyvrnParsedEffect& AllEvent = AllTargetCommands[0].Effects[0];
			TestEqual(TEXT("All-target has one targeting"), AllEvent.Targeting.Num(), 1);
			if (AllEvent.Targeting.Num() == 1)
			{
				TestTrue(TEXT("All maps to the Hand region"), AllEvent.Targeting[0].Target == EWyvrnHapticTarget::Hand);
				TestTrue(TEXT("All keeps Global spatialization"), AllEvent.Targeting[0].Side == EWyvrnHapticSide::Global);
			}
		}
	}

	// Interrupts_Commands as the bare string "All" is the stop-all sentinel (kept even
	// though it has no effects and no named interrupts).
	const FString StopAllConfig = TEXT("{ \"ExternalCommands\": [ { \"External_Command_ID\": \"StopEverything\", \"Interrupts_Commands\": \"All\" } ] }");
	TArray<FWyvrnParsedCommand> StopAllCommands;
	if (TestTrue(TEXT("stop-all parse succeeds"), FWyvrnConfigParser::Parse(StopAllConfig, StopAllCommands, Error)))
	{
		TestEqual(TEXT("stop-all command kept"), StopAllCommands.Num(), 1);
		if (StopAllCommands.Num() == 1)
		{
			TestTrue(TEXT("bInterruptAll set for bare string All"), StopAllCommands[0].bInterruptAll);
			TestEqual(TEXT("no named interrupts for bare string All"), StopAllCommands[0].InterruptCommands.Num(), 0);
		}
	}

	// Interrupts_Commands as an array containing "All" means a literal event named "All".
	const FString NamedAllConfig = TEXT("{ \"ExternalCommands\": [ { \"External_Command_ID\": \"StopNamed\", \"Interrupts_Commands\": [ \"All\" ] } ] }");
	TArray<FWyvrnParsedCommand> NamedAllCommands;
	if (TestTrue(TEXT("named-all parse succeeds"), FWyvrnConfigParser::Parse(NamedAllConfig, NamedAllCommands, Error)))
	{
		TestEqual(TEXT("named-all command kept"), NamedAllCommands.Num(), 1);
		if (NamedAllCommands.Num() == 1)
		{
			TestFalse(TEXT("bInterruptAll NOT set for array All"), NamedAllCommands[0].bInterruptAll);
			TestEqual(TEXT("array All is one literal name"), NamedAllCommands[0].InterruptCommands.Num(), 1);
		}
	}

	// A chroma-only command carries no haptics and must be dropped.
	const FString ChromaOnly = TEXT("{ \"ExternalCommands\": [ { \"External_Command_ID\": \"Start\", \"Chroma_Events\": [ { \"Chroma_Effect\": \"Idle_Keyboard\" } ] } ] }");
	TArray<FWyvrnParsedCommand> ChromaCommands;
	TestTrue(TEXT("chroma-only parse succeeds"), FWyvrnConfigParser::Parse(ChromaOnly, ChromaCommands, Error));
	TestEqual(TEXT("chroma-only yields no commands"), ChromaCommands.Num(), 0);

	// Malformed JSON fails with an error.
	TArray<FWyvrnParsedCommand> Ignored;
	FString MalformedError;
	TestFalse(TEXT("malformed JSON fails"), FWyvrnConfigParser::Parse(TEXT("{ not json"), Ignored, MalformedError));
	TestTrue(TEXT("malformed JSON reports an error"), !MalformedError.IsEmpty());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
