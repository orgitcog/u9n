// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	[SupportedPlatforms("Win64")]
	public class DatasmithNavisworks2026 : DatasmithNavisworksBase
	{
		public DatasmithNavisworks2026(ReadOnlyTargetRules Target)
			: base(Target)
		{
		}

		public override string GetNavisworksVersion()
		{
			return "2026";
		}
	}
}
