// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnHapticEffectFactory.h"

#include "WyvrnHapticEffect.h"

UWyvrnHapticEffectFactory::UWyvrnHapticEffectFactory()
{
	bCreateNew = false;
	bEditorImport = true;
	bText = true;
	SupportedClass = UWyvrnHapticEffect::StaticClass();
	Formats.Add(TEXT("haps;Interhaptics Haptic Effect"));
}

UObject* UWyvrnHapticEffectFactory::FactoryCreateText(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	UObject* Context,
	const TCHAR* Type,
	const TCHAR*& Buffer,
	const TCHAR* BufferEnd,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	UWyvrnHapticEffect* HapticEffect = NewObject<UWyvrnHapticEffect>(InParent, InClass, InName, Flags);
	HapticEffect->Json.AppendChars(Buffer, static_cast<int32>(BufferEnd - Buffer));
	HapticEffect->SourceName = InName.ToString();

	return HapticEffect;
}
