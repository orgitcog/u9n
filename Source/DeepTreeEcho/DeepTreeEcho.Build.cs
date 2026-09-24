// Copyright Deep Tree Echo. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DeepTreeEcho : ModuleRules
{
	public DeepTreeEcho(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		string DeepTreeEchoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../DeepTreeEcho"));
		string ReservoirEchoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ReservoirEcho"));

		// Nest-1 root (forwarding headers + Core/Self/...) plus 9 nest-4 terms
		PublicIncludePaths.AddRange(
			new string[] {
				DeepTreeEchoRoot,
				Path.Combine(DeepTreeEchoRoot, "Core"),
				Path.Combine(DeepTreeEchoRoot, "Self"),
				Path.Combine(DeepTreeEchoRoot, "Self", "EchoSelf"),
				Path.Combine(DeepTreeEchoRoot, "Reservoir"),
				Path.Combine(DeepTreeEchoRoot, "Cognition4E"),
				Path.Combine(DeepTreeEchoRoot, "Memory"),
				Path.Combine(DeepTreeEchoRoot, "Attention"),
				Path.Combine(DeepTreeEchoRoot, "Streams"),
				Path.Combine(DeepTreeEchoRoot, "Enaction"),
				Path.Combine(DeepTreeEchoRoot, "Wisdom"),
				Path.Combine(DeepTreeEchoRoot, "Testing"),
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(ReservoirEchoRoot, "reservoircpp_cpp/include"),
				Path.Combine(ReservoirEchoRoot, "external/eigen-3.4.0"),
				Path.Combine(ReservoirEchoRoot, "external/taskflow-3.8.0"),
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
			}
		);

		// Nest sources live under ../../DeepTreeEcho. UBT only compiles this
		// directory, so EchoSelf / Autognosis / Core are pulled in via thin
		// wrappers (EchoSelfIntegration.cpp, DeepTreeEchoCore.cpp, …).

		bEnableExceptions = true;
		bUseUnity = false;

		PublicDefinitions.Add("DEEPTREEECHO_API=");
		PublicDefinitions.Add("EIGEN_MPL2_ONLY");
	}
}
