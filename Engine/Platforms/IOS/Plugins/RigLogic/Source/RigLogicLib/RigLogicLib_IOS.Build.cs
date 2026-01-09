// Copyright Epic Games, Inc. All Rights Reserved.
namespace UnrealBuildTool.Rules
{
	public class RigLogicLib_IOS : RigLogicLib
	{
		public RigLogicLib_IOS(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDefinitions.Add("RL_AUTODETECT_NEON");
			PrivateDefinitions.Add("RL_AUTODETECT_HALF_FLOATS");
		}
	}
}