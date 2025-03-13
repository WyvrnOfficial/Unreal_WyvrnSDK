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

};
