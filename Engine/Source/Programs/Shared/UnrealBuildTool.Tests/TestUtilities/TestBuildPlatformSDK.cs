// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool.Tests.TestUtilities
{
	internal class TestSDK : UEBuildPlatformSDK
	{
		public TestSDK(ILogger InLogger) : base(InLogger)
		{
		}

		public override bool TryConvertVersionToInt(string? StringValue, out ulong OutValue, string? Hint = null)
		{
			OutValue = 1;
			return true;
		}

		protected override string? GetInstalledSDKVersion()
		{
			return "1.0.0";
		}
	}
}
