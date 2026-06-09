// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WyvrnHapticTypes.generated.h"

class UWyvrnHapticEffect;

/**
 * Body region a haptic event addresses, mirroring the WYVRN.config "Target".
 * On PS5 the DualSense provider renders only the hand region; the others are
 * ignored.
 */
UENUM(BlueprintType)
enum class EWyvrnHapticTarget : uint8
{
	Head,
	Hand,
	Chest,
	Waist,
	Leg
};

/**
 * Playback priority from the WYVRN.config "Priority". Drives voice stealing when
 * the haptic voice pool is saturated.
 */
UENUM(BlueprintType)
enum class EWyvrnHapticPriority : uint8
{
	Low,
	Medium,
	High
};

/**
 * One haptic event triggered by a command (a WYVRN.config Haptic_Event): the
 * Interhaptics effect asset plus its playback parameters.
 */
USTRUCT(BlueprintType)
struct WYVRNSDKPLUGIN_API FWyvrnHapticEvent
{
	GENERATED_BODY()

	/** Interhaptics haptic effect (one .haps). Its JSON is handed to HAR AddHM at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	UWyvrnHapticEffect* Effect = nullptr;

	/** Intensity factor forwarded to SetEventIntensity. HAR clamps it above 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	float Gain = 1.0f;

	/** Loop count forwarded to SetEventLoop: 0/1 = one shot, < 0 = infinite. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	int32 Loop = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	EWyvrnHapticPriority Priority = EWyvrnHapticPriority::High;

	/** Body regions this effect targets. Translated to HAR targets at play time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	TArray<EWyvrnHapticTarget> Targets;
};

/**
 * A named WYVRN event (External_Command_ID) and the haptics it triggers.
 */
USTRUCT(BlueprintType)
struct WYVRNSDKPLUGIN_API FWyvrnHapticCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	FString EventName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	TArray<FWyvrnHapticEvent> Effects;

	/** Events whose playback stops when this command fires (WYVRN.config Interrupts_Commands). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	TArray<FString> InterruptCommands;
};
