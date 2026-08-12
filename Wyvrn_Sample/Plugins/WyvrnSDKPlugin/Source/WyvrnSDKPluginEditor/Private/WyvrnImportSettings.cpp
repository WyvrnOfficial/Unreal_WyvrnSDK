// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnImportSettings.h"

#include "Misc/App.h"
#include "Misc/Paths.h"

UWyvrnImportSettings::UWyvrnImportSettings()
{
	// Default to the Synapse / Interhaptics install location for haptic folders.
	HapticFolderRoot = TEXT("C:/Program Files (x86)/Interhaptics/hapticFolders");
	OutputContentPath = TEXT("/Game/Wyvrn");
}

FString UWyvrnImportSettings::GetDiscoveredFolder() const
{
	const FString AppName = AppFolderName.IsEmpty() ? FString(FApp::GetProjectName()) : AppFolderName;
	return FPaths::Combine(HapticFolderRoot, AppName);
}

FName UWyvrnImportSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}
