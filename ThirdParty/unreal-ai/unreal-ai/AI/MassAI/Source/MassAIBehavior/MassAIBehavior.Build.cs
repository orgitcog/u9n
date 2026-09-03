// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MassAIBehavior : ModuleRules
	{
		public MassAIBehavior(ReadOnlyTargetRules Target) : base(Target)
		{
			CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Warning;

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AIModule"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"MassEntity",
					"Core",
					"CoreUObject",
					"Engine",
					"DeveloperSettings",
					"GameplayTags",
					"MassActors",
					"MassCommon",
					"MassLOD",
					"MassMovement",
					"MassNavigation",
					"MassNavMeshNavigation",
					"MassZoneGraphNavigation",
					"MassRepresentation",
					"MassSignals",
					"MassSmartObjects",
					"MassSpawner",
					"MassSimulation",
					"NavigationSystem",
					"SmartObjectsModule",
					"StateTreeModule",
					"ZoneGraph",
					"ZoneGraphAnnotations",
					"MassGameplayExternalTraits",
					"NavCorridor"
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("EditorFramework");
				PrivateDependencyModuleNames.Add("UnrealEd");
			}
		}
	}
}
