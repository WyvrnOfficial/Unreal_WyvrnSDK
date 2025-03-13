// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WyvrnSDKPluginTypes.generated.h"

USTRUCT(BlueprintType)
struct WYVRNSDKPLUGIN_API FWyvrnSDKAppInfoType
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WyvrnSDK")
	FString Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WyvrnSDK")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WyvrnSDK")
	FString Author_Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WyvrnSDK")
	FString Author_Contact;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WyvrnSDK")
	//int32 SupportedDevice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WyvrnSDK")
	int32 Category;
};
