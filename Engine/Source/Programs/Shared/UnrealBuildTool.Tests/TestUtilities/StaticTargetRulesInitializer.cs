// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnrealBuildTool.Tests.TestUtilities
{
	[TestClass]
	public static class StaticTargetRulesInitializer
	{
		internal const string UBTTestFolderPrefix = "UBT-Test-";
		internal static readonly string UBTTestFolderRoot = Path.GetTempPath();

		[AssemblyInitialize]
		public static void InitializeBuildEnvironment(TestContext context)
		{
			// Clean any previous runs linger temp folder
			// Remarks: this is due to target rules modules dll being held by the test process; full cleanup not feasible on existing run
			string[] subfolders = Directory.GetDirectories(UBTTestFolderRoot, $"{UBTTestFolderPrefix}*", SearchOption.TopDirectoryOnly);

			foreach (string folder in subfolders)
			{
				if (Directory.Exists(folder))
				{
					try
					{
						Directory.Delete(folder, true);
					}
					catch (Exception) { }
				}
			}

			// Example, on how to register a fully stubbed test environment.
			/*
			 	UEBuildPlatform.RegisterBuildPlatform(
					new TestBuildPlatfrom(
							UnrealTargetPlatform.Win64,
							new TestSDK(logger),
							new UnrealArchitectureConfig(UnrealArch.X64),
							logger),
					logger
			);*/

			ILogger logger = new TestLogger();

			UEBuildPlatform? _;
			if (!UEBuildPlatform.TryGetBuildPlatform(UnrealTargetPlatform.Win64, out _))
			{
				MicrosoftPlatformSDK SDK = new MicrosoftPlatformSDK(logger);
				UEBuildPlatform.RegisterBuildPlatform(new WindowsPlatform(UnrealTargetPlatform.Win64, SDK, logger), logger);
			}

			if (!UEBuildPlatform.TryGetBuildPlatform(UnrealTargetPlatform.Android, out _))
			{
				AndroidPlatformSDK ASDK = new AndroidPlatformSDK(logger);
				UEBuildPlatform.RegisterBuildPlatform(new AndroidPlatform(ASDK, logger), logger);
			}

			XmlConfig.ReadConfigFiles(null, null, logger);
		}
	}
}
