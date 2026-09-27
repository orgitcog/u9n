// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RuntimeTelemetry : ModuleRules
	{
		public RuntimeTelemetry(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Engine",
					"IoStoreOnDemandCore"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
					"Analytics",
					"StudioTelemetry"
				}
			);
		}
	}
}
