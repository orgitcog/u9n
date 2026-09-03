// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class DatasmithMax2026Target : DatasmithMaxBaseTarget
{
	public DatasmithMax2026Target(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "DatasmithMax2026";
		ExeBinariesSubFolder = @"3DSMax\2026";

		AddCopyPostBuildStep(Target);
	}
}
