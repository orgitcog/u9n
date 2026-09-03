// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ControlRigPhysics : ModuleRules
{
	public ControlRigPhysics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"PhysicsControl"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ControlRig",
				"Core",
				"CoreUObject",
				"Engine",
				"RigVM"
			}
		);

		SetupModulePhysicsSupport(Target);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"RigVMDeveloper",
					"ControlRigDeveloper",
					"Slate",
					"SlateCore",
					"EditorStyle",
				}
			);
		}
	}
}
