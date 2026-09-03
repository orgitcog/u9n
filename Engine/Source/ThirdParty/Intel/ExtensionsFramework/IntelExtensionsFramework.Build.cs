// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class IntelExtensionsFramework : ModuleRules
{
	public IntelExtensionsFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && Target.Architecture.bIsX64)
		{
			string ThirdPartyDir = Path.Combine(Target.UEThirdPartySourceDirectory, "Intel", "ExtensionsFramework");
			string IncludeDir = ThirdPartyDir;
			string LibrariesDir = ThirdPartyDir;

			// Forcing Intel Extension off due to a crash related to enabling the extension after CL 38886087
			PublicDefinitions.Add("INTEL_EXTENSIONS=1");
			PublicDefinitions.Add("INTEL_GPU_CRASH_DUMPS=1");
			PublicDefinitions.Add("INTEL_BREADCRUMBS_USE_BREADCRUMB_PTRS=1");

			PublicSystemIncludePaths.Add(IncludeDir);
			PublicAdditionalLibraries.Add(Path.Combine(LibrariesDir, "igdext64.lib"));
		}
		else
		{
			PublicDefinitions.Add("INTEL_EXTENSIONS=0");
			PublicDefinitions.Add("INTEL_GPU_CRASH_DUMPS=0");
			PublicDefinitions.Add("INTEL_BREADCRUMBS_USE_BREADCRUMB_PTRS=0");
		}
	}
}