// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MoverAnimNext : ModuleRules
{
	public MoverAnimNext(ReadOnlyTargetRules Target) : base(Target)
	{

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Mover",
				"AnimNext",
				"PoseSearch"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"RigVM",
				"Engine",
				"PhysicsCore",
				"DeveloperSettings"
			}
			);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"RigVMDeveloper"
				}
			);
		}

		SetupGameplayDebuggerSupport(Target);
	}
}
