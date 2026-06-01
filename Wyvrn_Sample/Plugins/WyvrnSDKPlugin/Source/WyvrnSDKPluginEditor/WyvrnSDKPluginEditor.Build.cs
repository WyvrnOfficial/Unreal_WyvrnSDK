// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class WyvrnSDKPluginEditor : ModuleRules
	{
		public WyvrnSDKPluginEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

#if UE_5_4_OR_LATER
			DefaultBuildSettings = BuildSettingsVersion.Latest;
#endif

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"WyvrnSDKPlugin"
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Json",
					"JsonUtilities",
					"UnrealEd",
					"AssetTools",
					"AssetRegistry",
					"Projects",
					"DeveloperSettings",
					"ToolMenus",
					"Slate",
					"SlateCore",
					"DesktopPlatform"
				}
				);
		}
	}
}
