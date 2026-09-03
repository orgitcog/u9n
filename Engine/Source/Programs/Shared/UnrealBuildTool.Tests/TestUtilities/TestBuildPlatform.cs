// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool.Tests.TestUtilities
{
	internal class TestBuildPlatfrom : UEBuildPlatform
	{
		public TestBuildPlatfrom(UnrealTargetPlatform InPlatform, UEBuildPlatformSDK SDK, UnrealArchitectureConfig ArchitectureConfig, ILogger InLogger) : base(InPlatform, SDK, ArchitectureConfig, InLogger)
		{

		}
		public override UEToolChain CreateToolChain(ReadOnlyTargetRules Target)
		{
			throw new System.NotImplementedException();
		}

		public override void Deploy(TargetReceipt Receipt)
		{

		}

		public override bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return false;
		}

		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{

		}

		public override bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target)
		{
			return false;
		}
	}
}
