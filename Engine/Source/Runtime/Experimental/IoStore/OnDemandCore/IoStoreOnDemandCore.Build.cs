// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IoStoreOnDemandCore : ModuleRules
{
	public IoStoreOnDemandCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"Json"
			}
		);

		bAllowConfidentialPlatformDefines = true;
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error; 
	}
}
