// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WyvrnImportSettings.generated.h"

/**
 * Project settings for the WYVRN haptics importer (Project Settings → Plugins →
 * WYVRN Haptics Import). The importer's default source folder is
 * <HapticFolderRoot>/<AppFolderName>; the confirm dialog lets the user accept it
 * or browse to another.
 */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "WYVRN Haptics Import"))
class UWyvrnImportSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWyvrnImportSettings();

	/** Base folder Synapse/Interhaptics installs per-app WYVRN haptic folders under. */
	UPROPERTY(config, EditAnywhere, Category = "WYVRN")
	FString HapticFolderRoot;

	/** WYVRN app folder name (e.g. "Game Sample Application"). */
	UPROPERTY(config, EditAnywhere, Category = "WYVRN")
	FString AppFolderName;

	/** Content path where baked assets are written. */
	UPROPERTY(config, EditAnywhere, Category = "WYVRN")
	FString OutputContentPath;

	/** Resolved default source folder: HapticFolderRoot / AppFolderName. */
	FString GetDiscoveredFolder() const;

	virtual FName GetCategoryName() const override;
};
