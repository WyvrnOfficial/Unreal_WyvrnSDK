// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnHapticData.h"

const FWyvrnHapticCommand* UWyvrnHapticData::FindCommand(const FString& EventName) const
{
	return Commands.FindByPredicate([&EventName](const FWyvrnHapticCommand& Command)
	{
		return Command.EventName == EventName;
	});
}
