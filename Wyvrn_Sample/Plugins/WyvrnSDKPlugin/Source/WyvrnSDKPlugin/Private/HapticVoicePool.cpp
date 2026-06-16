// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "HapticVoicePool.h"

#include "Math/NumericLimits.h"

#include "IInterhapticsRuntime.h"
#include "WyvrnHapticData.h"
#include "WyvrnHapticEffect.h"

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
			if (EffectEntry.Effect == nullptr || VoicesByEffect.Contains(EffectEntry.Effect))
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
				FVoice Voice;
				Voice.MaterialId = MaterialId;
				Voices.Add(Voice);
			}
			VoicesByEffect.Add(EffectEntry.Effect, MoveTemp(Voices));
		}
	}
}

void FHapticVoicePool::PlayCommand(const FWyvrnHapticCommand& Command, double NowSeconds)
{
	for (const FString& Interrupt : Command.InterruptCommands)
	{
		// "All" is a WYVRN.config sentinel meaning stop every active event,
		// not an event literally named "All".
		if (Interrupt == TEXT("All"))
		{
			Runtime.StopAll();
			ActiveVoicesByEvent.Empty();
			continue;
		}

		if (TArray<int32>* Active = ActiveVoicesByEvent.Find(Interrupt))
		{
			for (const int32 MaterialId : *Active)
			{
				Runtime.Stop(MaterialId);
			}
			ActiveVoicesByEvent.Remove(Interrupt);
		}
	}

	for (const FWyvrnHapticEvent& EffectEntry : Command.Effects)
	{
		TArray<FVoice>* Voices = EffectEntry.Effect != nullptr ? VoicesByEffect.Find(EffectEntry.Effect) : nullptr;
		if (Voices == nullptr || Voices->Num() == 0)
		{
			continue;
		}

		FVoice* Voice = AcquireVoice(*Voices, NowSeconds);
		if (Voice == nullptr)
		{
			continue;
		}

		Runtime.SetIntensity(Voice->MaterialId, EffectEntry.Gain);
		Runtime.SetLoop(Voice->MaterialId, EffectEntry.Loop);
		for (const EWyvrnHapticTarget Target : EffectEntry.Targets)
		{
			Runtime.AddTarget(Voice->MaterialId, Target);
		}
		Runtime.Play(Voice->MaterialId, NowSeconds);

		Voice->bLooping = EffectEntry.Loop < 0;
		Voice->Priority = EffectEntry.Priority;
		Voice->BusyUntilSeconds = Voice->bLooping
			? TNumericLimits<double>::Max()
			: NowSeconds + Runtime.GetLength(Voice->MaterialId);

		ActiveVoicesByEvent.FindOrAdd(Command.EventName).Add(Voice->MaterialId);
	}
}

void FHapticVoicePool::Tick(double NowSeconds)
{
	for (TPair<UWyvrnHapticEffect*, TArray<FVoice>>& Pair : VoicesByEffect)
	{
		for (FVoice& Voice : Pair.Value)
		{
			if (!Voice.bLooping && Voice.BusyUntilSeconds > 0.0 && Voice.BusyUntilSeconds <= NowSeconds)
			{
				Voice.BusyUntilSeconds = 0.0;
				ForgetVoice(Voice.MaterialId);
			}
		}
	}
}

FHapticVoicePool::FVoice* FHapticVoicePool::AcquireVoice(TArray<FVoice>& Voices, double NowSeconds)
{
	for (FVoice& Voice : Voices)
	{
		if (Voice.BusyUntilSeconds <= NowSeconds)
		{
			ForgetVoice(Voice.MaterialId);
			return &Voice;
		}
	}

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

	if (Candidate != nullptr)
	{
		ForgetVoice(Candidate->MaterialId);
	}
	return Candidate;
}

void FHapticVoicePool::ForgetVoice(int32 MaterialId)
{
	for (TPair<FString, TArray<int32>>& Pair : ActiveVoicesByEvent)
	{
		Pair.Value.Remove(MaterialId);
	}
}
