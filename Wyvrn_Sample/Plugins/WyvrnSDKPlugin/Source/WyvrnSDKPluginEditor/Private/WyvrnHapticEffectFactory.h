// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "WyvrnHapticEffectFactory.generated.h"

/**
 * Imports Interhaptics .haps files into UWyvrnHapticEffect assets, storing the
 * file's JSON text verbatim so the runtime backend can hand it to HAR AddHM.
 */
UCLASS()
class UWyvrnHapticEffectFactory : public UFactory
{
	GENERATED_BODY()

public:
	UWyvrnHapticEffectFactory();

	virtual UObject* FactoryCreateText(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		UObject* Context,
		const TCHAR* Type,
		const TCHAR*& Buffer,
		const TCHAR* BufferEnd,
		FFeedbackContext* Warn,
		bool& bOutOperationCanceled) override;
};
