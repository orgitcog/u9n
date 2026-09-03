// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{

	[SupportedPlatforms("Win64", "Mac")]
	public class DatasmithSketchUpRuby2025 : DatasmithSketchUpRubyBase
	{
		public DatasmithSketchUpRuby2025(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PrivateDefinitions.Add("SKP_SDK_2025");
		}

		public override string GetSketchUpSDKFolder()
		{
			return "SDK_WIN_x64_2025-0-575";
		}

		public override string GetSketchUpEnvVar()
		{
			return "SKP_SDK_2025";
		}
		public override string GetRubyLibName()
		{
			return "x64-ucrt-ruby320.lib";
		}
	}
}
