// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnConfigParser.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(LogWyvrnImport);

namespace
{
	bool ParseTarget(const FString& In, EWyvrnHapticTarget& Out)
	{
		if (In == TEXT("Head")) { Out = EWyvrnHapticTarget::Head; return true; }
		if (In == TEXT("Hand")) { Out = EWyvrnHapticTarget::Hand; return true; }
		// WYVRN's "All" is HAR's root body-part group (GroupID All = 0), which subsumes
		// Hand. This backend renders nothing but the hand anyway - FInterhapticsRuntime::
		// AddTarget discards every other region - so "All" and "Hand" produce the identical
		// HAR call here. Without this case the targeting is dropped, the event is left with
		// no targets and discarded whole, and a command with no other content disappears
		// from the baked data entirely (its SetEventName then matches nothing at runtime).
		if (In == TEXT("All")) { Out = EWyvrnHapticTarget::Hand; return true; }
		if (In == TEXT("Chest")) { Out = EWyvrnHapticTarget::Chest; return true; }
		if (In == TEXT("Waist")) { Out = EWyvrnHapticTarget::Waist; return true; }
		if (In == TEXT("Leg")) { Out = EWyvrnHapticTarget::Leg; return true; }
		return false;
	}

	EWyvrnHapticPriority ParsePriority(const FString& In)
	{
		if (In.Equals(TEXT("VeryLow"), ESearchCase::IgnoreCase)) { return EWyvrnHapticPriority::VeryLow; }
		if (In.Equals(TEXT("Low"), ESearchCase::IgnoreCase)) { return EWyvrnHapticPriority::Low; }
		if (In.Equals(TEXT("Medium"), ESearchCase::IgnoreCase)) { return EWyvrnHapticPriority::Medium; }
		if (In.Equals(TEXT("High"), ESearchCase::IgnoreCase)) { return EWyvrnHapticPriority::High; }
		if (In.Equals(TEXT("VeryHigh"), ESearchCase::IgnoreCase)) { return EWyvrnHapticPriority::VeryHigh; }
		// Unknown/missing Priority defaults to Medium: a malformed event must not dominate,
		// since priority ducking silences every event below the highest one playing.
		if (!In.IsEmpty())
		{
			UE_LOG(LogWyvrnImport, Warning, TEXT("WyvrnConfigParser: unknown Priority '%s'; defaulting to Medium."), *In);
		}
		return EWyvrnHapticPriority::Medium;
	}

	EWyvrnHapticMixing ParseMixing(const FString& In)
	{
		if (In.Equals(TEXT("Override"), ESearchCase::IgnoreCase)) { return EWyvrnHapticMixing::Override; }
		return EWyvrnHapticMixing::Merge;
	}

	EWyvrnHapticSide ParseSpatialization(const FString& In)
	{
		if (In.Equals(TEXT("Left"), ESearchCase::IgnoreCase)) { return EWyvrnHapticSide::Left; }
		if (In.Equals(TEXT("Right"), ESearchCase::IgnoreCase)) { return EWyvrnHapticSide::Right; }
		return EWyvrnHapticSide::Global;
	}

	// Interrupts_Commands says what to stop when this command fires. WYVRN semantics:
	// the bare string "All" stops EVERY event, but an ARRAY (even one containing "All")
	// is a list of literal event names to stop. So an array element "All" means an event
	// literally named "All", not the stop-everything sentinel.
	void ParseInterruptCommands(const TSharedPtr<FJsonObject>& CommandObject, TArray<FString>& OutCommands, bool& bOutInterruptAll)
	{
		const TArray<TSharedPtr<FJsonValue>>* AsArray = nullptr;
		if (CommandObject->TryGetArrayField(TEXT("Interrupts_Commands"), AsArray))
		{
			for (const TSharedPtr<FJsonValue>& Value : *AsArray)
			{
				FString Name;
				if (Value.IsValid() && Value->TryGetString(Name) && !Name.IsEmpty())
				{
					OutCommands.Add(Name);
				}
			}
			return;
		}

		FString Single;
		if (CommandObject->TryGetStringField(TEXT("Interrupts_Commands"), Single) && !Single.IsEmpty())
		{
			if (Single.Equals(TEXT("All"), ESearchCase::IgnoreCase))
			{
				bOutInterruptAll = true;
			}
			else
			{
				OutCommands.Add(Single);
			}
		}
	}

	FWyvrnParsedEffect ParseHapticEvent(const TSharedPtr<FJsonObject>& EventObject, const FString& EventName)
	{
		FWyvrnParsedEffect Effect;
		EventObject->TryGetStringField(TEXT("Haptic_Effect"), Effect.EffectName);
		EventObject->TryGetNumberField(TEXT("Loop"), Effect.Loop);

		FString PriorityString;
		EventObject->TryGetStringField(TEXT("Priority"), PriorityString);
		Effect.Priority = ParsePriority(PriorityString);

		FString MixingString;
		EventObject->TryGetStringField(TEXT("Mixing"), MixingString);
		Effect.Mixing = ParseMixing(MixingString);

		const TArray<TSharedPtr<FJsonValue>>* Targeting = nullptr;
		if (EventObject->TryGetArrayField(TEXT("Targeting"), Targeting))
		{
			for (const TSharedPtr<FJsonValue>& TargetingValue : *Targeting)
			{
				const TSharedPtr<FJsonObject> TargetingObject = TargetingValue->AsObject();
				if (!TargetingObject.IsValid())
				{
					continue;
				}

				FString TargetString;
				TargetingObject->TryGetStringField(TEXT("Target"), TargetString);

				EWyvrnHapticTarget Target;
				if (!ParseTarget(TargetString, Target))
				{
					UE_LOG(LogWyvrnImport, Warning,
						TEXT("WyvrnConfigParser: unknown Target '%s' in event '%s'; skipping it."),
						*TargetString, *EventName);
					continue;
				}

				double Gain = 1.0;
				TargetingObject->TryGetNumberField(TEXT("Gain"), Gain);

				FString SpatializationString;
				TargetingObject->TryGetStringField(TEXT("Spatialization"), SpatializationString);

				FWyvrnParsedTargeting ParsedTargeting;
				ParsedTargeting.Target = Target;
				ParsedTargeting.Side = ParseSpatialization(SpatializationString);
				ParsedTargeting.Gain = static_cast<float>(Gain);
				Effect.Targeting.Add(ParsedTargeting);
			}
		}

		return Effect;
	}
}

bool FWyvrnConfigParser::Parse(const FString& ConfigJson, TArray<FWyvrnParsedCommand>& OutCommands, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ConfigJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("WyvrnConfigParser: WYVRN.config is not valid JSON.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Commands = nullptr;
	if (!Root->TryGetArrayField(TEXT("ExternalCommands"), Commands))
	{
		OutError = TEXT("WyvrnConfigParser: WYVRN.config has no 'ExternalCommands' array.");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& CommandValue : *Commands)
	{
		const TSharedPtr<FJsonObject> CommandObject = CommandValue->AsObject();
		if (!CommandObject.IsValid())
		{
			continue;
		}

		FWyvrnParsedCommand Parsed;
		if (!CommandObject->TryGetStringField(TEXT("External_Command_ID"), Parsed.EventName) || Parsed.EventName.IsEmpty())
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* HapticEvents = nullptr;
		if (CommandObject->TryGetArrayField(TEXT("Haptic_Events"), HapticEvents))
		{
			for (const TSharedPtr<FJsonValue>& EventValue : *HapticEvents)
			{
				const TSharedPtr<FJsonObject> EventObject = EventValue->AsObject();
				if (!EventObject.IsValid())
				{
					continue;
				}

				FWyvrnParsedEffect Effect = ParseHapticEvent(EventObject, Parsed.EventName);
				if (Effect.EffectName.IsEmpty() || Effect.Targeting.Num() == 0)
				{
					continue;
				}
				Parsed.Effects.Add(MoveTemp(Effect));
			}
		}

		ParseInterruptCommands(CommandObject, Parsed.InterruptCommands, Parsed.bInterruptAll);

		// Drop chroma-only / empty commands: they produce no haptics on PS5. A stop-all
		// command has no effects and no named interrupts, so keep it via bInterruptAll.
		if (Parsed.Effects.Num() > 0 || Parsed.InterruptCommands.Num() > 0 || Parsed.bInterruptAll)
		{
			OutCommands.Add(MoveTemp(Parsed));
		}
	}

	return true;
}
