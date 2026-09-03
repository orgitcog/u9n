// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class astcenc : ModuleRules
{
	protected string DynamicLibNamePrefix;
	protected string DynamicLibNameSuffix;

	protected void SetDynamicLibNameStrings()
	{
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			DynamicLibNamePrefix = "astcenc_thunk_win64_";
			DynamicLibNameSuffix = ".dll";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			DynamicLibNamePrefix = "libastcenc_thunk_osx64_";
			DynamicLibNameSuffix = ".dylib";
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			DynamicLibNamePrefix = "libastcenc_thunk_linux64_";
			DynamicLibNameSuffix = ".so";
		}
		else
		{
			throw new BuildException("Platform {0} not supported in astcec.", Target.Platform);
		}

		PublicDefinitions.Add("ASTCENC_DLL_PREFIX=\"" + DynamicLibNamePrefix + "\"");
		PublicDefinitions.Add("ASTCENC_DLL_SUFFIX=\"" + DynamicLibNameSuffix + "\"");
	}
	
	protected void AddDynamicLibsForVersion(string Version)
	{
		string DynamicLibName = DynamicLibNamePrefix + Version + DynamicLibNameSuffix;
		string FullDynamicLibName = Path.Combine(ModuleDirectory, "Thunks", DynamicLibName);

		if (!File.Exists(FullDynamicLibName))
		{
			throw new BuildException("Platform {0} can't find dynamic lib for astcenc: {1}", Target.Platform, FullDynamicLibName);
		}

		FullDynamicLibName = "$(EngineDir)/" + UnrealBuildTool.Utils.MakePathRelativeTo(FullDynamicLibName, EngineDirectory);

		RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", DynamicLibName), FullDynamicLibName, StagedFileType.NonUFS);
	}

	public astcenc(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add("WITH_ASTC_ENCODER=1");

		PublicSystemIncludePaths.Add(ModuleDirectory);
		
		SetDynamicLibNameStrings();
		
		AddDynamicLibsForVersion("4.2.0");
		AddDynamicLibsForVersion("5.0.1");
	}
}

