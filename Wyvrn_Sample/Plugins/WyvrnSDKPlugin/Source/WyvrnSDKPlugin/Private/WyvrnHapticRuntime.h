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

/**
 * The vibration gain range exposed to games: an integer 0..100, where 100 is
 * unattenuated and 0 is silent. Out-of-range input is clamped rather than rejected,
 * and the clamped value is what the plugin caches and reports back, so a Get always
 * returns a value that was actually applied.
 *
 * Vibration only - it does not affect adaptive-trigger resistance, which has its own
 * on/off, nor does a gain of 0 stop events playing (an event may carry a Stiffness
 * track that still needs to arm a trigger). The master switch is separate again.
 *
 * Deliberately not PS5-guarded: only the HAR call itself is platform-specific, so
 * keeping the conversion here lets it be unit tested on the editor platform.
 */
inline int32 WyvrnClampVibrationGain(int32 Gain0To100)
{
	return FMath::Clamp(Gain0To100, 0, 100);
}

/**
 * Maps the 0..100 vibration gain to HAR's global intensity factor (SetGlobalIntensity):
 * 100 => 1.0, HAR's documented base value, and 0 => 0.0, genuine silence.
 * HAR clamps only the low end, so the ceiling is ours - an intensity above 1.0
 * scales the per-band amplitude past full scale and clips on the audio-out stream.
 */
inline float WyvrnVibrationGainToScalar(int32 Gain0To100)
{
	return WyvrnClampVibrationGain(Gain0To100) / 100.0f;
}
