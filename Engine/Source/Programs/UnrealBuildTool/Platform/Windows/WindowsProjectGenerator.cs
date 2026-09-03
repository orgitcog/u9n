// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class WindowsProjectGenerator : PlatformProjectGenerator
	{
		/// <summary>
		/// Whether to enable the experimental remote debugging support. 
		/// This will configure the project settings for remote debugging based on the fields below.
		/// Refer to https://learn.microsoft.com/en-us/visualstudio/debugger/remote-debugging-cpp for details on setting up Visual Studio and the remote device for remote debugging.
		/// </summary>
		protected bool bEnableExperimentalRemoteDebugging = false;

		/// <summary>
		/// When disabled, the remote debug directory is expected to be the root of a staged build.
		/// When enabled, the project name and target will be appended to the remote debug directory, allowing multiple projects to be more easily debugged.
		/// ...this can be used alongside RunUAT BuildCookRun [...] -archive -archivedirectory=\\remotepc\Share\MyProject\
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		protected bool bPerProjectRemoteDebugPaths = true;


		/// <summary>
		/// Remote machine name for Windows X64 remote debugging
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		protected string? RemoteDebugMachineX64;

		/// <summary>
		/// Local shared folder path on the remote Windows X64 remote machine to deploy to. 
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		protected string? RemoteDebugDirectoryX64;


		/// <summary>
		/// Remote machine name for Windows Arm64 remote debugging
		/// </summary>
		[XmlConfigFile(Category="WindowsPlatform")]
		protected string? RemoteDebugMachineArm64;

		/// <summary>
		/// Local shared folder path on the remote Windows Arm64 remote machine to deploy to. 
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		protected string? RemoteDebugDirectoryArm64;


		/// <summary>
		/// Remote machine name for Windows Arm64ec remote debugging
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		protected string? RemoteDebugMachineArm64ec;

		/// <summary>
		/// Local shared folder path on the remote Windows Arm64ec remote machine to deploy to. 
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		protected string? RemoteDebugDirectoryArm64ec;


		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Arguments">Command line arguments passed to the project generator</param>
		/// <param name="Logger">Logger for output</param>
		public WindowsProjectGenerator(CommandLineArguments Arguments, ILogger Logger)
			: base(Arguments, Logger)
		{
			XmlConfig.ApplyTo(this);

			// must read this value directly because it exists in the windows target rules too
			if (XmlConfig.TryGetValue(typeof(WindowsTargetRules), "bEnableExperimentalRemoteDebugging", out object? ConfigObject))
			{
				bEnableExperimentalRemoteDebugging = (bool)ConfigObject;
			}
		}

		/// <inheritdoc/>
		public override IEnumerable<UnrealTargetPlatform> GetPlatforms()
		{
			yield return UnrealTargetPlatform.Win64;
		}

		/// <inheritdoc/>
		public override string GetVisualStudioPlatformName(VSSettings InVSSettings)
		{
			if (InVSSettings.Platform == UnrealTargetPlatform.Win64)
			{
				if (InVSSettings.Architecture == UnrealArch.Arm64)
				{
					return "arm64";
				}
				else if (InVSSettings.Architecture == UnrealArch.Arm64ec)
				{
					return "arm64ec";
				}
				return "x64";
			}
			return InVSSettings.Platform.ToString();
		}

		/// <inheritdoc/>
		public override string GetVisualStudioUserFileStrings(VisualStudioUserFileSettings VCUserFileSettings, VSSettings InVSSettings, string InConditionString, TargetRules InTargetRules, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference? NMakeOutputPath, string ProjectName, string? ForeignUProjectPath)
		{
			StringBuilder VCUserFileContent = new StringBuilder();

			string LocalOrRemoteString = InVSSettings.Architecture == null || (InVSSettings.Architecture.Value.bIsX64 == UnrealArch.Host.Value.bIsX64)
				? "Local" : "Remote";

			VCUserFileContent.AppendLine("  <PropertyGroup {0}>", InConditionString);
			if (InTargetRules.Type != TargetType.Game)
			{
				string DebugOptions = "";

				if (ForeignUProjectPath != null)
				{
					DebugOptions += ForeignUProjectPath;
					DebugOptions += " -skipcompile";
				}
				else if (InTargetRules.Type == TargetType.Editor && InTargetRules.ProjectFile != null)
				{
					DebugOptions += ProjectName;
				}

				VCUserFileContent.AppendLine($"    <{LocalOrRemoteString}DebuggerCommandArguments>{DebugOptions}</{LocalOrRemoteString}DebuggerCommandArguments>");
			}

			VCUserFileContent.AppendLine($"    <DebuggerFlavor>Windows{LocalOrRemoteString}Debugger</DebuggerFlavor>");
			VCUserFileContent.AppendLine("  </PropertyGroup>");

			return VCUserFileContent.ToString();
		}

		/// <inheritdoc/>
		public override void GetVisualStudioPathsEntries(VSSettings InVSSettings, TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference NMakeOutputPath, StringBuilder ProjectFileBuilder)
		{
			base.GetVisualStudioPathsEntries(InVSSettings, TargetType, TargetRulesPath, ProjectFilePath, NMakeOutputPath, ProjectFileBuilder);

			if (bEnableExperimentalRemoteDebugging && 
				TargetType != TargetType.Editor &&
				GetRemoteDebugProperties(InVSSettings.Architecture, out string? RemoteDebugMachine, out string? RemoteDebugDirectory))
			{			
				string RemoteDebugWorkingDir = RemoteDebugDirectory;

				// can optionally use a subdirectory within the shared folder to make it simpler to debug multiple projects
				if (bPerProjectRemoteDebugPaths)
				{
					string ProjectName = (TargetType == TargetType.Program)
						? NMakeOutputPath.Directory.GetDirectoryName()
						: NMakeOutputPath.Directory!.ParentDirectory!.ParentDirectory!.GetDirectoryName();

					string PlatformTargetName = (TargetType == TargetType.Program) ? "" : "Windows";
					if (TargetType != TargetType.Game && TargetType != TargetType.Program)
					{
						PlatformTargetName += TargetType.ToString();
					}

					DirectoryReference BaseDir = NMakeOutputPath.Directory!.ParentDirectory!.ParentDirectory!.ParentDirectory!;
					string RelativeWorkingDir = NMakeOutputPath.Directory.MakeRelativeTo(BaseDir);
					RemoteDebugWorkingDir = Path.Combine(RemoteDebugDirectory, ProjectName, PlatformTargetName, RelativeWorkingDir);
				}

				string OutDir = ProjectFile.NormalizeProjectPath(NMakeOutputPath.Directory.FullName); // need to override OutDir for remote debugging so that VS deploys the executable correctly
				string RemoteDebugCommand = Path.Combine(RemoteDebugWorkingDir, NMakeOutputPath.GetFileName());

				ProjectFileBuilder.AppendLine($"    <OutDir>{OutDir}</OutDir>");
				ProjectFileBuilder.AppendLine($"    <DeploymentDirectory>{RemoteDebugWorkingDir}</DeploymentDirectory>");
				ProjectFileBuilder.AppendLine($"    <RemoteDebuggerCommand>{RemoteDebugCommand}</RemoteDebuggerCommand>");
				ProjectFileBuilder.AppendLine($"    <RemoteDebuggerWorkingDirectory>{RemoteDebugWorkingDir}</RemoteDebuggerWorkingDirectory>");
				ProjectFileBuilder.AppendLine($"    <RemoteDebuggerServerName>{RemoteDebugMachine}</RemoteDebuggerServerName>");
				ProjectFileBuilder.AppendLine($"    <RemoteDebuggerDebuggerType>NativeOnly</RemoteDebuggerDebuggerType>");
				ProjectFileBuilder.AppendLine($"    <RemoteDebuggerDeployCppRuntime>false</RemoteDebuggerDeployCppRuntime>");

				// default to remote debugging for other architectures
				if (InVSSettings.Architecture != null && InVSSettings.Architecture.Value != UnrealArch.Host.Value)
				{
					ProjectFileBuilder.AppendLine($"    <DebuggerFlavor>WindowsRemoteDebugger</DebuggerFlavor>");
				}
			}
		}

		/// <inheritdoc/>
		public override bool GetVisualStudioDeploymentEnabled(VSSettings InVSSettings)
		{
			// remote debugging requires deployment
			if (bEnableExperimentalRemoteDebugging && GetRemoteDebugProperties(InVSSettings.Architecture, out _, out _))
			{
				return true;
			}

			return false;
		}

		/// <inheritdoc/>
		public override bool RequiresVSUserFileGeneration()
		{
			return true;
		}

		/// <inheritdoc/>
		public override IList<string> GetSystemIncludePaths(UEBuildTarget InTarget)
		{
			List<string> Result = new List<string>();
			foreach (DirectoryReference Path in InTarget.Rules.WindowsPlatform.Environment!.IncludePaths)
			{
				Result.Add(Path.FullName);
			}

			return Result;
		}

		protected virtual bool GetRemoteDebugProperties(UnrealArch? Architecture, [NotNullWhen(true)] out string? RemoteDebugMachine, [NotNullWhen(true)] out string? RemoteDebugDirectory)
		{
			RemoteDebugMachine = null;
			RemoteDebugDirectory = null;

			if (Architecture != null)
			{
				if (Architecture == UnrealArch.X64)
				{
					RemoteDebugMachine = RemoteDebugMachineX64;
					RemoteDebugDirectory = RemoteDebugDirectoryX64;
				}
				else if (Architecture == UnrealArch.Arm64)
				{
					RemoteDebugMachine = RemoteDebugMachineArm64;
					RemoteDebugDirectory = RemoteDebugDirectoryArm64;
				}
				else if (Architecture == UnrealArch.Arm64ec)
				{
					RemoteDebugMachine = RemoteDebugMachineArm64ec;
					RemoteDebugDirectory = RemoteDebugDirectoryArm64ec;
				}
			}

			return !string.IsNullOrEmpty(RemoteDebugMachine) && !string.IsNullOrEmpty(RemoteDebugDirectory);
		}
	}
}
