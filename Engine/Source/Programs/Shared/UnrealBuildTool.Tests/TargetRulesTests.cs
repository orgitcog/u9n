// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using EpicGames.Core;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using UnrealBuildTool.Tests.TestUtilities;
using UnrealBuildBase;

namespace UnrealBuildTool.Tests
{
	[TestClass]
	public class TargetRulesTests
	{
		[TestMethod]
		public void TestTargetRequiresUniqueEnvironmentFirstOrder()
		{
			DirectoryReference programsDirectory = DirectoryReference.Combine(Unreal.EngineSourceDirectory, "Programs");
			DirectoryReference testStubsDirectory = DirectoryReference.Combine(programsDirectory, "Shared", "UnrealBuildTool.Tests", "UBT");

			// Generate ephemeral TargetRules & uproject files
			using (SafeTestDirectory testRootDirectory = SafeTestDirectory.CreateTestDirectory(Path.Combine(StaticTargetRulesInitializer.UBTTestFolderRoot, $"{StaticTargetRulesInitializer.UBTTestFolderPrefix}{Guid.NewGuid().ToString()}")))
			using (SafeTestDirectory testSourceDirectory = SafeTestDirectory.CreateTestDirectory(Path.Combine(testRootDirectory.TemporaryDirectory, "Source")))
			using (SafeTestFile testEditorTargetTestFile = new SafeTestFile(File.ReadAllText(Path.Combine(testStubsDirectory.FullName, "TargetStubs", "TestStubEditor.Target.ubttest")), "TestStubEditor.Target.cs", testSourceDirectory.TemporaryDirectory))
			using (SafeTestFile testUprojectTestFile = new SafeTestFile(File.ReadAllText(Path.Combine(testStubsDirectory.FullName, "UprojectStubs", "TestStub.uproject.ubttest")), "TestStub.uproject", testRootDirectory.TemporaryDirectory))
			{
				FileReference testUproject = new FileReference(testUprojectTestFile.TemporaryFile);

				// Generate rules assembly & subsequent targets
				RulesAssembly generatedTestRulesAssembly = RulesCompiler.CreateTargetRulesAssembly(testUproject, "TestStubEditor", false, false, false, null, false, new TestLogger());
				TargetRules testTargetRules = generatedTestRulesAssembly.CreateTargetRules("TestStubEditor", UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Development, null, testUproject, null, new TestLogger());
				string? baseTarget = null;

				// Do not mutate the object
				{
					bool result = testTargetRules.RequiresUniqueEnvironment(generatedTestRulesAssembly, null, new Dictionary<string, (string?, string?)>(), out baseTarget);

					Assert.IsFalse(result);
				}

				// Mutate the object against a RequiresUniqueEnvironment
				{
					WarningLevel preWarningLevel = testTargetRules.CppCompileWarningSettings.ShadowVariableWarningLevel;
					testTargetRules.CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Warning;
					bool result = testTargetRules.RequiresUniqueEnvironment(generatedTestRulesAssembly, null, new Dictionary<string, (string?, string?)>(), out baseTarget);

					Assert.IsTrue(result);

					testTargetRules.CppCompileWarningSettings.ShadowVariableWarningLevel = preWarningLevel;
				}


				// Mutate the object against a RequiresUniqueEnvironment (Clang only)
				{
					WarningLevel preWarningLevel = testTargetRules.CppCompileWarningSettings.NonTrivialMemAccessWarningLevel;
					testTargetRules.CppCompileWarningSettings.NonTrivialMemAccessWarningLevel = WarningLevel.Warning;
					bool result = testTargetRules.RequiresUniqueEnvironment(generatedTestRulesAssembly, null, new Dictionary<string, (string?, string?)>(), out baseTarget);

					Assert.IsTrue(result);

					testTargetRules.CppCompileWarningSettings.NonTrivialMemAccessWarningLevel = preWarningLevel;
				}

				// Mutate the object against a RequiresUniqueEnvironment (Default warning set)
				{
					WarningLevel preWarningLevel = testTargetRules.CppCompileWarningSettings.DeprecationWarningLevel;
					testTargetRules.CppCompileWarningSettings.DeprecationWarningLevel = WarningLevel.Error;
					bool result = testTargetRules.RequiresUniqueEnvironment(generatedTestRulesAssembly, null, new Dictionary<string, (string?, string?)>(), out baseTarget);

					Assert.IsTrue(result);

					testTargetRules.CppCompileWarningSettings.DeprecationWarningLevel = WarningLevel.Warning;
					result = testTargetRules.RequiresUniqueEnvironment(generatedTestRulesAssembly, null, new Dictionary<string, (string?, string?)>(), out baseTarget);

					Assert.IsFalse(result);

					testTargetRules.CppCompileWarningSettings.DeprecationWarningLevel = preWarningLevel;
				}

				// Mutate the object against a RequiresUniqueEnvironment, and set bOverrideBuildEnvironment=true
				{
					WarningLevel preWarningLevel = testTargetRules.CppCompileWarningSettings.ShadowVariableWarningLevel;
					testTargetRules.CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Warning;
					testTargetRules.bOverrideBuildEnvironment = true;

					bool result = testTargetRules.RequiresUniqueEnvironment(generatedTestRulesAssembly, null, new Dictionary<string, (string?, string?)>(), out baseTarget);

					Assert.IsFalse(result);

					testTargetRules.CppCompileWarningSettings.ShadowVariableWarningLevel = preWarningLevel;
				}
			}
		}
	}
}