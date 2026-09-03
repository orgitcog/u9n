// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ADOSupport: ModuleRules
	{
		public ADOSupport(ReadOnlyTargetRules Target) : base(Target)
		{
			if (OperatingSystem.IsWindows() && Target.Platform == UnrealTargetPlatform.Win64)
			{
				string AdoFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonProgramFiles), "System", "ADO");
				PublicSystemIncludePaths.Add(AdoFolder);

				string MsAdo15 = Path.Combine(AdoFolder, "msado15.dll");
				TypeLibraries.Add(new TypeLibrary(MsAdo15, "rename(\"EOF\", \"ADOEOF\")", "msado15.tlh", "msado15.tli"));

				ExtraRootPath = ("ADO", AdoFolder);

				PrivateDefinitions.Add("USE_ADO_INTEGRATION=1");
			}
			else
			{
				PrivateDefinitions.Add("USE_ADO_INTEGRATION=0");
			}

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"DatabaseSupport",
					// ... add other public dependencies that you statically link with here ...
				}
				);
		}
	}
}
