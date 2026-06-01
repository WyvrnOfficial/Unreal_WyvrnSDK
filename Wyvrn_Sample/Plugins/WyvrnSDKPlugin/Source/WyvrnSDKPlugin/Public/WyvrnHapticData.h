// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WyvrnHapticTypes.h"
#include "WyvrnHapticData.generated.h"

/**
 * Baked, cooked-in haptics for a title: the controller-relevant slice of a
 * WYVRN.config plus references to its imported materials.
 *
 * Produced at editor time by the WyvrnSDKPluginEditor importer and consumed at
 * runtime by the Interhaptics backend. This is the contract between the two
 * systems of the PS5 port.
 */
UCLASS(BlueprintType)
class WYVRNSDKPLUGIN_API UWyvrnHapticData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Every WYVRN event that carries haptics. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	TArray<FWyvrnHapticCommand> Commands;

	/** WYVRN app / folder this data was baked from. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	FString SourceAppName;

	/** Returns the command for EventName, or nullptr if the event carries no haptics. */
	const FWyvrnHapticCommand* FindCommand(const FString& EventName) const;
};
