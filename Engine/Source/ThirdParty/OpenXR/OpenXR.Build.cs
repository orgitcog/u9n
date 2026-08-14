// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class OpenXR : ModuleRules
{
	protected string RootPath { get => Target.UEThirdPartySourceDirectory + "OpenXR"; }
	protected string LoaderPath { get => RootPath + "/loader"; }

	public OpenXR(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(RootPath + "/include");
		PublicDefinitions.Add("XR_NO_PROTOTYPES");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string PlatformDir = (Target.Architecture == UnrealArch.Arm64) ? "WinArm64" : "win64";
			PublicAdditionalLibraries.Add(Path.Combine(LoaderPath, PlatformDir, "openxr_loader.lib"));

			PublicDelayLoadDLLs.Add("openxr_loader.dll");
			RuntimeDependencies.Add(Path.Combine("$(EngineDir)/Binaries/ThirdParty/OpenXR", PlatformDir, "openxr_loader.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string OpenXRPath = "$(EngineDir)/Binaries/ThirdParty/OpenXR/linux/x86_64-unknown-linux-gnu/libopenxr_loader.so";

			PublicAdditionalLibraries.Add(OpenXRPath);
			RuntimeDependencies.Add(OpenXRPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// toolchain will filter
			string[] Architectures = new string[] {
				"arm64-v8a",
				"armeabi-v7a",
				"x86",
				"x86_64",
			};

			string BasePath = "$(EngineDir)/Binaries/ThirdParty/OpenXR/Android/";
			foreach (string Architecture in Architectures)
			{
				string OpenXRPath = Path.Combine(BasePath, Architecture, "libopenxr_loader.so");
				PublicAdditionalLibraries.Add(OpenXRPath);
				RuntimeDependencies.Add(OpenXRPath);
			}

			AdditionalPropertiesForReceipt.Add("AndroidPlugin", "Source/ThirdParty/OpenXR/OpenXR_APL.xml");
		}
	}
}
