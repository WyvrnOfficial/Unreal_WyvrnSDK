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
 * Lateral side of a target region, mirroring the WYVRN.config Targeting
 * "Spatialization". On the DualSense, Global drives both palms, Left/Right one.
 * Maps to the HAR ELateralFlag (Global/Left/Right) at play time.
 */
UENUM(BlueprintType)
enum class EWyvrnHapticSide : uint8
{
	Global,
	Left,
	Right
};

/**
 * Playback priority from the WYVRN.config "Priority". Five levels mirroring the
 * authoring tool. A live event silences every lower-priority event for its
 * duration (priority ducking), and drives voice stealing when an effect's voice
 * pool is saturated.
 */
UENUM(BlueprintType)
enum class EWyvrnHapticPriority : uint8
{
	VeryLow,
	Low,
	Medium,
	High,
	VeryHigh
};

/**
 * How an event mixes against other live events of equal priority, from the
 * WYVRN.config "Mixing". Merge plays them together (additive); Override makes the
 * newer event play alone and silences the older equal-priority one until it ends.
 */
UENUM(BlueprintType)
enum class EWyvrnHapticMixing : uint8
{
	Merge,
	Override
};

/**
 * One body-region target of a haptic event with its lateral side (a WYVRN.config
 * Targeting entry). On PS5 only the hand region is rendered.
 */
USTRUCT(BlueprintType)
struct WYVRNSDKPLUGIN_API FWyvrnHapticTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	EWyvrnHapticTarget Region = EWyvrnHapticTarget::Hand;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	EWyvrnHapticSide Side = EWyvrnHapticSide::Global;

	bool operator==(const FWyvrnHapticTarget& Other) const
	{
		return Region == Other.Region && Side == Other.Side;
	}
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

	/** Mixing against other live equal-priority events (WYVRN.config Mixing). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	EWyvrnHapticMixing Mixing = EWyvrnHapticMixing::Merge;

	/** Body regions (with side) this effect targets. Translated to HAR targets at play time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	TArray<FWyvrnHapticTarget> Targets;
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

	/** Literal event names whose playback stops when this command fires (array form of Interrupts_Commands). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	TArray<FString> InterruptCommands;

	/** Stop every active event when this command fires (the bare-string "All" form of Interrupts_Commands). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WyvrnSDK|Haptics")
	bool bInterruptAll = false;
};
