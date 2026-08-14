// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using EpicGames.Core;

namespace UnrealBuildTool
{
	class VisionOSToolChainSettings : IOSToolChainSettings
	{
		public VisionOSToolChainSettings(IOSProjectSettings ProjectSettings, ILogger Logger) 
			: base("XROS", "XRSimulator", "xros", ProjectSettings, Logger)
		{
		}

		public virtual string RuntimeVersion
		{
			get
			{
				return UEBuildPlatformSDK.GetSDKForPlatform("VisionOS")!.GetSoftwareInfo()!.Min!;
			}
		}
	}

	class VisionOSToolChain : IOSToolChain
	{
		public VisionOSToolChain(ReadOnlyTargetRules InTarget, VisionOSProjectSettings InProjectSettings, ILogger InLogger)
			: base(InTarget, InProjectSettings, () => new VisionOSToolChainSettings(InProjectSettings, InLogger), ClangToolChainOptions.None, InLogger)
		{
		}
	}
}
