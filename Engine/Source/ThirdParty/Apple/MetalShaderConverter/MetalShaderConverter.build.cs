// Copyright (C) 2022 Apple Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class MetalShaderConverter : ModuleRules
{
	public MetalShaderConverter(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string SourcePath = Path.Combine(Target.UEThirdPartySourceDirectory,"Apple", "MetalShaderConverter");
		string IncludePath = Path.Combine(SourcePath, "include");
		string BinariesPath = Path.Combine(Target.UEThirdPartyBinariesDirectory, "Apple", "MetalShaderConverter");

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicIncludePaths.Add(Path.Combine(IncludePath, "common"));
			PublicIncludePaths.Add(Path.Combine(IncludePath, "mac"));
			
			string DylibPath = Path.Combine(BinariesPath, "Mac", "libmetalirconverter.dylib");
			PublicAdditionalLibraries.Add(DylibPath);
			RuntimeDependencies.Add(DylibPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add(Path.Combine(IncludePath, "common"));
			PublicAdditionalLibraries.Add(Path.Combine(SourcePath, "lib", "metalirconverter.lib"));

			string DynamicLibName = "metalirconverter.dll";
			RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", DynamicLibName), Path.Combine(BinariesPath, "Windows", DynamicLibName));
		}
	}
}
