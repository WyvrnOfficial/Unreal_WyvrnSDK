// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "HapticVoicePool.h"

#include "Math/NumericLimits.h"

#include "IInterhapticsRuntime.h"
#include "WyvrnHapticRuntime.h"
#include "WyvrnHapticsLog.h"

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

uint8 FHapticVoicePool::MakeTriggerMask(const FWyvrnRuntimeEvent& EffectEntry)
{
	if (!EffectEntry.bHasStiffness)
	{
		return 0;
	}

	uint8 Mask = 0;
	for (const FWyvrnHapticTarget& Target : EffectEntry.Targets)
	{
		// Only the hand region maps to the controller, hence to its triggers.
		if (Target.Region != EWyvrnHapticTarget::Hand)
		{
			continue;
		}
		switch (Target.Side)
		{
		case EWyvrnHapticSide::Left:  Mask |= TriggerLeft; break;
		case EWyvrnHapticSide::Right: Mask |= TriggerRight; break;
		default:                      Mask |= TriggerLeft | TriggerRight; break;
		}
	}
	return Mask;
}

FHapticVoicePool::FHapticVoicePool(IInterhapticsRuntime& InRuntime, int32 InVoicesPerEffect)
	: Runtime(InRuntime)
	, VoicesPerEffect(FMath::Max(1, InVoicesPerEffect))
{
}

FHapticVoicePool::~FHapticVoicePool()
{
	// Leave the pad neutral: an applied trigger effect outlives playback on the
	// device, so it must be released explicitly on teardown. The backend resets the
	// pool before the runtime, so Runtime is still valid here.
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (AppliedTriggerMaterial[Side] != -1)
		{
			Runtime.StopTriggerEffect(Side == 0);
		}
	}
}

void FHapticVoicePool::Preload(const FWyvrnRuntimeData& Data)
{
	for (const FWyvrnRuntimeCommand& Command : Data.Commands)
	{
		for (const FWyvrnRuntimeEvent& EffectEntry : Command.Effects)
		{
			if (EffectEntry.EffectId < 0)
			{
				continue;
			}

			const FPoolKey Key{ EffectEntry.EffectId, MakeTargetSignature(EffectEntry.Targets) };
			if (VoicesByPool.Contains(Key))
			{
				continue;
			}

			TArray<FVoice> Voices;
			Voices.Reserve(VoicesPerEffect);
			for (int32 Index = 0; Index < VoicesPerEffect; ++Index)
			{
				const int32 MaterialId = Runtime.AddMaterial(Data.EffectJson[EffectEntry.EffectId]);
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

void FHapticVoicePool::PlayCommand(const FWyvrnRuntimeCommand& Command, double NowSeconds)
{
	WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [VoicePool]: PlayCommand('%s') — effects=%d interruptAll=%d namedInterrupts=%d."),
		*Command.EventName, Command.Effects.Num(), Command.bInterruptAll ? 1 : 0, Command.InterruptCommands.Num());

	// The bare-string "All" form (bInterruptAll) stops every event; named interrupts
	// stop only the events they name (an event literally named "All" would match here).
	if (Command.bInterruptAll)
	{
		Runtime.StopAll();
		DeactivateAllVoices();
		TriggerClaims.Empty();
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

		// Release the interrupted event's latched trigger claims too. Deliberately
		// independent of the voice lookup above: a one-shot's voices may have retired
		// long ago while its latch is still holding a trigger.
		TriggerClaims.RemoveAll([&Interrupt](const FTriggerClaim& Claim)
			{
				return Claim.EventName == Interrupt;
			});
	}

	for (const FWyvrnRuntimeEvent& EffectEntry : Command.Effects)
	{
		TArray<FVoice>* Voices = EffectEntry.EffectId >= 0
			? VoicesByPool.Find(FPoolKey{ EffectEntry.EffectId, MakeTargetSignature(EffectEntry.Targets) })
			: nullptr;
		if (Voices == nullptr || Voices->Num() == 0)
		{
			WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [VoicePool]: '%s' effect id=%d has no preloaded voices; skipped."),
				*Command.EventName, EffectEntry.EffectId);
			continue;
		}

		const bool bLooping = EffectEntry.Loop < 0;

		// Playing a stiffness-carrying event LATCHES its trigger claim, independent of
		// the voice below (which only spans the vibration window): the claim upserts on
		// (event, material) so replays re-assert ownership without stacking, and only
		// an interrupt releases it. Any pooled id works — they share one envelope.
		if (const uint8 TriggerMask = MakeTriggerMask(EffectEntry))
		{
			const int32 ClaimMaterial = (*Voices)[0].MaterialId;
			FTriggerClaim* Claim = TriggerClaims.FindByPredicate(
				[&Command, ClaimMaterial](const FTriggerClaim& Existing)
				{
					return Existing.EventName == Command.EventName && Existing.MaterialId == ClaimMaterial;
				});
			if (Claim == nullptr)
			{
				Claim = &TriggerClaims.AddDefaulted_GetRef();
				Claim->EventName = Command.EventName;
				Claim->MaterialId = ClaimMaterial;
			}
			Claim->Mask = TriggerMask;
			Claim->Sequence = NextSequence++;
		}

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
			WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [VoicePool]: '%s' effect id=%d saturated by higher-priority voices; dropped."),
				*Command.EventName, EffectEntry.EffectId);
			continue;
		}

		// Targets are already bound to this material id at preload; just (re)start it.
		Runtime.SetLoop(Voice->MaterialId, EffectEntry.Loop);
		Runtime.Play(Voice->MaterialId, NowSeconds);
		WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [VoicePool]: '%s' effect id=%d -> material %d, %s, priority=%d (playing)."),
			*Command.EventName, EffectEntry.EffectId, Voice->MaterialId,
			bLooping ? TEXT("loop") : TEXT("one-shot"), static_cast<int32>(EffectEntry.Priority));

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
	UpdateTriggerEffects();
}

void FHapticVoicePool::StopAll()
{
	WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [VoicePool]: StopAll — stopping every voice and releasing both adaptive triggers."));

	Runtime.StopAll();
	DeactivateAllVoices();
	TriggerClaims.Empty();
	// With no claims left this switches off whichever triggers were armed, and is a
	// no-op when none were.
	UpdateTriggerEffects();
}

void FHapticVoicePool::SetAdaptiveTriggersEnabled(bool bEnabled)
{
	if (bAdaptiveTriggersEnabled == bEnabled)
	{
		return;
	}

	WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [VoicePool]: adaptive triggers %s."), bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));

	bAdaptiveTriggersEnabled = bEnabled;
	// Claims are left untouched, so disabling releases the pad and enabling re-arms
	// from whatever is still claimed.
	UpdateTriggerEffects();
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

	// A dominant voice that just ended must un-duck the lower-priority voices it was
	// masking. Trigger claims are deliberately untouched: a latch outlives its
	// voice's playback window and only an interrupt releases it.
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

int32 FHapticVoicePool::GetActiveVoiceCount() const
{
	int32 Count = 0;
	for (const TPair<FPoolKey, TArray<FVoice>>& Pair : VoicesByPool)
	{
		for (const FVoice& Voice : Pair.Value)
		{
			if (Voice.bActive)
			{
				++Count;
			}
		}
	}
	return Count;
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

void FHapticVoicePool::UpdateTriggerEffects()
{
	// Per trigger, the newest latched claim wins (mirroring how Override picks the
	// newest voice); releasing it falls back to the newest survivor, and a trigger
	// with no claim left is switched off. Claims are independent of voices, so
	// neither the natural end of playback nor ducking disturbs an armed trigger.
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const uint8 SideBit = Side == 0 ? TriggerLeft : TriggerRight;

		const FTriggerClaim* Owner = nullptr;
		for (const FTriggerClaim& Claim : TriggerClaims)
		{
			if ((Claim.Mask & SideBit) != 0 && (Owner == nullptr || Claim.Sequence > Owner->Sequence))
			{
				Owner = &Claim;
			}
		}

		// The provider holds one effect per trigger and the latest call wins, so a
		// changed owner is a single re-arm; an unchanged one needs no call (the
		// stiffness envelope is static per material).
		// Disabling the triggers is modelled as "no owner": the claims survive, they
		// just stop reaching the pad until the triggers are switched back on.
		const int32 OwnerMaterial = (bAdaptiveTriggersEnabled && Owner != nullptr) ? Owner->MaterialId : -1;
		if (OwnerMaterial == AppliedTriggerMaterial[Side])
		{
			continue;
		}
		if (OwnerMaterial != -1)
		{
			if (Runtime.StartTriggerEffect(OwnerMaterial, Side == 0))
			{
				AppliedTriggerMaterial[Side] = OwnerMaterial;
				WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [VoicePool]: adaptive trigger %s -> material %d."),
					Side == 0 ? TEXT("L2") : TEXT("R2"), OwnerMaterial);
			}
			else
			{
				// Pad rejected the arm (e.g. no controller yet). Keep the recorded pad
				// state unchanged so the next lifecycle change retries this owner.
				WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [VoicePool]: adaptive trigger %s arm FAILED for material %d; will retry."),
					Side == 0 ? TEXT("L2") : TEXT("R2"), OwnerMaterial);
			}
		}
		else
		{
			Runtime.StopTriggerEffect(Side == 0);
			AppliedTriggerMaterial[Side] = -1;
			WYVRN_HAPTIC_TRACE(TEXT("WyvrnTrace [VoicePool]: adaptive trigger %s released."),
				Side == 0 ? TEXT("L2") : TEXT("R2"));
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
