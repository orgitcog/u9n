// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextAnimGraphTestSuite : ModuleRules
	{
		public AnimNextAnimGraphTestSuite(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AnimNext",
					"AnimNextTestSuite",
					"AnimNextAnimGraph",
					"RigVM", 
					"PythonScriptPlugin",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"UnrealEd",
						"AnimNextUncookedOnly",
						"AnimNextEditor",
						"RigVMDeveloper",
						"AnimNextAnimGraphEditor",
						"AnimNextAnimGraphUncookedOnly",
					}
				);
			}
		}
	}
}