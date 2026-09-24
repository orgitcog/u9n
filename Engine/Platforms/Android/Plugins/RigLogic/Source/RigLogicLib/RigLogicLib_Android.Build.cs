// Copyright Epic Games, Inc. All Rights Reserved.
namespace UnrealBuildTool.Rules
{
	public class RigLogicLib_Android : RigLogicLib
	{
		public RigLogicLib_Android(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDefinitions.Add("RL_AUTODETECT_SSE");
			PrivateDefinitions.Add("RL_AUTODETECT_NEON");
			PrivateDefinitions.Add("RL_AUTODETECT_HALF_FLOATS");
		}
	}
}