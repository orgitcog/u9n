// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ADMSpatialization : ModuleRules
	{
		public ADMSpatialization(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AudioExtensions",
					"Core",
					"CoreUObject",
					"DeveloperSettings",
					"Engine",
					"Networking",
					"OSC"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AudioMixerCore",
					"AudioMixer",
					"SignalProcessing"
				}
			);
		}
	}
}
