// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnHapticMaterialFactory.h"

#include "WyvrnHapticMaterial.h"

UWyvrnHapticMaterialFactory::UWyvrnHapticMaterialFactory()
{
	bCreateNew = false;
	bEditorImport = true;
	bText = true;
	SupportedClass = UWyvrnHapticMaterial::StaticClass();
	Formats.Add(TEXT("haps;Interhaptics Haptic Material"));
}

UObject* UWyvrnHapticMaterialFactory::FactoryCreateText(
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

	UWyvrnHapticMaterial* Material = NewObject<UWyvrnHapticMaterial>(InParent, InClass, InName, Flags);
	Material->Json.AppendChars(Buffer, static_cast<int32>(BufferEnd - Buffer));
	Material->SourceName = InName.ToString();

	return Material;
}
