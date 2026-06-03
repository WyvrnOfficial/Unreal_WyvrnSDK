// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WyvrnHapticEffect.generated.h"

/**
 * A single Interhaptics haptic effect imported from a .haps file.
 *
 * Stores the effect as JSON text so the PS5 backend can hand it verbatim to
 * HAR AddHM(const char*) at runtime. Imported at editor time and cooked into the
 * pak, so no loose files are needed on the packaged title.
 */
UCLASS(BlueprintType)
class WYVRNSDKPLUGIN_API UWyvrnHapticEffect : public UObject
{
	GENERATED_BODY()

public:
	/** Interhaptics haptic effect JSON: the verbatim .haps contents. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	FString Json;

	/** Effect name this asset was imported from (e.g. "Effect1"). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	FString SourceName;
};
