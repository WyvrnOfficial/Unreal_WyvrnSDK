// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Minimal, ABI-compatible mirror of the Interhaptics HAR target types
 * (Interhaptics::HapticBodyMapping in the HAR SDK's SharedTypes.h).
 *
 * The PS5 backend resolves HAR entry points from HAR.prx at runtime via
 * sceKernelDlsym rather than linking the SDK, so it only needs the binary layout
 * of the structures it passes across the module boundary - not the SDK headers
 * themselves (which are third-party/closed and not vendored into this repo).
 *
 * The enum underlying type (int) and the field order of FCommandData MUST match
 * the SDK exactly; the values below are copied verbatim from SharedTypes.h.
 */
namespace WyvrnSDK
{
	namespace HAR
	{
		// Interhaptics::HapticBodyMapping::Operator
		enum class EOperator : int32
		{
			Minus = -1,
			Neutral = 0,
			Plus = 1,
		};

		// Interhaptics::HapticBodyMapping::LateralFlag
		enum class ELateralFlag : int32
		{
			Unknown = -1,
			Global = 0,
			Right = 1,
			Left = 2,
			Center = 3,
		};

		// Interhaptics::HapticBodyMapping::GroupID (only the group this backend targets).
		enum class EGroupID : int32
		{
			Hand = 302,
		};

		// Interhaptics::HapticBodyMapping::CommandData { Operator Sign; GroupID Group; LateralFlag Side; }
		struct FCommandData
		{
			EOperator Sign;
			EGroupID Group;
			ELateralFlag Side;
		};

		static_assert(sizeof(FCommandData) == 12, "FCommandData must stay layout-compatible with HAR CommandData (3 x int32).");
	}
}
