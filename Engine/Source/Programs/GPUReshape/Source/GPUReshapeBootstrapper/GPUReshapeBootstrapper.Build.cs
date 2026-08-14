// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GPUReshapeBootstrapper : ModuleRules
{
	public GPUReshapeBootstrapper(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("Projects");
	}
}