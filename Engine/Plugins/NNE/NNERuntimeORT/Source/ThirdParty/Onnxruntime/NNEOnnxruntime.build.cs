// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

public class NNEOnnxruntime : ModuleRules
{
	public NNEOnnxruntime(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Internal"));
		
		string OrtPlatformRelativePath = Path.Combine("Binaries", "ThirdParty", "Onnxruntime");
		string SharedLibFileName = "UNSUPPORTED_PLATFORM";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			OrtPlatformRelativePath = Path.Combine(OrtPlatformRelativePath, Target.Architecture == UnrealArch.Arm64 ? "WinArm64" : "Win64");
			string OrtPlatformPath = Path.Combine(PluginDirectory, OrtPlatformRelativePath);

			SharedLibFileName = "onnxruntime.dll";

			PublicDelayLoadDLLs.Add(SharedLibFileName);
			
			RuntimeDependencies.Add(Path.Combine(OrtPlatformPath, SharedLibFileName));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.LinuxArm64)
		{
			OrtPlatformRelativePath = Path.Combine(OrtPlatformRelativePath, Target.Platform.ToString());
			string OrtPlatformPath = Path.Combine(PluginDirectory, OrtPlatformRelativePath);

			SharedLibFileName = "libonnxruntime.so.1.20";
			string SharedLibFileNameWithoutVersion = "libonnxruntime.so";
			
			PublicDelayLoadDLLs.Add(Path.Combine(OrtPlatformPath, SharedLibFileNameWithoutVersion));
			PublicDelayLoadDLLs.Add(Path.Combine(OrtPlatformPath, SharedLibFileName));

			RuntimeDependencies.Add(Path.Combine(OrtPlatformPath, SharedLibFileNameWithoutVersion));
			RuntimeDependencies.Add(Path.Combine(OrtPlatformPath, SharedLibFileName));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			OrtPlatformRelativePath = Path.Combine(OrtPlatformRelativePath, Target.Platform.ToString());
			string OrtPlatformPath = Path.Combine(PluginDirectory, OrtPlatformRelativePath);

			SharedLibFileName = "libonnxruntime.1.20.dylib";
			string SharedLibFileNameWithoutVersion = "libonnxruntime.dylib";

			PublicDelayLoadDLLs.Add(Path.Combine(OrtPlatformPath, SharedLibFileNameWithoutVersion));
			PublicDelayLoadDLLs.Add(Path.Combine(OrtPlatformPath, SharedLibFileName));

			RuntimeDependencies.Add(Path.Combine(OrtPlatformPath, SharedLibFileNameWithoutVersion));
			RuntimeDependencies.Add(Path.Combine(OrtPlatformPath, SharedLibFileName));
		}

		PublicDefinitions.Add("UE_ORT_USE_INLINE_NAMESPACE = 1");
		PublicDefinitions.Add("UE_ORT_INLINE_NAMESPACE_NAME = Ort01201");

		if (Target.Type == TargetType.Editor)
		{
			bEnableExceptions = true;
			bDisableAutoRTFMInstrumentation = true;
		}
		else
		{
			PublicDefinitions.Add("ORT_NO_EXCEPTIONS");
		}

		PublicDefinitions.Add("ORT_API_MANUAL_INIT");
		PublicDefinitions.Add("ONNXRUNTIME_SHAREDLIB_PATH=" + Path.Combine(OrtPlatformRelativePath, SharedLibFileName).Replace('\\', '/'));
	}
}