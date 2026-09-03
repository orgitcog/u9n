// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using UnrealBuildTool;
using System;
using System.Linq;
using Microsoft.Extensions.Logging;

public class BuildSettings : ModuleRules
{
	public BuildSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		CppCompileWarningSettings.DeterministicWarningLevel = WarningLevel.Off; // This module intentionally uses __DATE__ and __TIME__ macros
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PrivateIncludePathModuleNames.Add("Core");

		bRequiresImplementModule = false;

		PrivateDefinitions.Add($"ENGINE_VERSION_MAJOR={Target.Version.MajorVersion}");
		PrivateDefinitions.Add($"ENGINE_VERSION_MINOR={Target.Version.MinorVersion}");
		PrivateDefinitions.Add($"ENGINE_VERSION_HOTFIX={Target.Version.PatchVersion}");
		PrivateDefinitions.Add($"ENGINE_VERSION_STRING=\"{Target.Version.MajorVersion}.{Target.Version.MinorVersion}.{Target.Version.PatchVersion}-{Target.BuildVersion}\"");
		PrivateDefinitions.Add($"ENGINE_IS_LICENSEE_VERSION={(Target.Version.IsLicenseeVersion ? "true" : "false")}");
		PrivateDefinitions.Add($"ENGINE_IS_PROMOTED_BUILD={(Target.Version.IsPromotedBuild ? "true" : "false")}");
		
		if (!Target.GlobalDefinitions.Any(x => x.Contains("CURRENT_CHANGELIST", StringComparison.Ordinal)))
		{
			PrivateDefinitions.Add($"CURRENT_CHANGELIST={Target.Version.Changelist}");
		}

		PrivateDefinitions.Add($"COMPATIBLE_CHANGELIST={Target.Version.EffectiveCompatibleChangelist}");
		PrivateDefinitions.Add($"BRANCH_NAME=\"{Target.Version.BranchName}\"");
		PrivateDefinitions.Add($"BUILD_VERSION=\"{Target.BuildVersion}\"");
		PrivateDefinitions.Add($"BUILD_SOURCE_URL=\"{Target.Version.BuildURL}\"");

		string userName = string.Empty;
		string userDomainName = string.Empty;
		string machineName = string.Empty;
		if (Target.bEnablePrivateBuildInformation)
		{
			userName = Environment.UserName;
			userDomainName = Environment.UserDomainName;
			machineName = UnrealBuildBase.Unreal.MachineName;
		}
		PrivateDefinitions.Add($"BUILD_USER=\"{userName}\"");
		PrivateDefinitions.Add($"BUILD_USERDOMAINNAME=\"{userDomainName}\"");
		PrivateDefinitions.Add($"BUILD_MACHINENAME=\"{machineName}\"");

		PrivateDefinitions.Add("SUPPRESS_PER_MODULE_INLINE_FILE"); // This module does not use core's standard operator new/delete overloads
		
		if (Target.bWithLiveCoding && Target.LinkType == TargetLinkType.Monolithic)
		{
			PrivateDefinitions.Add(String.Format("UE_LIVE_CODING_ENGINE_DIR=\"{0}\"", UnrealBuildBase.Unreal.EngineDirectory.FullName.Replace("\\", "\\\\")));
			if (Target.ProjectFile != null)
			{
				PrivateDefinitions.Add(String.Format("UE_LIVE_CODING_PROJECT=\"{0}\"", Target.ProjectFile.FullName.Replace("\\", "\\\\")));
			}
		}

		PrivateDefinitions.Add("UE_PERSISTENT_ALLOCATOR_RESERVE_SIZE=" + GetPersistentAllocatorReserveSize().ToString() + "ULL");

		if (Target.bBuildEditor)
		{
			bDisableAutoRTFMInstrumentation = true;
		}
	}

	private ulong GetPersistentAllocatorReserveSize()
	{
		// We have to separate persistent allocator reserve into Editor and Engine as some platforms, like iOS have very limited VM by default, so we can't reserve a lot of vm just in case
		ConfigHierarchyType ConfigHierarchy = Target.Type == TargetType.Editor ? ConfigHierarchyType.Editor : ConfigHierarchyType.Engine;
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchy, Target.BuildEnvironment == TargetBuildEnvironment.Unique ? DirectoryReference.FromFile(Target.ProjectFile) : null, Target.Platform);
		int SizeOfPermanentObjectPool = 0;
		if (Ini.GetInt32("/Script/Engine.GarbageCollectionSettings", "gc.SizeOfPermanentObjectPool", out SizeOfPermanentObjectPool))
		{
			Target.Logger.LogWarning("/Script/Engine.GarbageCollectionSettings, gc.SizeOfPermanentObjectPool ini for Project {0} setting was deprecated in a favor of MemoryPools, PersistentAllocatorReserveSizeMB", Target.ProjectFile.ToString());
		}
		int PersistentAllocatorReserveSizeMB = 0;
		Ini.GetInt32("MemoryPools", "PersistentAllocatorReserveSizeMB", out PersistentAllocatorReserveSizeMB);
		return (ulong)PersistentAllocatorReserveSizeMB * 1024 * 1024;
	}
}
