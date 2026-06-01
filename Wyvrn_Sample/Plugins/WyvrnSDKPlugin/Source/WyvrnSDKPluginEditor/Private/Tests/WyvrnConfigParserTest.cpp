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
	// A command with a Head event (0.7) and a Hand event (1.0), plus an interrupt.
	const FString Config = TEXT("{ \"ExternalCommands\": [ { \"External_Command_ID\": \"Effect1\", \"Haptic_Events\": [ { \"Haptic_Effect\": \"Effect1\", \"Loop\": 0, \"Priority\": \"High\", \"Targeting\": [ { \"Gain\": 0.7, \"Target\": \"Head\" } ] }, { \"Haptic_Effect\": \"Effect1\", \"Loop\": 0, \"Priority\": \"High\", \"Targeting\": [ { \"Gain\": 1.0, \"Target\": \"Hand\" } ] } ], \"Interrupt_Command\": \"Effect2\" } ] }");

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
