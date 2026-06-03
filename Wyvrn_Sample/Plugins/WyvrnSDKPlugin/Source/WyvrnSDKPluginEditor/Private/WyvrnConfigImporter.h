// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWyvrnHapticData;

/**
 * Bakes PS5 haptics from a WYVRN folder: reads WYVRN.config, imports the
 * controller-relevant .haps files as UWyvrnHapticEffect assets, and writes a
 * UWyvrnHapticData under the given content path. Editor-only.
 */
class FWyvrnConfigImporter
{
public:
	/**
	 * Imports from SourceFolder (which must contain WYVRN.config) and saves the
	 * baked assets under OutputContentPath (e.g. "/Game/Wyvrn").
	 * Returns the saved data asset, or nullptr on failure with OutError set.
	 */
	static UWyvrnHapticData* ImportFromFolder(const FString& SourceFolder, const FString& OutputContentPath, FString& OutError);
};
