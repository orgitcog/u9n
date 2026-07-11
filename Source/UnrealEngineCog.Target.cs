// Copyright Deep Tree Echo. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealEngineCogTarget : TargetRules
{
	public UnrealEngineCogTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;

		ExtraModuleNames.AddRange(
			new string[]
			{
				"DeepTreeEcho",
				"UnrealEcho",
				"DeepTreeEchoAvatar",
			}
		);
	}
}
