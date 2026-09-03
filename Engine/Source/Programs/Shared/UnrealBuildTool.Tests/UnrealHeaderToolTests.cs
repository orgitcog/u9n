// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.UHT.Utils;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using UnrealBuildTool.Modes;

namespace UnrealBuildTool.Tests
{
	[TestClass]
	public class UnrealHeaderToolTests
	{
		[TestMethod]
		[Ignore]
		public void Run()
		{
			CommandLineArguments commandLineArguments = new CommandLineArguments([]);
			UhtGlobalOptions options = new UhtGlobalOptions(commandLineArguments);

			// Initialize the attributes
			UhtTables tables = new UhtTables();

			// Initialize the configuration
			IUhtConfig config = new UhtConfigImpl(commandLineArguments);

			// Run the tests
			using ILoggerFactory factory = LoggerFactory.Create(x => x.AddEpicDefault());
			Assert.IsTrue(UhtTestHarness.RunTests(tables, config, options, factory.CreateLogger<UhtTestHarness>()));
		}
	}
}
