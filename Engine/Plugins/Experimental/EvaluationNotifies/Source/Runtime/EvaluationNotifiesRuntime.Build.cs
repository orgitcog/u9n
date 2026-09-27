// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class EvaluationNotifiesRuntime : ModuleRules
	{
		public EvaluationNotifiesRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add(Path.Combine(EngineDirectory,"Source/ThirdParty/AHEasing/AHEasing-1.3.2"));
			
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimationCore",
                "AnimGraphRuntime",
                "Core",
                "CoreUObject",
                "Engine",
                "AnimationWarpingRuntime",
                "RigVM",
                "AnimNext",
                "AnimNextAnimGraph", 
            });
		}
	}
}
