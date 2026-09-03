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
		string DeepTreeEchoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../DeepTreeEcho"));
		string ReservoirEchoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ReservoirEcho"));

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
				// DeepTreeEcho-root-relative includes used by the AI controller
				// (e.g. "UnrealBridge/CognitiveActionArbiter.h", "Core/DeepTreeEchoCore.h")
				DeepTreeEchoRoot,
				// Eigen — required by CognitiveActionArbiter.h; DeepTreeEcho keeps
				// it in PrivateIncludePaths, which does not propagate here.
				Path.Combine(ReservoirEchoRoot, "external/eigen-3.4.0"),
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

		// Eigen requires exception support
		bEnableExceptions = true;

		// Compiler definitions
		PublicDefinitions.Add("DEEPTREEECHOAVATAR_API=");
	}
}
