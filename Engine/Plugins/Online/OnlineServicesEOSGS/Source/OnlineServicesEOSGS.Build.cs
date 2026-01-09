// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineServicesEOSGS : ModuleRules
{
	public OnlineServicesEOSGS(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"EOSSDK",
				"OnlineServicesInterface",
				"OnlineServicesEpicCommon",
				"OnlineServicesCommon",
				"OnlineServicesCommonEngineUtils",
				"OnlineBase",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreOnline",
				"CoreUObject",
				"EOSShared",
				"Sockets"
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"SocketSubsystemEOS"
				}
			);
		}

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicWeakFrameworks.Add("AuthenticationServices");
			PrivateDependencyModuleNames.Add("ApplicationCore");
		}
	}
}
