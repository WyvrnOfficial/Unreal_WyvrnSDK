// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Wyvrn_SampleEditorTarget : TargetRules
{
//	public Wyvrn_SampleEditorTarget(TargetInfo Target) //___HACK_UE4_VERSION_4_15_OR_LESS
    public Wyvrn_SampleEditorTarget(TargetInfo Target) : base(Target) //___HACK_UE4_VERSION_4_16_OR_GREATER
    {
#if UE_5_4_OR_LATER
        DefaultBuildSettings = BuildSettingsVersion.Latest;
#endif

        Type = TargetType.Editor;

        ExtraModuleNames.AddRange(new string[] { "Wyvrn_Sample" }); //___HACK_UE4_VERSION_4_16_OR_GREATER
    }

	//
	// TargetRules interface.
	//

//	public override void SetupBinaries( //___HACK_UE4_VERSION_4_15_OR_LESS
//        TargetInfo Target, //___HACK_UE4_VERSION_4_15_OR_LESS
//        ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations, //___HACK_UE4_VERSION_4_15_OR_LESS
//        ref List<string> OutExtraModuleNames //___HACK_UE4_VERSION_4_15_OR_LESS
//        ) //___HACK_UE4_VERSION_4_15_OR_LESS
//    { //___HACK_UE4_VERSION_4_15_OR_LESS
//        OutExtraModuleNames.AddRange( new string[] { "Wyvrn_Sample" } ); //___HACK_UE4_VERSION_4_15_OR_LESS
//    } //___HACK_UE4_VERSION_4_15_OR_LESS
}
