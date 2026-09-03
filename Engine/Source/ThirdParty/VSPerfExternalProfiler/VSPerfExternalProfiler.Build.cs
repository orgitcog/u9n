// Copyright Epic Games, Inc. All Rights Reserved.
using EpicGames.Core;
using UnrealBuildTool;
using System;
using System.IO;

public class VSPerfExternalProfiler : ModuleRules
{
	public VSPerfExternalProfiler(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// We on purpose is not adding any public include paths since we don't want to affect core's rsp/definition files

		PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "Lib", Target.Architecture.WindowsLibDir, "VSPerfExternalProfiler.lib"));

	}
}
