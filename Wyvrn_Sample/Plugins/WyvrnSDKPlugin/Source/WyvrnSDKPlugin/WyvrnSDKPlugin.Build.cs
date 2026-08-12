// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class WyvrnSDKPlugin : ModuleRules
	{
//		public WyvrnSDKPlugin(TargetInfo Target) //___HACK_UE4_VERSION_4_15_OR_LESS
		public WyvrnSDKPlugin(ReadOnlyTargetRules Target) : base(Target) //___HACK_UE4_VERSION_4_16_OR_GREATER
		{
            // https://answers.unrealengine.com/questions/51798/how-can-i-enable-unwind-semantics-for-c-style-exce.html
//            UEBuildConfiguration.bForceEnableExceptions = true;//___HACK_UE4_VERSION_4_9_TO_VERSION_4_15
			
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs; //___HACK_UE4_VERSION_4_16_OR_GREATER

#if UE_5_4_OR_LATER
			DefaultBuildSettings = BuildSettingsVersion.Latest;
#endif

            //PrivateDefinitions.Add("NO_CHECK_WYVRNSDK_LIBRARY_SIGNATURE=1");
            //PublicDefinitions.Add("NO_CHECK_WYVRNSDK_LIBRARY_SIGNATURE=1");

            PublicIncludePaths.AddRange(
				new string[] {
                    ModuleDirectory + "/Public",
					// ... add public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
                    ModuleDirectory + "/Private",
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "CoreUObject",
                    "InputCore",
                    "Engine",
                    "Projects",
					// ... add private dependencies that you statically link with here ...
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);

			// On PS5 the haptics backend (InterhapticsRuntime) loads the Interhaptics HAR
			// engine + DualSense provider PRX at runtime. The PRX are built separately from
			// the Interhaptics HAR repo and dropped into Source/ThirdParty/Interhaptics/PS5/bin.
			// Stage them next to the title (loaded from /app0/sce_module/ - keep this in sync
			// with the paths in InterhapticsRuntime.cpp). If the binaries are not present the
			// runtime stays inert, so the title still builds and packages without them.
			// Compare by name rather than UnrealTargetPlatform.PS5: that member only exists
			// when the PS5 platform extension is installed, so referencing it directly breaks
			// this .Build.cs from even compiling on engine versions without PS5 support.
			if (Target.Platform.ToString() == "PS5")
			{
				string PrxBinDir = Path.Combine(ModuleDirectory, "..", "ThirdParty", "Interhaptics", "PS5", "bin");

				// Link the HAR + provider import stubs when present. The weak stubs let the
				// title launch (and the runtime stay inert) when the PRX are absent, so the
				// libraries stay optional to build. WITH_INTERHAPTICS_HAR gates the calls.
				string HarStub = Path.Combine(PrxBinDir, "HAR_stub_weak.a");
				string ProviderStub = Path.Combine(PrxBinDir, "Provider_DualSensePS5_stub_weak.a");
				if (File.Exists(HarStub) && File.Exists(ProviderStub))
				{
					// Link the import stubs (regular + weak) so the HAR/provider entry points resolve.
					foreach (string Stub in new string[] { "HAR_stub.a", "Provider_DualSensePS5_stub.a", "HAR_stub_weak.a", "Provider_DualSensePS5_stub_weak.a" })
					{
						string StubPath = Path.Combine(PrxBinDir, Stub);
						if (File.Exists(StubPath))
						{
							PublicAdditionalLibraries.Add(StubPath);
						}
					}

					// Delay-load the PRX (bind on load at runtime, not as launch-time NEEDED deps)
					// and stage them in-place; the runtime loads them via FPlatformProcess::GetDllHandle
					// and the loader resolves them on demand - so they don't need to sit in the
					// package-root /app0/sce_module/.
					foreach (string Prx in new string[] { "HAR.prx", "Provider_DualSensePS5.prx" })
					{
						string PrxPath = Path.Combine(PrxBinDir, Prx);
						if (File.Exists(PrxPath))
						{
							PublicDelayLoadDLLs.Add(Prx);
							RuntimeDependencies.Add(PrxPath);
						}
					}

					PublicDefinitions.Add("WITH_INTERHAPTICS_HAR=1");
				}
				else
				{
					PublicDefinitions.Add("WITH_INTERHAPTICS_HAR=0");
				}
			}

        }
	}
}
