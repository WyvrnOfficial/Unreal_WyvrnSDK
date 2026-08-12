// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnHapsFile.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool FWyvrnHapsFile::HasStiffnessTrack(const FString& HapsJson)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HapsJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	// .haps v6 renamed the keys (dropping the "m_" prefix) and flattened the stiffness
	// stream to a single interpolation curve, so there are no melodies or notes to walk:
	//     "stiffness": { "gain", "interpolation_function", "keyframes": [ ... ] }
	// HAR itself dispatches on the version key and parses both shapes (HapsFactory ->
	// HapsParserV5/V6), so this detector has to know both as well or a v6 effect silently
	// loses its adaptive trigger. The two spellings never coexist in one file, so probing
	// for v6 first leaves the v5 answer below untouched.
	const TSharedPtr<FJsonObject>* StiffnessV6 = nullptr;
	if (Root->TryGetObjectField(TEXT("stiffness"), StiffnessV6))
	{
		// Same "empty layer" rule as v5: the block can exist with nothing authored in it.
		const TArray<TSharedPtr<FJsonValue>>* Keyframes = nullptr;
		return (*StiffnessV6)->TryGetArrayField(TEXT("keyframes"), Keyframes) && Keyframes->Num() > 0;
	}

	const TSharedPtr<FJsonObject>* Stiffness = nullptr;
	if (!Root->TryGetObjectField(TEXT("m_stiffness"), Stiffness))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Melodies = nullptr;
	if (!(*Stiffness)->TryGetArrayField(TEXT("m_melodies"), Melodies))
	{
		return false;
	}

	// Authoring tools leave empty layers behind (a melody with zero notes), so an
	// effect only counts as stiffness-carrying when a melody holds an actual note.
	for (const TSharedPtr<FJsonValue>& MelodyValue : *Melodies)
	{
		const TSharedPtr<FJsonObject> Melody = MelodyValue.IsValid() ? MelodyValue->AsObject() : nullptr;
		if (!Melody.IsValid())
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* Notes = nullptr;
		if (Melody->TryGetArrayField(TEXT("m_notes"), Notes) && Notes->Num() > 0)
		{
			return true;
		}
	}
	return false;
}
