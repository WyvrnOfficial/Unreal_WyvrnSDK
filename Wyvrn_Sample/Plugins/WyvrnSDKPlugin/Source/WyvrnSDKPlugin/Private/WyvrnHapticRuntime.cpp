// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnHapticRuntime.h"

#include "WyvrnHapticData.h"
#include "WyvrnHapticEffect.h"

FWyvrnRuntimeData FWyvrnRuntimeData::Build(const UWyvrnHapticData& Data)
{
	FWyvrnRuntimeData Out;
	Out.Commands.Reserve(Data.Commands.Num());

	// Distinct effect assets share an EffectId so they share one voice/material pool.
	TMap<const UWyvrnHapticEffect*, int32> EffectIds;

	for (const FWyvrnHapticCommand& Command : Data.Commands)
	{
		FWyvrnRuntimeCommand RuntimeCommand;
		RuntimeCommand.EventName = Command.EventName;
		RuntimeCommand.InterruptCommands = Command.InterruptCommands;
		RuntimeCommand.bInterruptAll = Command.bInterruptAll;

		for (const FWyvrnHapticEvent& EffectEntry : Command.Effects)
		{
			if (EffectEntry.Effect == nullptr)
			{
				continue;
			}

			int32 EffectId;
			if (const int32* Found = EffectIds.Find(EffectEntry.Effect))
			{
				EffectId = *Found;
			}
			else
			{
				EffectId = Out.EffectJson.Add(EffectEntry.Effect->Json);
				EffectIds.Add(EffectEntry.Effect, EffectId);
			}

			FWyvrnRuntimeEvent RuntimeEvent;
			RuntimeEvent.EffectId = EffectId;
			RuntimeEvent.Gain = EffectEntry.Gain;
			RuntimeEvent.Loop = EffectEntry.Loop;
			RuntimeEvent.Priority = EffectEntry.Priority;
			RuntimeEvent.Mixing = EffectEntry.Mixing;
			RuntimeEvent.Targets = EffectEntry.Targets;
			RuntimeEvent.bHasStiffness = EffectEntry.Effect->bHasStiffnessTrack;
			RuntimeCommand.Effects.Add(MoveTemp(RuntimeEvent));
		}

		Out.Commands.Add(MoveTemp(RuntimeCommand));
	}

	return Out;
}

const FWyvrnRuntimeCommand* FWyvrnRuntimeData::FindCommand(const FString& EventName) const
{
	for (const FWyvrnRuntimeCommand& Command : Commands)
	{
		if (Command.EventName == EventName)
		{
			return &Command;
		}
	}
	return nullptr;
}
