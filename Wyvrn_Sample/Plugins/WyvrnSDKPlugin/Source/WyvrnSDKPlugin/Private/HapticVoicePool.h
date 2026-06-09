// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WyvrnHapticTypes.h"

class IInterhapticsRuntime;
class UWyvrnHapticData;
class UWyvrnHapticEffect;

/**
 * Maps WYVRN events to HAR playback.
 *
 * HAR keys playback by material id and restarts a live id, so a single id cannot
 * overlap itself. The pool therefore loads each effect under several ids
 * (voices) for polyphony, reclaims a voice once its playback length elapses, and
 * steals the lowest-priority voice when an effect is saturated. Owned by the PS5
 * backend; not thread-safe (driven from the game thread tick).
 */
class FHapticVoicePool
{
public:
	FHapticVoicePool(IInterhapticsRuntime& InRuntime, int32 InVoicesPerEffect);

	/** Loads every distinct effect in Data, InVoicesPerEffect times, via the runtime. */
	void Preload(const UWyvrnHapticData& Data);

	/** Applies Command's Interrupt_Command stops, then plays each of its effects. */
	void PlayCommand(const FWyvrnHapticCommand& Command, double NowSeconds);

	/** Reclaims voices whose playback length has elapsed. Call once per frame. */
	void Tick(double NowSeconds);

private:
	struct FVoice
	{
		int32 MaterialId = -1;
		double BusyUntilSeconds = 0.0;
		EWyvrnHapticPriority Priority = EWyvrnHapticPriority::Low;
		bool bLooping = false;
	};

	/** Returns a free voice, or steals the lowest-priority / earliest-finishing one. */
	FVoice* AcquireVoice(TArray<FVoice>& Voices, double NowSeconds);

	/** Drops MaterialId from every event's active-voice list. */
	void ForgetVoice(int32 MaterialId);

	IInterhapticsRuntime& Runtime;
	int32 VoicesPerEffect;
	TMap<UWyvrnHapticEffect*, TArray<FVoice>> VoicesByEffect;
	TMap<FString, TArray<int32>> ActiveVoicesByEvent;
};
