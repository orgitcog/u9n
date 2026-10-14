using UnrealBuildTool;
using System.IO;

public class DTEAvatarBridge : ModuleRules
{
    public DTEAvatarBridge(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        // Header-only, UE-independent mapping helpers. This is intentionally private
        // to keep the public plugin API free of standard-library implementation types.
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Portable"));
    }
}
