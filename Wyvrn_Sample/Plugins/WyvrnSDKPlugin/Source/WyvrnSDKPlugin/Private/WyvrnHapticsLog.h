// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Shared log category for the PS5 haptics path (Interhaptics runtime + backend).
// Declared once here so the runtime and backend translation units share a single
// category instead of each defining a file-static one (which collides in unity builds).
DECLARE_LOG_CATEGORY_EXTERN(LogWyvrnHaptics, Log, All);
