// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class portmidi : ModuleRules
{
	public portmidi(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Target.UEThirdPartySourceDirectory + "portmidi/include");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
			string PlatformSubdir = Target.Architecture == UnrealArch.Arm64 ? "WinArm64" : "Win64";
            PublicAdditionalLibraries.Add(Target.UEThirdPartySourceDirectory + $"portmidi/lib/{PlatformSubdir}/portmidi_64.lib");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add(Target.UEThirdPartySourceDirectory + "portmidi/lib/Mac/libportmidi.a");
			PublicFrameworks.Add("CoreAudio");
			PublicFrameworks.Add("CoreMIDI");
        }
	}
}
