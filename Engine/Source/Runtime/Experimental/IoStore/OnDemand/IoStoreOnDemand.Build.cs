// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IoStoreOnDemand : ModuleRules
{
	public IoStoreOnDemand(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.Add("Core");
		PublicDependencyModuleNames.Add("IoStoreOnDemandCore");
		PublicDependencyModuleNames.Add("TraceLog");
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Analytics",
				"CoreUObject",
				"IoStoreHttpClient",
				"Json",
			}
		);

		bAllowConfidentialPlatformDefines = true;
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error; 

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop) &&
			(Target.Type == TargetType.Editor || Target.Type == TargetType.Program))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "S3Client", "RSA" });
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDefinitions.Add("UE_HTTPCLIENT_THREADSAFE_CHECKS=1");
		}
		else
		{
			PrivateDefinitions.Add("UE_HTTPCLIENT_THREADSAFE_CHECKS=0");
		}
	}
}
