// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IntelOIDN : ModuleRules
{
    public IntelOIDN(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && Target.Architecture.bIsX64)
        {
			PublicDependencyModuleNames.Add("IntelTBB");

			string SDKDir = Target.UEThirdPartySourceDirectory + "Intel/OIDN/";

			PublicSystemIncludePaths.Add(SDKDir + "include/");
            PublicSystemLibraryPaths.Add(SDKDir + "lib/");
            PublicAdditionalLibraries.Add(SDKDir + "lib/OpenImageDenoise.lib");
			PublicAdditionalLibraries.Add(SDKDir + "lib/OpenImageDenoise_core.lib");
			RuntimeDependencies.Add("$(TargetOutputDir)/OpenImageDenoise.dll"           , SDKDir + "bin/OpenImageDenoise.dll");
			RuntimeDependencies.Add("$(TargetOutputDir)/OpenImageDenoise_core.dll"      , SDKDir + "bin/OpenImageDenoise_core.dll");
			RuntimeDependencies.Add("$(TargetOutputDir)/OpenImageDenoise_device_cpu.dll", SDKDir + "bin/OpenImageDenoise_device_cpu.dll");

			PublicDelayLoadDLLs.Add("OpenImageDenoise.dll");
			PublicDelayLoadDLLs.Add("OpenImageDenoise_core.dll");
			PublicDelayLoadDLLs.Add("OpenImageDenoise_device_cpu.dll");
			PublicDefinitions.Add("WITH_INTELOIDN=1");
        }
		else
		{
			PublicDefinitions.Add("WITH_INTELOIDN=0");
		}
    }
}
