// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "Engine.h"
#include "UMG.h"
#include "SampleGameWyvrnBP.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogWyvrnSampleGame, Log, All);


UCLASS()
class USampleGameWyvrnBP : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameSampleStart", Keywords = "Init at the start of the application"), Category = "Sample")
	static void SampleGameSampleStart();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameSampleEnd", Keywords = "Uninit at the end of the application"), Category = "Sample")
	static void SampleGameSampleEnd();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect1", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect1();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect2", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect2();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect3", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect3();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect4", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect4();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect5", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect5();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect6", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect6();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect7", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect7();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect8", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect8();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect9", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect9();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect10", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect10();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect11", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect11();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect12", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect12();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect13", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect13();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect14", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect14();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SampleGameShowEffect15", Keywords = "Example"), Category = "Sample")
	static void SampleGameShowEffect15();

	// --- PS5 haptics behaviour test harness ---
	// Each fires a Debug_* WYVRN.config command (bake the debug WYVRN.config and
	// wire these to on-screen buttons). See the test procedure for press sequences.

	/** Loop effect A on both palms (High). Case 1/3/4/5. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Loop A"), Category = "Sample|Debug")
	static void SampleGameDebugLoopA();

	/** Loop effect B on both palms (High). Case 2/3. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Loop B"), Category = "Sample|Debug")
	static void SampleGameDebugLoopB();

	/** Stop only Loop A. Case 1/3/5. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Stop A"), Category = "Sample|Debug")
	static void SampleGameDebugStopA();

	/** Stop every active effect ("All" sentinel). Case 2; resets between tests. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Stop All"), Category = "Sample|Debug")
	static void SampleGameDebugStopAll();

	/** Loop on both palms (Spatialization Global). Case 7. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Hand Global"), Category = "Sample|Debug")
	static void SampleGameDebugHandGlobal();

	/** Loop on the left palm only (Spatialization Left). Case 7. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Hand Left"), Category = "Sample|Debug")
	static void SampleGameDebugHandLeft();

	/** Loop on the right palm only (Spatialization Right). Case 7. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Hand Right"), Category = "Sample|Debug")
	static void SampleGameDebugHandRight();

	/** Loop a low-priority effect. Press first for case 6a. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Priority Low (loop)"), Category = "Sample|Debug")
	static void SampleGameDebugPriorityLow();

	/** One-shot high-priority effect; ducks the low loop, then it resumes. Case 6a. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Priority High (once)"), Category = "Sample|Debug")
	static void SampleGameDebugPriorityHigh();

	/** Loop an equal-priority (Medium) baseline. Press first for case 6b/6c. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Equal A (loop)"), Category = "Sample|Debug")
	static void SampleGameDebugEqualA();

	/** One-shot equal-priority Merge effect; plays together with Equal A. Case 6c. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Merge B (once)"), Category = "Sample|Debug")
	static void SampleGameDebugMergeB();

	/** One-shot equal-priority Override effect; silences Equal A, then it resumes. Case 6b. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Debug: Override B (once)"), Category = "Sample|Debug")
	static void SampleGameDebugOverrideB();

};
