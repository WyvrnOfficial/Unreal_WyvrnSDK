// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Shared log category for the PS5 haptics path (Interhaptics runtime + backend).
// Declared once here so the runtime and backend translation units share a single
// category instead of each defining a file-static one (which collides in unity builds).
DECLARE_LOG_CATEGORY_EXTERN(LogWyvrnHaptics, Log, All);

#if defined(PLATFORM_PS5) && PLATFORM_PS5

// Runtime toggle for the verbose per-event trace that follows a WYVRN event all the
// way down the PS5 path: game-thread SetEventName -> backend -> render-worker queue
// -> command lookup -> voice playback. OFF by default. Backed by the console variable
// "wyvrn.Haptics.VerboseLog" (defined in InterhapticsHapticBackend.cpp); flip it at
// runtime on the devkit console, or preset it in a config's [ConsoleVariables] section:
//     wyvrn.Haptics.VerboseLog 1
// Safe to query from any thread (the game thread enqueues, the worker drains).
bool WyvrnHapticsVerboseLoggingEnabled();

// Guarded trace. When the toggle is off this is a single cheap branch and logs nothing.
// Forwards straight to UE_LOG, so use it exactly like UE_LOG's (Format, ...) arguments.
#define WYVRN_HAPTIC_TRACE(...) \
	do { \
		if (WyvrnHapticsVerboseLoggingEnabled()) \
		{ \
			UE_LOG(LogWyvrnHaptics, Log, ##__VA_ARGS__); \
		} \
	} while (0)

#else

// Non-PS5 (e.g. the cross-platform voice-pool unit tests): compile the traces out entirely.
#define WYVRN_HAPTIC_TRACE(...) do {} while (0)

#endif // PLATFORM_PS5
