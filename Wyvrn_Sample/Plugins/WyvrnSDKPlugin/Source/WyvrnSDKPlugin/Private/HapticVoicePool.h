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
 * steals the lowest-priority voice when an effect is saturated.
 *
 * Voices are keyed by (effect, target-set): HAR's AddTargetToEvent is additive and
 * has no clear, so a material id is bound to one target-set for its lifetime and
 * its targets are applied once at preload. The same .haps used with Global vs Left
 * vs Right therefore gets independent material ids and never conflates sides.
 *
 * It also arbitrates across every live voice: the highest-priority event playing
 * silences all lower-priority events for its duration (priority ducking), and
 * equal-priority events either merge or, when the newest is an Override event,
 * play alone. Ducking mutes via SetIntensity(0) while the event keeps playing, so
 * a ducked event's timeline still advances and it resumes mid-stream when the
 * dominant event ends. Owned by the PS5 backend; not thread-safe (driven from the
 * game thread tick).
 */
class FHapticVoicePool
{
public:
	FHapticVoicePool(IInterhapticsRuntime& InRuntime, int32 InVoicesPerEffect);

	/** Loads every distinct (effect, target-set) in Data, InVoicesPerEffect times, via the runtime. */
	void Preload(const UWyvrnHapticData& Data);

	/** Applies Command's Interrupts_Commands stops, then plays each of its effects. */
	void PlayCommand(const FWyvrnHapticCommand& Command, double NowSeconds);

	/** Reclaims voices whose playback length has elapsed. Call once per frame. */
	void Tick(double NowSeconds);

private:
	struct FVoice
	{
		int32 MaterialId = -1;
		double BusyUntilSeconds = 0.0;
		EWyvrnHapticPriority Priority = EWyvrnHapticPriority::VeryLow;
		EWyvrnHapticMixing Mixing = EWyvrnHapticMixing::Merge;
		/** Intended intensity when this voice is audible (the event's Gain). */
		float Gain = 1.0f;
		/** Last intensity sent to HAR; -1 forces the next arbitration pass to set it. */
		float AppliedIntensity = -1.0f;
		/** Monotonic play order, so Override picks the newest equal-priority voice. */
		uint64 Sequence = 0;
		bool bLooping = false;
		/** Playing in HAR right now (possibly ducked to intensity 0). */
		bool bActive = false;
	};

	// A voice pool is identified by its effect plus the exact set of targets bound
	// to its material ids (an order-independent signature of the (region, side) pairs).
	struct FPoolKey
	{
		UWyvrnHapticEffect* Effect = nullptr;
		uint32 TargetSignature = 0;

		bool operator==(const FPoolKey& Other) const
		{
			return Effect == Other.Effect && TargetSignature == Other.TargetSignature;
		}

		friend uint32 GetTypeHash(const FPoolKey& Key)
		{
			return HashCombine(GetTypeHash(Key.Effect), Key.TargetSignature);
		}
	};

	/** Order-independent signature of an event's (region, side) target set. */
	static uint32 MakeTargetSignature(const TArray<FWyvrnHapticTarget>& Targets);

	/** Returns a free voice, or steals one — but never evicts a strictly higher-priority voice. */
	FVoice* AcquireVoice(TArray<FVoice>& Voices, EWyvrnHapticPriority IncomingPriority);

	/** Finds the voice holding MaterialId across all pools, or null. */
	FVoice* FindVoice(int32 MaterialId);

	/** Finds a live voice in Voices currently owned by EventName (for restarting a loop), or null. */
	FVoice* FindLiveVoiceForEvent(TArray<FVoice>& Voices, const FString& EventName);

	/** Stops tracking a stopped voice: clears its busy/loop state and active-event entries. */
	void DeactivateVoice(int32 MaterialId);

	/** Stops tracking every voice (the "All" interrupt). */
	void DeactivateAllVoices();

	/** Re-applies priority ducking / Override muting across all live voices. */
	void UpdateArbitration();

	/** Drops MaterialId from every event's active-voice list and prunes emptied keys. */
	void ForgetVoice(int32 MaterialId);

	IInterhapticsRuntime& Runtime;
	int32 VoicesPerEffect;
	uint64 NextSequence = 0;
	TMap<FPoolKey, TArray<FVoice>> VoicesByPool;
	TMap<FString, TArray<int32>> ActiveVoicesByEvent;
};
