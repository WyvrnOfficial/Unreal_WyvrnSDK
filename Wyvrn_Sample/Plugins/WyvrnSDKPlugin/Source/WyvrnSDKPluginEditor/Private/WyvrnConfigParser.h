// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WyvrnHapticTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWyvrnImport, Log, All);

/** One body-region target of a haptic event, with its side and gain (WYVRN.config Targeting entry). */
struct FWyvrnParsedTargeting
{
	EWyvrnHapticTarget Target = EWyvrnHapticTarget::Hand;
	EWyvrnHapticSide Side = EWyvrnHapticSide::Global;
	float Gain = 1.0f;
};

/** A haptic effect entry parsed from a command's Haptic_Events. */
struct FWyvrnParsedEffect
{
	FString EffectName;
	int32 Loop = 0;
	EWyvrnHapticPriority Priority = EWyvrnHapticPriority::High;
	EWyvrnHapticMixing Mixing = EWyvrnHapticMixing::Merge;
	TArray<FWyvrnParsedTargeting> Targeting;
};

/** A parsed WYVRN.config ExternalCommand carrying haptics. */
struct FWyvrnParsedCommand
{
	FString EventName;
	TArray<FWyvrnParsedEffect> Effects;
	/** Literal event names to stop (array form of Interrupts_Commands). */
	TArray<FString> InterruptCommands;
	/** True when Interrupts_Commands was the bare string "All" (stop every event). */
	bool bInterruptAll = false;
};

/**
 * Parses a WYVRN.config (the ExternalCommands / Haptic_Events / Targeting schema)
 * into intermediate command data.
 *
 * Faithful to the file: it applies no device filtering and keeps every parsed
 * target with its gain. The importer decides which targets the DualSense renders.
 * Commands that carry no haptics (chroma-only) are omitted. Returns false on
 * malformed JSON or a missing ExternalCommands array.
 */
class FWyvrnConfigParser
{
public:
	static bool Parse(const FString& ConfigJson, TArray<FWyvrnParsedCommand>& OutCommands, FString& OutError);
};
