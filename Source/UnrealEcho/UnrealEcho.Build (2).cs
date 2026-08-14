// Copyright Deep Tree Echo. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealEcho : ModuleRules
{
	public UnrealEcho(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// SECURITY NOTE: This module intentionally does NOT use PreBuildSteps or
		// PostBuildSteps. Any addition of build steps requires explicit security
		// review per SECURITY.md policy. See issue #603.

		// UnrealEcho source directories (relative to repository root)
		string UnrealEchoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../UnrealEcho"));
		string DeepTreeEchoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../DeepTreeEcho"));

		// Include paths for UnrealEcho components
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(UnrealEchoRoot, "Animation"),
				Path.Combine(UnrealEchoRoot, "AssetManagement"),
				Path.Combine(UnrealEchoRoot, "Audio"),
				Path.Combine(UnrealEchoRoot, "Avatar"),
				Path.Combine(UnrealEchoRoot, "Character"),
				Path.Combine(UnrealEchoRoot, "Consciousness"),
				Path.Combine(UnrealEchoRoot, "Cosmetics"),
				Path.Combine(UnrealEchoRoot, "DeepTreeEchoAvatar/Public"),
				Path.Combine(UnrealEchoRoot, "Environment"),
				Path.Combine(UnrealEchoRoot, "Interaction"),
				Path.Combine(UnrealEchoRoot, "Live2DCubism"),
				Path.Combine(UnrealEchoRoot, "Narrative"),
				Path.Combine(UnrealEchoRoot, "NeuralNetwork"),
				Path.Combine(UnrealEchoRoot, "Neurochemical"),
				Path.Combine(UnrealEchoRoot, "Patterns"),
				Path.Combine(UnrealEchoRoot, "Performance"),
				Path.Combine(UnrealEchoRoot, "Personality"),
				Path.Combine(UnrealEchoRoot, "Rendering"),
				Path.Combine(UnrealEchoRoot, "Testing"),
				Path.Combine(UnrealEchoRoot, "Tests"),
			}
		);

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
				"UMG",
				"AnimGraphRuntime",
				"DeepTreeEcho",  // Cognitive architecture dependency
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"RenderCore",
				"RHI",
				"Renderer",
				"Live2DCubismFramework",  // Cubism SDK dependency
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
		PublicDefinitions.Add("UNREALECHO_API=");
	}
}
