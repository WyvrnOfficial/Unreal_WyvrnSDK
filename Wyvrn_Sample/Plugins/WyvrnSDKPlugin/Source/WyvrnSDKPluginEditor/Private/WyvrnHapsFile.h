// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Inspects .haps (Interhaptics haptic effect) JSON at import time. The runtime
 * hands the .haps text to HAR verbatim, so anything the plugin needs to know
 * about an effect's content is detected here and baked onto the asset.
 */
class FWyvrnHapsFile
{
public:
	/**
	 * True when the .haps carries an authored Stiffness track: an "m_stiffness"
	 * block with at least one melody holding at least one note. Returns false
	 * for malformed JSON.
	 */
	static bool HasStiffnessTrack(const FString& HapsJson);
};
