// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnConfigImporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#include "Settings/ProjectPackagingSettings.h"

#include "WyvrnConfigParser.h"
#include "WyvrnHapsFile.h"
#include "WyvrnHapticData.h"
#include "WyvrnHapticEffect.h"

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

	// Ensure the importer's output content path is force-cooked. The baked haptic
	// data is loaded by path at runtime and referenced by nothing, so the cooker
	// would otherwise skip it. Adding it to ProjectPackagingSettings here means any
	// project that imports through this plugin gets it packaged without editing
	// config by hand. Idempotent, and persisted to the project's DefaultGame.ini.
	void EnsureAlwaysCookDirectory(const FString& ContentPath)
	{
		FString PackagePath = ContentPath;
		PackagePath.RemoveFromEnd(TEXT("/"));
		if (PackagePath.IsEmpty())
		{
			return;
		}

		UProjectPackagingSettings* Settings = GetMutableDefault<UProjectPackagingSettings>();
		const bool bAlreadyListed = Settings->DirectoriesToAlwaysCook.ContainsByPredicate(
			[&PackagePath](const FDirectoryPath& Dir) { return Dir.Path == PackagePath; });
		if (bAlreadyListed)
		{
			return;
		}

		FDirectoryPath NewDirectory;
		NewDirectory.Path = PackagePath;
		Settings->DirectoriesToAlwaysCook.Add(NewDirectory);

		// Persist to the project's DefaultGame.ini. TryUpdateDefaultConfigFile writes
		// the whole settings object; UpdateSinglePropertyInConfigFile can't be used
		// here - it rejects array-of-struct properties like DirectoriesToAlwaysCook.
		if (Settings->TryUpdateDefaultConfigFile())
		{
			UE_LOG(LogWyvrnImport, Log, TEXT("WyvrnConfigImporter: added '%s' to DirectoriesToAlwaysCook so the baked haptics cook into packaged builds."), *PackagePath);
		}
		else
		{
			UE_LOG(LogWyvrnImport, Warning, TEXT("WyvrnConfigImporter: could not persist DirectoriesToAlwaysCook to DefaultGame.ini; add '%s' manually."), *PackagePath);
		}
	}

	// Reads <SourceFolder>/<EffectName>.haps and saves it as a UWyvrnHapticEffect.
	// Returns nullptr (OutError empty) when the file is simply absent, so the
	// caller can skip that effect; OutError is set only on a hard failure.
	UWyvrnHapticEffect* ImportHapticEffect(const FString& SourceFolder, const FString& HapticEffectPath, const FString& EffectName, FString& OutError)
	{
		const FString HapsPath = FPaths::Combine(SourceFolder, EffectName + TEXT(".haps"));
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *HapsPath))
		{
			UE_LOG(LogWyvrnImport, Warning, TEXT("WyvrnConfigImporter: missing '%s'; skipping effect '%s'."), *HapsPath, *EffectName);
			return nullptr;
		}

		const FString PackageName = FPaths::Combine(HapticEffectPath, EffectName);
		UPackage* Package = CreatePackage(*PackageName);
		if (Package == nullptr)
		{
			OutError = FString::Printf(TEXT("WyvrnConfigImporter: could not create package '%s'."), *PackageName);
			return nullptr;
		}
		// Fully load any existing on-disk asset so SavePackage doesn't reject a
		// partially-loaded package, then reuse the object instead of colliding with it.
		Package->FullyLoad();

		UWyvrnHapticEffect* HapticEffect = FindObject<UWyvrnHapticEffect>(Package, *EffectName);
		if (HapticEffect == nullptr)
		{
			HapticEffect = NewObject<UWyvrnHapticEffect>(Package, FName(*EffectName), RF_Public | RF_Standalone);
		}
		HapticEffect->bHasStiffnessTrack = FWyvrnHapsFile::HasStiffnessTrack(Json);
		HapticEffect->Json = MoveTemp(Json);
		HapticEffect->SourceName = EffectName;
		if (HapticEffect->bHasStiffnessTrack)
		{
			UE_LOG(LogWyvrnImport, Log, TEXT("WyvrnConfigImporter: effect '%s' carries a Stiffness track; its events will drive the DualSense adaptive triggers."), *EffectName);
		}

		FAssetRegistryModule::AssetCreated(HapticEffect);
		Package->MarkPackageDirty();

		if (!SaveAssetPackage(Package, HapticEffect, OutError))
		{
			return nullptr;
		}
		return HapticEffect;
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

	const FString HapticEffectPath = FPaths::Combine(OutputContentPath, TEXT("HapticEffect"));
	const FString DataPackageName = FPaths::Combine(OutputContentPath, TEXT("WyvrnHapticData"));

	UPackage* DataPackage = CreatePackage(*DataPackageName);
	if (DataPackage == nullptr)
	{
		OutError = FString::Printf(TEXT("WyvrnConfigImporter: could not create package '%s'."), *DataPackageName);
		return nullptr;
	}
	// Fully load any existing data asset so SavePackage doesn't reject a
	// partially-loaded package, then reuse and clear it on reimport.
	DataPackage->FullyLoad();

	UWyvrnHapticData* Data = FindObject<UWyvrnHapticData>(DataPackage, TEXT("WyvrnHapticData"));
	if (Data == nullptr)
	{
		Data = NewObject<UWyvrnHapticData>(DataPackage, FName(TEXT("WyvrnHapticData")), RF_Public | RF_Standalone);
	}
	Data->Commands.Reset();

	FString NormalizedSource = SourceFolder;
	FPaths::NormalizeDirectoryName(NormalizedSource);
	Data->SourceAppName = FPaths::GetCleanFilename(NormalizedSource);

	TMap<FString, UWyvrnHapticEffect*> EffectsByName;

	for (const FWyvrnParsedCommand& ParsedCommand : ParsedCommands)
	{
		FWyvrnHapticCommand Command;
		Command.EventName = ParsedCommand.EventName;
		Command.InterruptCommands = ParsedCommand.InterruptCommands;
		Command.bInterruptAll = ParsedCommand.bInterruptAll;

		for (const FWyvrnParsedEffect& ParsedEffect : ParsedCommand.Effects)
		{
			TArray<FWyvrnHapticTarget> Targets;
			float Gain = 1.0f;
			bool bHasControllerTarget = false;
			for (const FWyvrnParsedTargeting& Targeting : ParsedEffect.Targeting)
			{
				if (IsDualSenseTarget(Targeting.Target))
				{
					// Preserve the lateral side; dedup on the (region, side) pair so
					// Hand-Left and Hand-Right both survive but duplicates collapse.
					FWyvrnHapticTarget Target;
					Target.Region = Targeting.Target;
					Target.Side = Targeting.Side;
					Targets.AddUnique(Target);
					// HAR intensity is per-event (one material id per effect+target-set), so an
					// event carries a single Gain: take the first controller target's. Per-side
					// gains (e.g. Left 0.5 / Right 1.0) collapse to this one value.
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

			UWyvrnHapticEffect** Cached = EffectsByName.Find(ParsedEffect.EffectName);
			UWyvrnHapticEffect* HapticEffect = Cached != nullptr
				? *Cached
				: ImportHapticEffect(SourceFolder, HapticEffectPath, ParsedEffect.EffectName, OutError);
			if (HapticEffect == nullptr)
			{
				if (!OutError.IsEmpty())
				{
					return nullptr;
				}
				continue;
			}
			EffectsByName.Add(ParsedEffect.EffectName, HapticEffect);

			FWyvrnHapticEvent EffectEntry;
			EffectEntry.Effect = HapticEffect;
			EffectEntry.Gain = Gain;
			EffectEntry.Loop = ParsedEffect.Loop;
			EffectEntry.Priority = ParsedEffect.Priority;
			EffectEntry.Mixing = ParsedEffect.Mixing;
			EffectEntry.Targets = MoveTemp(Targets);
			Command.Effects.Add(MoveTemp(EffectEntry));
		}

		if (Command.Effects.Num() > 0 || Command.InterruptCommands.Num() > 0 || Command.bInterruptAll)
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

	EnsureAlwaysCookDirectory(OutputContentPath);

	UE_LOG(LogWyvrnImport, Log, TEXT("WyvrnConfigImporter: baked %d command(s) from '%s'."), Data->Commands.Num(), *SourceFolder);
	return Data;
}
