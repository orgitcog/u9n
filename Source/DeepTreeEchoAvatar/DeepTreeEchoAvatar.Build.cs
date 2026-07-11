// Copyright Deep Tree Echo. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DeepTreeEchoAvatar : ModuleRules
{
	public DeepTreeEchoAvatar(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// SECURITY NOTE: This module intentionally does NOT use PreBuildSteps or
		// PostBuildSteps. Any addition of build steps requires explicit security
		// review per SECURITY.md policy. See issue #603.

		// Avatar module source roots
		string UnrealEchoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../UnrealEcho"));

		// Public API surface: DeepTreeEchoAvatar headers
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(UnrealEchoRoot, "DeepTreeEchoAvatar/Public"),
			}
		);

		// Private implementation files
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(UnrealEchoRoot, "DeepTreeEchoAvatar/Private"),
			}
		);

		// Core Unreal Engine dependencies
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"AIModule",          // For AAIController / UAIPerceptionComponent
				"DeepTreeEcho",      // Cognitive architecture
				"UnrealEcho",        // Avatar / animation layer
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
			}
		);

		// Optional editor-only dependencies
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
				}
			);
		}

		// Disable unity builds for better debugging
		bUseUnity = false;

		// Compiler definitions
		PublicDefinitions.Add("DEEPTREEECHOAVATAR_API=");
	}
}
