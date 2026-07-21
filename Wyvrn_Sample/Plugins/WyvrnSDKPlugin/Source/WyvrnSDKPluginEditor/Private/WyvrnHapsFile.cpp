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
