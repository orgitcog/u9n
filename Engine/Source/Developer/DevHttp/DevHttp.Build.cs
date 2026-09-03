// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DevHttp : ModuleRules
{
	public DevHttp(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "SSL" });
		
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && Target.WindowsPlatform.bUseXCurl)
		{
			PublicDependencyModuleNames.Add("XCurl");
		}
		else
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");
		}
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
	}
}
