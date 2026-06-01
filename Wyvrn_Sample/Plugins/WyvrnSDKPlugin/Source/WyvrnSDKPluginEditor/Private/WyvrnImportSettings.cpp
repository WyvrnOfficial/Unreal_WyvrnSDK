// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnImportSettings.h"

#include "Misc/Paths.h"

UWyvrnImportSettings::UWyvrnImportSettings()
{
	// Default to the Synapse / Interhaptics install location for haptic folders.
	HapticFolderRoot = TEXT("C:/Program Files (x86)/Interhaptics/hapticFolders");
	OutputContentPath = TEXT("/Game/Wyvrn");
}

FString UWyvrnImportSettings::GetDiscoveredFolder() const
{
	return FPaths::Combine(HapticFolderRoot, AppFolderName);
}

FName UWyvrnImportSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}
