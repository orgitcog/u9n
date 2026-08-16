// EchoSelf.Build.cs — deprecated sidecar.
// EchoSelf compiles as part of the DeepTreeEcho module (DeepTreeEcho/Self/EchoSelf/).
// Do not add this module to UnrealEngineCog.uproject.

using UnrealBuildTool;

public class EchoSelf : ModuleRules
{
    public EchoSelf(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeepTreeEcho" });
    }
}
