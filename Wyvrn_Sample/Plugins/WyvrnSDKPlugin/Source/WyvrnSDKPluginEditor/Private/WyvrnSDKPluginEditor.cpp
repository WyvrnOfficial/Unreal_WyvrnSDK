// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "HAL/IConsoleManager.h"
#include "IDesktopPlatform.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"

#include "WyvrnConfigImporter.h"
#include "WyvrnConfigParser.h"
#include "WyvrnHapticData.h"
#include "WyvrnImportSettings.h"

#define LOCTEXT_NAMESPACE "WyvrnSDKPluginEditor"

namespace
{
	bool PickHapticFolder(const FString& DefaultPath, FString& OutPath)
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (DesktopPlatform == nullptr)
		{
			return false;
		}
		const void* ParentWindow = FSlateApplication::IsInitialized()
			? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
			: nullptr;
		return DesktopPlatform->OpenDirectoryDialog(ParentWindow, TEXT("Select the WYVRN haptic folder"), DefaultPath, OutPath);
	}

	// Menu entry: discover the folder, let the user confirm or re-pick, then import.
	void RunHapticImportInteractive()
	{
		const UWyvrnImportSettings* Settings = GetDefault<UWyvrnImportSettings>();
		FString Folder = Settings->GetDiscoveredFolder();

		while (true)
		{
			const EAppReturnType::Type Answer = FMessageDialog::Open(
				EAppMsgType::YesNoCancel,
				FText::Format(
					LOCTEXT("ConfirmFolder", "WYVRN haptics will be imported from:\n\n{0}\n\nUse this folder?\nYes = import   No = pick another   Cancel = abort"),
					FText::FromString(Folder)));

			if (Answer == EAppReturnType::Cancel)
			{
				return;
			}
			if (Answer == EAppReturnType::Yes)
			{
				break;
			}

			FString Picked;
			if (PickHapticFolder(Folder, Picked) && !Picked.IsEmpty())
			{
				Folder = Picked;
			}
		}

		// A previous import already baked assets here: confirm before overwriting.
		const FString DataPackageName = FPaths::Combine(Settings->OutputContentPath, TEXT("WyvrnHapticData"));
		if (FPackageName::DoesPackageExist(DataPackageName))
		{
			const EAppReturnType::Type Overwrite = FMessageDialog::Open(
				EAppMsgType::YesNo,
				FText::Format(
					LOCTEXT("ConfirmOverwrite", "WYVRN haptics already exist at:\n\n{0}\n\nReimport and overwrite them?"),
					FText::FromString(Settings->OutputContentPath)));
			if (Overwrite != EAppReturnType::Yes)
			{
				return;
			}
		}

		FString Error;
		const UWyvrnHapticData* Data = FWyvrnConfigImporter::ImportFromFolder(Folder, Settings->OutputContentPath, Error);
		const FText Message = Data != nullptr
			? FText::Format(LOCTEXT("ImportOk", "Imported {0} WYVRN command(s) to {1}."), FText::AsNumber(Data->Commands.Num()), FText::FromString(Settings->OutputContentPath))
			: FText::Format(LOCTEXT("ImportFailed", "WYVRN haptics import failed:\n\n{0}"), FText::FromString(Error));
		FMessageDialog::Open(EAppMsgType::Ok, Message);
	}

	// Headless trigger for build pipelines / quick iteration: Wyvrn.ImportHaptics [folder]
	void RunHapticImportConsole(const TArray<FString>& Args)
	{
		const UWyvrnImportSettings* Settings = GetDefault<UWyvrnImportSettings>();
		const FString Folder = Args.Num() > 0 ? Args[0] : Settings->GetDiscoveredFolder();

		FString Error;
		const UWyvrnHapticData* Data = FWyvrnConfigImporter::ImportFromFolder(Folder, Settings->OutputContentPath, Error);
		if (Data != nullptr)
		{
			UE_LOG(LogWyvrnImport, Log, TEXT("Wyvrn.ImportHaptics: baked %d command(s) from '%s'."), Data->Commands.Num(), *Folder);
		}
		else
		{
			UE_LOG(LogWyvrnImport, Error, TEXT("Wyvrn.ImportHaptics: %s"), *Error);
		}
	}

	FAutoConsoleCommand GImportHapticsCommand(
		TEXT("Wyvrn.ImportHaptics"),
		TEXT("Bakes PS5 haptics from a WYVRN folder. Optional arg: source folder (defaults to the WYVRN Haptics Import setting)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RunHapticImportConsole));
}

class FWyvrnSDKPluginEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWyvrnSDKPluginEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);

		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		if (ToolsMenu == nullptr)
		{
			return;
		}

		FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("WYVRN"), LOCTEXT("WyvrnSection", "WYVRN"));
		Section.AddMenuEntry(
			TEXT("WyvrnImportHaptics"),
			LOCTEXT("ImportHapticsLabel", "Import WYVRN Haptics..."),
			LOCTEXT("ImportHapticsTooltip", "Bake PS5 haptics from a WYVRN.config folder."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&RunHapticImportInteractive)));
	}
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWyvrnSDKPluginEditorModule, WyvrnSDKPluginEditor)
