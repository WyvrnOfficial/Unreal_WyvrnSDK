// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

/**
 * CPU profiling group for the PS5 Interhaptics backend. In a Development build,
 * view it with `stat WyvrnHaptics` (per-frame ms for the tick and HAR render, the
 * per-trigger dispatch cost, and the live voice count) or in Unreal Insights with
 * -statnamedevents. Compiles out in Test/Shipping (STATS == 0), so no ship cost.
 */
DECLARE_STATS_GROUP(TEXT("WyvrnHaptics"), STATGROUP_WyvrnHaptics, STATCAT_Advanced);
