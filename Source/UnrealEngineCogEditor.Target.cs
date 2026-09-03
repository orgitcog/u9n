// Copyright Deep Tree Echo. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealEngineCogEditorTarget : TargetRules
{
	public UnrealEngineCogEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
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
