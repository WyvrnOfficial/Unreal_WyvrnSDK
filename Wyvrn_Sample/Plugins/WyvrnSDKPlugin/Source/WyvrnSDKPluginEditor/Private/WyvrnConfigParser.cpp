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
		if (In == TEXT("Chest")) { Out = EWyvrnHapticTarget::Chest; return true; }
		if (In == TEXT("Waist")) { Out = EWyvrnHapticTarget::Waist; return true; }
		if (In == TEXT("Leg")) { Out = EWyvrnHapticTarget::Leg; return true; }
		return false;
	}

	EWyvrnHapticPriority ParsePriority(const FString& In)
	{
		if (In.Equals(TEXT("Low"), ESearchCase::IgnoreCase)) { return EWyvrnHapticPriority::Low; }
		if (In.Equals(TEXT("Medium"), ESearchCase::IgnoreCase)) { return EWyvrnHapticPriority::Medium; }
		return EWyvrnHapticPriority::High;
	}

	// Interrupt_Command may be a single event name or an array of them.
	void ParseInterruptCommands(const TSharedPtr<FJsonObject>& CommandObject, TArray<FString>& OutCommands)
	{
		const TArray<TSharedPtr<FJsonValue>>* AsArray = nullptr;
		if (CommandObject->TryGetArrayField(TEXT("Interrupt_Command"), AsArray))
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
		if (CommandObject->TryGetStringField(TEXT("Interrupt_Command"), Single) && !Single.IsEmpty())
		{
			OutCommands.Add(Single);
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

				FWyvrnParsedTargeting ParsedTargeting;
				ParsedTargeting.Target = Target;
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

		ParseInterruptCommands(CommandObject, Parsed.InterruptCommands);

		// Drop chroma-only / empty commands: they produce no haptics on PS5.
		if (Parsed.Effects.Num() > 0 || Parsed.InterruptCommands.Num() > 0)
		{
			OutCommands.Add(MoveTemp(Parsed));
		}
	}

	return true;
}
