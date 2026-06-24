// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "HapticVoicePool.h"

#include "Math/NumericLimits.h"

#include "IInterhapticsRuntime.h"
#include "WyvrnHapticData.h"
#include "WyvrnHapticEffect.h"

uint32 FHapticVoicePool::MakeTargetSignature(const TArray<FWyvrnHapticTarget>& Targets)
{
	// Order-independent: each (region, side) maps to one bit. region in [0,4], side in [0,2].
	uint32 Signature = 0;
	for (const FWyvrnHapticTarget& Target : Targets)
	{
		const uint32 Bit = static_cast<uint32>(Target.Region) * 3u + static_cast<uint32>(Target.Side);
		Signature |= (1u << Bit);
	}
	return Signature;
}

FHapticVoicePool::FHapticVoicePool(IInterhapticsRuntime& InRuntime, int32 InVoicesPerEffect)
	: Runtime(InRuntime)
	, VoicesPerEffect(FMath::Max(1, InVoicesPerEffect))
{
}

void FHapticVoicePool::Preload(const UWyvrnHapticData& Data)
{
	for (const FWyvrnHapticCommand& Command : Data.Commands)
	{
		for (const FWyvrnHapticEvent& EffectEntry : Command.Effects)
		{
			if (EffectEntry.Effect == nullptr)
			{
				continue;
			}

			const FPoolKey Key{ EffectEntry.Effect, MakeTargetSignature(EffectEntry.Targets) };
			if (VoicesByPool.Contains(Key))
			{
				continue;
			}

			TArray<FVoice> Voices;
			Voices.Reserve(VoicesPerEffect);
			for (int32 Index = 0; Index < VoicesPerEffect; ++Index)
			{
				const int32 MaterialId = Runtime.AddMaterial(EffectEntry.Effect->Json);
				if (MaterialId < 0)
				{
					break;
				}
				// Bind targets once, here: HAR's AddTargetToEvent is additive with no clear,
				// so a material id keeps one fixed target-set for its whole lifetime.
				for (const FWyvrnHapticTarget& Target : EffectEntry.Targets)
				{
					Runtime.AddTarget(MaterialId, Target.Region, Target.Side);
				}
				FVoice Voice;
				Voice.MaterialId = MaterialId;
				Voices.Add(Voice);
			}
			VoicesByPool.Add(Key, MoveTemp(Voices));
		}
	}
}

void FHapticVoicePool::PlayCommand(const FWyvrnHapticCommand& Command, double NowSeconds)
{
	// The bare-string "All" form (bInterruptAll) stops every event; named interrupts
	// stop only the events they name (an event literally named "All" would match here).
	if (Command.bInterruptAll)
	{
		Runtime.StopAll();
		DeactivateAllVoices();
	}

	for (const FString& Interrupt : Command.InterruptCommands)
	{
		if (TArray<int32>* Active = ActiveVoicesByEvent.Find(Interrupt))
		{
			// Copy the ids first: DeactivateVoice mutates ActiveVoicesByEvent.
			TArray<int32> MaterialIds = *Active;
			for (const int32 MaterialId : MaterialIds)
			{
				Runtime.Stop(MaterialId);
				DeactivateVoice(MaterialId);
			}
			ActiveVoicesByEvent.Remove(Interrupt);
		}
	}

	for (const FWyvrnHapticEvent& EffectEntry : Command.Effects)
	{
		TArray<FVoice>* Voices = EffectEntry.Effect != nullptr
			? VoicesByPool.Find(FPoolKey{ EffectEntry.Effect, MakeTargetSignature(EffectEntry.Targets) })
			: nullptr;
		if (Voices == nullptr || Voices->Num() == 0)
		{
			continue;
		}

		const bool bLooping = EffectEntry.Loop < 0;

		// Re-triggering a still-playing looping event restarts its own voice instead of
		// layering another identical infinite loop, which would pile up active voices and
		// load the per-frame renderer. One-shots keep their polyphony.
		FVoice* Voice = bLooping ? FindLiveVoiceForEvent(*Voices, Command.EventName) : nullptr;
		const bool bAlreadyTracked = Voice != nullptr;
		if (Voice == nullptr)
		{
			Voice = AcquireVoice(*Voices, EffectEntry.Priority);
		}
		if (Voice == nullptr)
		{
			// Pool saturated with strictly higher-priority voices: the new play would be
			// ducked to silence anyway, so drop it rather than evict a dominant event.
			continue;
		}

		// Targets are already bound to this material id at preload; just (re)start it.
		Runtime.SetLoop(Voice->MaterialId, EffectEntry.Loop);
		Runtime.Play(Voice->MaterialId, NowSeconds);

		Voice->bActive = true;
		Voice->bLooping = bLooping;
		Voice->Priority = EffectEntry.Priority;
		Voice->Mixing = EffectEntry.Mixing;
		Voice->Gain = EffectEntry.Gain;
		Voice->AppliedIntensity = -1.0f; // let arbitration apply the correct intensity below
		Voice->Sequence = NextSequence++;
		Voice->BusyUntilSeconds = bLooping
			? TNumericLimits<double>::Max()
			: NowSeconds + Runtime.GetLength(Voice->MaterialId);

		if (!bAlreadyTracked)
		{
			ActiveVoicesByEvent.FindOrAdd(Command.EventName).Add(Voice->MaterialId);
		}
	}

	// New voices may out-prioritize (or be out-prioritized by) what is already playing.
	UpdateArbitration();
}

void FHapticVoicePool::Tick(double NowSeconds)
{
	bool bReclaimedAny = false;
	for (TPair<FPoolKey, TArray<FVoice>>& Pair : VoicesByPool)
	{
		for (FVoice& Voice : Pair.Value)
		{
			if (Voice.bActive && !Voice.bLooping && Voice.BusyUntilSeconds > 0.0 && Voice.BusyUntilSeconds <= NowSeconds)
			{
				Voice.bActive = false;
				Voice.BusyUntilSeconds = 0.0;
				Voice.AppliedIntensity = -1.0f;
				ForgetVoice(Voice.MaterialId);
				bReclaimedAny = true;
			}
		}
	}

	// A dominant voice that just ended must un-duck the lower-priority voices it was masking.
	if (bReclaimedAny)
	{
		UpdateArbitration();
	}
}

FHapticVoicePool::FVoice* FHapticVoicePool::AcquireVoice(TArray<FVoice>& Voices, EWyvrnHapticPriority IncomingPriority)
{
	// Prefer a free (inactive) voice.
	for (FVoice& Voice : Voices)
	{
		if (!Voice.bActive)
		{
			ForgetVoice(Voice.MaterialId);
			return &Voice;
		}
	}

	// All voices busy: steal the lowest-priority / earliest-finishing one.
	FVoice* Candidate = nullptr;
	for (FVoice& Voice : Voices)
	{
		const bool bBetterSteal = Candidate == nullptr
			|| Voice.Priority < Candidate->Priority
			|| (Voice.Priority == Candidate->Priority && Voice.BusyUntilSeconds < Candidate->BusyUntilSeconds);
		if (bBetterSteal)
		{
			Candidate = &Voice;
		}
	}

	// Never let a lower-priority newcomer evict a strictly higher-priority live voice.
	if (Candidate == nullptr || Candidate->Priority > IncomingPriority)
	{
		return nullptr;
	}

	ForgetVoice(Candidate->MaterialId);
	return Candidate;
}

FHapticVoicePool::FVoice* FHapticVoicePool::FindVoice(int32 MaterialId)
{
	for (TPair<FPoolKey, TArray<FVoice>>& Pair : VoicesByPool)
	{
		for (FVoice& Voice : Pair.Value)
		{
			if (Voice.MaterialId == MaterialId)
			{
				return &Voice;
			}
		}
	}
	return nullptr;
}

FHapticVoicePool::FVoice* FHapticVoicePool::FindLiveVoiceForEvent(TArray<FVoice>& Voices, const FString& EventName)
{
	const TArray<int32>* Active = ActiveVoicesByEvent.Find(EventName);
	if (Active == nullptr)
	{
		return nullptr;
	}
	for (FVoice& Voice : Voices)
	{
		if (Voice.bActive && Active->Contains(Voice.MaterialId))
		{
			return &Voice;
		}
	}
	return nullptr;
}

void FHapticVoicePool::DeactivateVoice(int32 MaterialId)
{
	if (FVoice* Voice = FindVoice(MaterialId))
	{
		// Reset busy/loop state so the voice is reclaimable; a looping voice would
		// otherwise stay BusyUntil=Max forever and leak (never freed by Tick).
		Voice->bActive = false;
		Voice->bLooping = false;
		Voice->BusyUntilSeconds = 0.0;
		Voice->AppliedIntensity = -1.0f;
	}
	ForgetVoice(MaterialId);
}

void FHapticVoicePool::DeactivateAllVoices()
{
	for (TPair<FPoolKey, TArray<FVoice>>& Pair : VoicesByPool)
	{
		for (FVoice& Voice : Pair.Value)
		{
			Voice.bActive = false;
			Voice.bLooping = false;
			Voice.BusyUntilSeconds = 0.0;
			Voice.AppliedIntensity = -1.0f;
		}
	}
	ActiveVoicesByEvent.Empty();
}

void FHapticVoicePool::UpdateArbitration()
{
	// Gather every voice currently playing across all pools.
	TArray<FVoice*> Live;
	for (TPair<FPoolKey, TArray<FVoice>>& Pair : VoicesByPool)
	{
		for (FVoice& Voice : Pair.Value)
		{
			if (Voice.bActive)
			{
				Live.Add(&Voice);
			}
		}
	}
	if (Live.Num() == 0)
	{
		return;
	}

	// The highest priority playing dominates; everything below it is ducked.
	EWyvrnHapticPriority MaxPriority = Live[0]->Priority;
	for (const FVoice* Voice : Live)
	{
		if (Voice->Priority > MaxPriority)
		{
			MaxPriority = Voice->Priority;
		}
	}

	// Among the top-priority voices, find the newest. If it is an Override event it
	// plays alone (silencing older equal-priority voices); otherwise they all merge.
	const FVoice* Newest = nullptr;
	for (const FVoice* Voice : Live)
	{
		if (Voice->Priority == MaxPriority && (Newest == nullptr || Voice->Sequence > Newest->Sequence))
		{
			Newest = Voice;
		}
	}
	const bool bOverride = Newest != nullptr && Newest->Mixing == EWyvrnHapticMixing::Override;

	for (FVoice* Voice : Live)
	{
		bool bAudible;
		if (Voice->Priority < MaxPriority)
		{
			bAudible = false;
		}
		else
		{
			bAudible = bOverride ? (Voice == Newest) : true;
		}

		const float Desired = bAudible ? Voice->Gain : 0.0f;
		if (Voice->AppliedIntensity != Desired)
		{
			Runtime.SetIntensity(Voice->MaterialId, Desired);
			Voice->AppliedIntensity = Desired;
		}
	}
}

void FHapticVoicePool::ForgetVoice(int32 MaterialId)
{
	for (auto It = ActiveVoicesByEvent.CreateIterator(); It; ++It)
	{
		It.Value().Remove(MaterialId);
		if (It.Value().Num() == 0)
		{
			It.RemoveCurrent(); // prune emptied event keys so the map can't grow unbounded
		}
	}
}
