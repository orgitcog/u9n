// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class DatasmithSDK : ModuleRules
	{
		public DatasmithSDK(ReadOnlyTargetRules Target)
			: base(Target)
		{
			CppStandard = CppStandardVersion.Cpp20;

			PublicIncludePathModuleNames.Add("Launch");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DatasmithCore",
					"DatasmithExporter",
					"DatasmithExporterUI",
					// Network layer
					"UdpMessaging",
				}
			);
		}
	}
}