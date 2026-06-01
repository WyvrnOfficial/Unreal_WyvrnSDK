// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnConfigImporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#include "WyvrnConfigParser.h"
#include "WyvrnHapticData.h"
#include "WyvrnHapticMaterial.h"

namespace
{
	// The DualSense provider renders the hand region (left + right actuators);
	// other body targets in the config produce nothing on PS5 and are dropped.
	bool IsDualSenseTarget(EWyvrnHapticTarget Target)
	{
		return Target == EWyvrnHapticTarget::Hand;
	}

	bool SaveAssetPackage(UPackage* Package, UObject* Asset, FString& OutError)
	{
		const FString PackageName = Package->GetName();
		const FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_None;

		if (!UPackage::SavePackage(Package, Asset, *FileName, SaveArgs))
		{
			OutError = FString::Printf(TEXT("WyvrnConfigImporter: failed to save package '%s'."), *PackageName);
			return false;
		}
		return true;
	}

	// Reads <SourceFolder>/<EffectName>.haps and saves it as a UWyvrnHapticMaterial.
	// Returns nullptr (OutError empty) when the file is simply absent, so the
	// caller can skip that effect; OutError is set only on a hard failure.
	UWyvrnHapticMaterial* ImportMaterial(const FString& SourceFolder, const FString& MaterialsPath, const FString& EffectName, FString& OutError)
	{
		const FString HapsPath = FPaths::Combine(SourceFolder, EffectName + TEXT(".haps"));
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *HapsPath))
		{
			UE_LOG(LogWyvrnImport, Warning, TEXT("WyvrnConfigImporter: missing '%s'; skipping effect '%s'."), *HapsPath, *EffectName);
			return nullptr;
		}

		const FString PackageName = FPaths::Combine(MaterialsPath, EffectName);
		UPackage* Package = CreatePackage(*PackageName);
		if (Package == nullptr)
		{
			OutError = FString::Printf(TEXT("WyvrnConfigImporter: could not create package '%s'."), *PackageName);
			return nullptr;
		}

		UWyvrnHapticMaterial* Material = NewObject<UWyvrnHapticMaterial>(Package, FName(*EffectName), RF_Public | RF_Standalone);
		Material->Json = MoveTemp(Json);
		Material->SourceName = EffectName;

		FAssetRegistryModule::AssetCreated(Material);
		Package->MarkPackageDirty();

		if (!SaveAssetPackage(Package, Material, OutError))
		{
			return nullptr;
		}
		return Material;
	}
}

UWyvrnHapticData* FWyvrnConfigImporter::ImportFromFolder(const FString& SourceFolder, const FString& OutputContentPath, FString& OutError)
{
	const FString ConfigPath = FPaths::Combine(SourceFolder, TEXT("WYVRN.config"));
	FString ConfigJson;
	if (!FFileHelper::LoadFileToString(ConfigJson, *ConfigPath))
	{
		OutError = FString::Printf(TEXT("WyvrnConfigImporter: could not read '%s'."), *ConfigPath);
		return nullptr;
	}

	TArray<FWyvrnParsedCommand> ParsedCommands;
	if (!FWyvrnConfigParser::Parse(ConfigJson, ParsedCommands, OutError))
	{
		return nullptr;
	}

	const FString MaterialsPath = FPaths::Combine(OutputContentPath, TEXT("Materials"));
	const FString DataPackageName = FPaths::Combine(OutputContentPath, TEXT("WyvrnHapticData"));

	UPackage* DataPackage = CreatePackage(*DataPackageName);
	if (DataPackage == nullptr)
	{
		OutError = FString::Printf(TEXT("WyvrnConfigImporter: could not create package '%s'."), *DataPackageName);
		return nullptr;
	}

	UWyvrnHapticData* Data = NewObject<UWyvrnHapticData>(DataPackage, FName(TEXT("WyvrnHapticData")), RF_Public | RF_Standalone);

	FString NormalizedSource = SourceFolder;
	FPaths::NormalizeDirectoryName(NormalizedSource);
	Data->SourceAppName = FPaths::GetCleanFilename(NormalizedSource);

	TMap<FString, UWyvrnHapticMaterial*> MaterialsByEffect;

	for (const FWyvrnParsedCommand& ParsedCommand : ParsedCommands)
	{
		FWyvrnHapticCommand Command;
		Command.EventName = ParsedCommand.EventName;
		Command.InterruptCommands = ParsedCommand.InterruptCommands;

		for (const FWyvrnParsedEffect& ParsedEffect : ParsedCommand.Effects)
		{
			TArray<EWyvrnHapticTarget> Targets;
			float Gain = 1.0f;
			bool bHasControllerTarget = false;
			for (const FWyvrnParsedTargeting& Targeting : ParsedEffect.Targeting)
			{
				if (IsDualSenseTarget(Targeting.Target))
				{
					Targets.AddUnique(Targeting.Target);
					if (!bHasControllerTarget)
					{
						Gain = Targeting.Gain;
						bHasControllerTarget = true;
					}
				}
			}

			if (!bHasControllerTarget)
			{
				continue;
			}

			UWyvrnHapticMaterial** Cached = MaterialsByEffect.Find(ParsedEffect.EffectName);
			UWyvrnHapticMaterial* Material = Cached != nullptr
				? *Cached
				: ImportMaterial(SourceFolder, MaterialsPath, ParsedEffect.EffectName, OutError);
			if (Material == nullptr)
			{
				if (!OutError.IsEmpty())
				{
					return nullptr;
				}
				continue;
			}
			MaterialsByEffect.Add(ParsedEffect.EffectName, Material);

			FWyvrnHapticEffect Effect;
			Effect.Material = Material;
			Effect.Gain = Gain;
			Effect.Loop = ParsedEffect.Loop;
			Effect.Priority = ParsedEffect.Priority;
			Effect.Targets = MoveTemp(Targets);
			Command.Effects.Add(MoveTemp(Effect));
		}

		if (Command.Effects.Num() > 0 || Command.InterruptCommands.Num() > 0)
		{
			Data->Commands.Add(MoveTemp(Command));
		}
	}

	FAssetRegistryModule::AssetCreated(Data);
	DataPackage->MarkPackageDirty();
	if (!SaveAssetPackage(DataPackage, Data, OutError))
	{
		return nullptr;
	}

	UE_LOG(LogWyvrnImport, Log, TEXT("WyvrnConfigImporter: baked %d command(s) from '%s'."), Data->Commands.Num(), *SourceFolder);
	return Data;
}
