// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WyvrnHapticTypes.h"

class UWyvrnHapticData;

/**
 * Plain (non-UObject) snapshot of the baked haptic command table.
 *
 * Built once on the game thread (Build reads the UObject data) and then owned and
 * read exclusively by the render worker thread. Nothing here references a UObject,
 * so it is safe to use off the game thread — which is the whole point: the worker
 * never touches the UObject system.
 */
struct FWyvrnRuntimeEvent
{
	/** Index into FWyvrnRuntimeData::EffectJson (one distinct .haps); -1 if none. */
	int32 EffectId = -1;
	float Gain = 1.0f;
	int32 Loop = 0;
	EWyvrnHapticPriority Priority = EWyvrnHapticPriority::High;
	EWyvrnHapticMixing Mixing = EWyvrnHapticMixing::Merge;
	TArray<FWyvrnHapticTarget> Targets;
	/**
	 * The effect asset carries a Stiffness track (property of the .haps, copied per
	 * event). Playing such an event latches its stiffness envelope onto the DualSense
	 * adaptive triggers its Hand sides address (Global = both, Left/Right = that one)
	 * until the event is interrupted — the envelope maps trigger travel, not time, so
	 * only a designer-authored OFF command (or stop-all) releases it.
	 */
	bool bHasStiffness = false;
};

struct FWyvrnRuntimeCommand
{
	FString EventName;
	TArray<FWyvrnRuntimeEvent> Effects;
	/** Literal event names to stop (array form of Interrupts_Commands). */
	TArray<FString> InterruptCommands;
	/** Stop every active event (the bare-string "All" form). */
	bool bInterruptAll = false;
};

struct FWyvrnRuntimeData
{
	/** Verbatim .haps JSON per EffectId; handed to HAR AddHM at preload. */
	TArray<FString> EffectJson;
	TArray<FWyvrnRuntimeCommand> Commands;

	/** Builds the snapshot from the baked UObject data. Must be called on the game thread. */
	static FWyvrnRuntimeData Build(const UWyvrnHapticData& Data);

	/** Returns the command for EventName, or nullptr. */
	const FWyvrnRuntimeCommand* FindCommand(const FString& EventName) const;
};
