// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace UnrealBuildBase
{
	public static class Unreal
	{
		private static DirectoryReference FindRootDirectory()
		{
			if (LocationOverride.RootDirectory != null)
			{
				return DirectoryReference.FindCorrectCase(LocationOverride.RootDirectory);
			}

			string? OverrideArg = Environment.GetCommandLineArgs().FirstOrDefault(x => x?.StartsWith("-rootdirectory=") ?? false, null);
			if (OverrideArg != null)
			{
				string[] Parts = OverrideArg.Split('=', 2);
				return new DirectoryReference(Path.GetFullPath(Parts[1]));
			}

			// This base library may be used - and so be launched - from more than one location (at time of writing, UnrealBuildTool and AutomationTool)
			// Programs that use this assembly must be located under "Engine/Binaries/DotNET" and so we look for that sequence of directories in that path of the executing assembly

			// Use the EntryAssembly (the application path), rather than the ExecutingAssembly (the library path)
			string AssemblyLocation = Assembly.GetEntryAssembly()!.GetOriginalLocation();

			DirectoryReference? FoundRootDirectory = DirectoryReference.FindCorrectCase(DirectoryReference.FromString(AssemblyLocation)!);

			// Search up through the directory tree for the deepest instance of the sub-path "Engine/Binaries/DotNET"
			while (FoundRootDirectory != null)
			{
				if (String.Equals("DotNET", FoundRootDirectory.GetDirectoryName()))
				{
					FoundRootDirectory = FoundRootDirectory.ParentDirectory;
					if (FoundRootDirectory != null && String.Equals("Binaries", FoundRootDirectory.GetDirectoryName()))
					{
						FoundRootDirectory = FoundRootDirectory.ParentDirectory;
						if (FoundRootDirectory != null && String.Equals("Engine", FoundRootDirectory.GetDirectoryName()))
						{
							FoundRootDirectory = FoundRootDirectory.ParentDirectory;
							break;
						}
						continue;
					}
					continue;
				}
				FoundRootDirectory = FoundRootDirectory.ParentDirectory;
			}

			// Search up through the directory tree for the deepest instance of the sub-path "Engine/Source/Programs"
			if (FoundRootDirectory == null)
			{
				FoundRootDirectory = DirectoryReference.FindCorrectCase(DirectoryReference.FromString(AssemblyLocation)!);
				while (FoundRootDirectory != null)
				{
					if (String.Equals("Programs", FoundRootDirectory.GetDirectoryName()))
					{
						FoundRootDirectory = FoundRootDirectory.ParentDirectory;
						if (FoundRootDirectory != null && String.Equals("Source", FoundRootDirectory.GetDirectoryName()))
						{
							FoundRootDirectory = FoundRootDirectory.ParentDirectory;
							if (FoundRootDirectory != null && String.Equals("Engine", FoundRootDirectory.GetDirectoryName()))
							{
								FoundRootDirectory = FoundRootDirectory.ParentDirectory;
								break;
							}
							continue;
						}
						continue;
					}
					FoundRootDirectory = FoundRootDirectory.ParentDirectory;
				}
			}

			if (FoundRootDirectory == null)
			{
				throw new Exception($"This code requires that applications using it are launched from a path containing \"Engine/Binaries/DotNET\" or \"Engine/Source/Programs\". This application was launched from {Path.GetDirectoryName(AssemblyLocation)}");
			}

			// Confirm that we've found a valid root directory, by testing for the existence of a well-known file
			FileReference ExpectedExistingFile = FileReference.Combine(FoundRootDirectory, "Engine", "Build", "Build.version");
			if (!FileReference.Exists(ExpectedExistingFile))
			{
				throw new Exception($"Expected file \"Engine/Build/Build.version\" was not found at {ExpectedExistingFile.FullName}");
			}

			return FoundRootDirectory;
		}

		private static FileReference FindUnrealBuildToolDll()
		{
			// UnrealBuildTool.dll is assumed to be located under {RootDirectory}/Engine/Binaries/DotNET/UnrealBuildTool/
			FileReference UnrealBuildToolDllPath = FileReference.Combine(EngineDirectory, "Binaries", "DotNET", "UnrealBuildTool", "UnrealBuildTool.dll");

			UnrealBuildToolDllPath = FileReference.FindCorrectCase(UnrealBuildToolDllPath);

			if (!FileReference.Exists(UnrealBuildToolDllPath))
			{
				throw new Exception($"Unable to find UnrealBuildTool.dll in the expected location at {UnrealBuildToolDllPath.FullName}");
			}

			return UnrealBuildToolDllPath;
		}

		private static string DotnetVersionDirectory = "8.0.300";

		private static string FindRelativeDotnetDirectory(RuntimePlatform.Type HostPlatform)
		{
			string platform;
			string architecture;

			switch (HostPlatform)
			{
				case RuntimePlatform.Type.Linux: platform = "linux"; break;
				case RuntimePlatform.Type.Mac: platform = "mac"; break;
				case RuntimePlatform.Type.Windows: platform = "win"; break;
				default: throw new Exception($"Unsupported host platform {HostPlatform}");
			}

			switch (RuntimeInformation.ProcessArchitecture)
			{
				case Architecture.Arm64: architecture = "arm64"; break;
				case Architecture.X64: architecture = "x64"; break;
				default: throw new Exception($"Unsupported host architecture {RuntimeInformation.ProcessArchitecture}");
			}

			return Path.Combine("Binaries", "ThirdParty", "DotNet", DotnetVersionDirectory, $"{platform}-{architecture}");
		}

		private static string FindRelativeDotnetDirectory() => FindRelativeDotnetDirectory(RuntimePlatform.Current);

		/// <summary>
		/// Relative path to the dotnet executable from EngineDir
		/// </summary>
		/// <returns></returns>
		public static string RelativeDotnetDirectory => _RelativeDotnetDirectory.Value;
		private static readonly Lazy<string> _RelativeDotnetDirectory = new(FindRelativeDotnetDirectory);

		private static DirectoryReference FindDotnetDirectory() => DirectoryReference.Combine(EngineDirectory, RelativeDotnetDirectory);

		/// <summary>
		/// The full name of the root UE directory
		/// </summary>
		public static DirectoryReference RootDirectory => _RootDirectory.Value;
		private static readonly Lazy<DirectoryReference> _RootDirectory = new(FindRootDirectory);

		/// <summary>
		/// The full name of the Engine directory
		/// </summary>
		public static DirectoryReference EngineDirectory => _EngineDirectory.Value;
		private static readonly Lazy<DirectoryReference> _EngineDirectory = new(() => DirectoryReference.Combine(RootDirectory, "Engine"));

		/// <summary>
		/// The full name of the Engine/Source directory
		/// </summary>
		public static DirectoryReference EngineSourceDirectory => _EngineSourceDirectory.Value;
		private static readonly Lazy<DirectoryReference> _EngineSourceDirectory = new(() => DirectoryReference.Combine(EngineDirectory, "Source"));

		/// <summary>
		/// Returns the Application Settings Directory path. This matches FPlatformProcess::ApplicationSettingsDir().
		/// </summary>
		public static DirectoryReference ApplicationSettingDirectory => _ApplicationSettingDirectory.Value;
		private static readonly Lazy<DirectoryReference> _ApplicationSettingDirectory = new(GetApplicationSettingDirectory);

		/// <summary>
		/// Returns the User Settings Directory path. This matches FPlatformProcess::UserSettingsDir().
		/// </summary>
		public static DirectoryReference UserSettingDirectory => _UserSettingDirectory.Value;
		private static readonly Lazy<DirectoryReference> _UserSettingDirectory = new(GetUserSettingDirectory);

		/// <summary>
		/// Returns the User Directory path. This matches FPlatformProcess::UserDir().
		/// </summary>
		public static DirectoryReference? UserDirectory => _UserDirectory.Value;
		private static readonly Lazy<DirectoryReference?> _UserDirectory = new(GetUserDirectory);

		/// <summary>
		/// Writable engine directory. Uses the user's settings folder for installed builds.
		/// </summary>
		public static DirectoryReference WritableEngineDirectory => _WritableEngineDirectory.Value;
		private static readonly Lazy<DirectoryReference> _WritableEngineDirectory = new(() => IsEngineInstalled() ? DirectoryReference.Combine(UserSettingDirectory, "UnrealEngine") : EngineDirectory);

		/// <summary>
		/// The engine saved programs directory
		/// </summary>
		public static DirectoryReference EngineProgramSavedDirectory => _EngineProgramSavedDirectory.Value;
		private static readonly Lazy<DirectoryReference> _EngineProgramSavedDirectory = new(() => IsEngineInstalled() ? UserSettingDirectory : DirectoryReference.Combine(EngineDirectory, "Programs"));

		/// <summary>
		/// The path to UBT
		/// </summary>
		[Obsolete("Deprecated in UE5.1; to launch UnrealBuildTool, use this dll as the first argument with DonetPath")]
		public static FileReference UnrealBuildToolPath => _UnrealBuildToolDllPath.Value.ChangeExtension(RuntimePlatform.ExeExtension);

		/// <summary>
		/// The path to UBT
		/// </summary>
		public static FileReference UnrealBuildToolDllPath => _UnrealBuildToolDllPath.Value;
		private static readonly Lazy<FileReference> _UnrealBuildToolDllPath = new(FindUnrealBuildToolDll);

		/// <summary>
		/// The directory containing the bundled .NET installation
		/// </summary>
		public static DirectoryReference DotnetDirectory => _DotnetDirectory.Value;
		private static readonly Lazy<DirectoryReference> _DotnetDirectory = new(FindDotnetDirectory);

		/// <summary>
		/// The path of the bundled dotnet executable
		/// </summary>
		public static FileReference DotnetPath => _DotnetPath.Value;
		private static readonly Lazy<FileReference> _DotnetPath = new(() => FileReference.Combine(DotnetDirectory, "dotnet" + RuntimePlatform.ExeExtension));

		/// <summary>
		/// Returns true if the application is running on a build machine
		/// </summary>
		/// <returns>True if running on a build machine</returns>
		public static bool IsBuildMachine() => _IsBuildMachine.Value;
		private static readonly Lazy<bool> _IsBuildMachine = new Lazy<bool>(() => Environment.GetEnvironmentVariable("IsBuildMachine")?.Trim() == "1");

		/// <summary>
		/// Returns true if the application is running using installed Engine components
		/// </summary>
		/// <returns>True if running using installed Engine components</returns>
		public static bool IsEngineInstalled() => _IsEngineInstalled.Value;
		private static readonly Lazy<bool> _IsEngineInstalled = new(() => FileReference.Exists(FileReference.Combine(EngineDirectory, "Build", "InstalledBuild.txt")));

		/// <summary>
		/// If we are running with an installed project, specifies the path to it
		/// </summary>
		private static readonly Lazy<FileReference?> _InstalledProjectFile = new(() => {
			FileReference installedProjectLocationFile = FileReference.Combine(EngineDirectory, "Build", "InstalledProjectBuild.txt");
			return FileReference.Exists(installedProjectLocationFile) ? FileReference.Combine(RootDirectory, FileReference.ReadAllText(installedProjectLocationFile).Trim()) : null;
		});

		/// <summary>
		/// The original root directory that was used to compile the installed engine
		/// Used to remap source code paths when debugging.
		/// </summary>
		public static DirectoryReference OriginalCompilationRootDirectory => _OriginalCompilationRootDirectory.Value;
		private static readonly Lazy<DirectoryReference> _OriginalCompilationRootDirectory = new(FindOriginalCompilationRootDirectory);

		/// <summary>
		/// Returns where another platform's Dotnet is located
		/// </summary>
		/// <param name="HostPlatform"></param>
		/// <returns></returns>
		public static DirectoryReference FindDotnetDirectoryForPlatform(RuntimePlatform.Type HostPlatform) => DirectoryReference.Combine(EngineDirectory, FindRelativeDotnetDirectory(HostPlatform));

		/// <summary>
		/// Returns true if the application is running using an installed project (ie. a mod kit)
		/// </summary>
		/// <returns>True if running using an installed project</returns>
		public static bool IsProjectInstalled() => _InstalledProjectFile.Value != null;

		/// <summary>
		/// Gets the installed project file
		/// </summary>
		/// <returns>Location of the installed project file</returns>
		public static FileReference? GetInstalledProjectFile() => _InstalledProjectFile.Value;

		/// <summary>
		/// Checks whether the given file is under an installed directory, and should not be overridden
		/// </summary>
		/// <param name="File">File to test</param>
		/// <returns>True if the file is part of the installed distribution, false otherwise</returns>
		public static bool IsFileInstalled(FileReference File)
		{
			if (IsEngineInstalled() && File.IsUnderDirectory(EngineDirectory))
			{
				return true;
			}
			if (IsProjectInstalled() && File.IsUnderDirectory(_InstalledProjectFile.Value!.Directory))
			{
				return true;
			}
			return false;
		}

		private static DirectoryReference FindOriginalCompilationRootDirectory()
		{
			if (IsEngineInstalled())
			{
				// Load Engine\Intermediate\Build\BuildRules\*RulesManifest.json
				DirectoryReference BuildRules = DirectoryReference.Combine(EngineDirectory, "Intermediate", "Build", "BuildRules");
				FileReference? RulesManifest = DirectoryReference.EnumerateFiles(BuildRules, "*RulesManifest.json").FirstOrDefault();
				if (RulesManifest != null)
				{
					JsonObject Manifest = JsonObject.Read(RulesManifest);
					if (Manifest.TryGetStringArrayField("SourceFiles", out string[]? SourceFiles))
					{
						FileReference? SourceFile = FileReference.FromString(SourceFiles.FirstOrDefault());
						if (SourceFile != null && !SourceFile.IsUnderDirectory(EngineDirectory))
						{
							// Walk up parent directory until Engine is found
							DirectoryReference? Directory = SourceFile.Directory;
							while (Directory != null && !Directory.IsRootDirectory())
							{
								if (Directory.GetDirectoryName() == "Engine" && Directory.ParentDirectory != null)
								{
									return Directory.ParentDirectory;
								}

								Directory = Directory.ParentDirectory;
							}
						}
					}
				}
			}

			return RootDirectory;
		}

		public static class LocationOverride
		{
			/// <summary>
			/// If set, this value will be used to populate Unreal.RootDirectory
			/// </summary>
			public static DirectoryReference? RootDirectory = null;
		}

		// A subset of the functionality in DataDrivenPlatformInfo.GetAllPlatformInfos() - finds the DataDrivenPlatformInfo.ini files and records their existence, but does not parse them
		// (perhaps DataDrivenPlatformInfo.GetAllPlatformInfos() could be modified to use this data to avoid an additional search through the filesystem)
		private static readonly Lazy<HashSet<string>> IniPresentForPlatform = new Lazy<HashSet<string>>(() =>
		{
			HashSet<string> Set = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
			// find all platform directories (skipping NFL/NoRedist)
			foreach (DirectoryReference EngineConfigDir in GetExtensionDirs(Unreal.EngineDirectory, "Config", bIncludeRestrictedDirectories: false))
			{
				// look through all config dirs looking for the data driven ini file
				foreach (string FilePath in Directory.EnumerateFiles(EngineConfigDir.FullName, "DataDrivenPlatformInfo.ini", SearchOption.AllDirectories))
				{
					FileReference FileRef = new FileReference(FilePath);

					// get the platform name from the path
					string IniPlatformName;
					if (FileRef.IsUnderDirectory(DirectoryReference.Combine(Unreal.EngineDirectory, "Config")))
					{
						// Foo/Engine/Config/<Platform>/DataDrivenPlatformInfo.ini
						IniPlatformName = Path.GetFileName(Path.GetDirectoryName(FilePath))!;
					}
					else
					{
						// Foo/Engine/Platforms/<Platform>/Config/DataDrivenPlatformInfo.ini
						IniPlatformName = Path.GetFileName(Path.GetDirectoryName(Path.GetDirectoryName(FilePath)))!;
					}

					// DataDrivenPlatformInfo.GetAllPlatformInfos() checks that [DataDrivenPlatformInfo] section exists as part of validating that the file exists
					// This code should probably behave the same way.

					Set.Add(IniPlatformName);
				}
			}

			return Set;
		});

		private static bool DataDrivenPlatformInfoIniIsPresent(string PlatformName) => IniPresentForPlatform.Value.Contains(PlatformName);

		// cached dictionary of BaseDir to extension directories
		private static ConcurrentDictionary<DirectoryReference, Tuple<List<DirectoryReference>, List<DirectoryReference>>> CachedExtensionDirectories = new();

		/// <summary>
		/// Finds all the extension directories for the given base directory. This includes platform extensions and restricted folders.
		/// </summary>
		/// <param name="BaseDir">Location of the base directory</param>
		/// <param name="bIncludePlatformDirectories">If true, platform subdirectories are included (will return platform directories under Restricted dirs, even if bIncludeRestrictedDirectories is false)</param>
		/// <param name="bIncludeRestrictedDirectories">If true, restricted (NotForLicensees, NoRedist) subdirectories are included</param>
		/// <param name="bIncludeBaseDirectory">If true, BaseDir is included</param>
		/// <returns>List of extension directories, including the given base directory</returns>
		public static List<DirectoryReference> GetExtensionDirs(DirectoryReference BaseDir, bool bIncludePlatformDirectories = true, bool bIncludeRestrictedDirectories = true, bool bIncludeBaseDirectory = true)
		{
			Tuple<List<DirectoryReference>, List<DirectoryReference>> CachedDirs = CachedExtensionDirectories.GetOrAdd(BaseDir, _ =>
			{
				Tuple<List<DirectoryReference>, List<DirectoryReference>> NewCachedDirs = Tuple.Create(new List<DirectoryReference>(), new List<DirectoryReference>());

				DirectoryReference PlatformExtensionBaseDir = DirectoryReference.Combine(BaseDir, "Platforms");
				if (DirectoryReference.Exists(PlatformExtensionBaseDir))
				{
					NewCachedDirs.Item1.AddRange(DirectoryReference.EnumerateDirectories(PlatformExtensionBaseDir));
				}

				DirectoryReference RestrictedBaseDir = DirectoryReference.Combine(BaseDir, "Restricted");
				if (DirectoryReference.Exists(RestrictedBaseDir))
				{
					IEnumerable<DirectoryReference> RestrictedDirs = DirectoryReference.EnumerateDirectories(RestrictedBaseDir);
					NewCachedDirs.Item2.AddRange(RestrictedDirs);

					// also look for nested platforms in the restricted
					foreach (DirectoryReference RestrictedDir in RestrictedDirs)
					{
						DirectoryReference RestrictedPlatformExtensionBaseDir = DirectoryReference.Combine(RestrictedDir, "Platforms");
						if (DirectoryReference.Exists(RestrictedPlatformExtensionBaseDir))
						{
							NewCachedDirs.Item1.AddRange(DirectoryReference.EnumerateDirectories(RestrictedPlatformExtensionBaseDir));
						}
					}
				}

				// remove any platform directories in non-engine locations if the engine doesn't have the platform 
				if (BaseDir != Unreal.EngineDirectory && NewCachedDirs.Item1.Count > 0)
				{
					// if the DDPI.ini file doesn't exist, we haven't synced the platform, so just skip this directory
					NewCachedDirs.Item1.RemoveAll(x => !DataDrivenPlatformInfoIniIsPresent(x.GetDirectoryName()));
				}
				return NewCachedDirs;
			});

			// now return what the caller wanted (always include BaseDir)
			List<DirectoryReference> ExtensionDirs = [];
			if (bIncludeBaseDirectory)
			{
				ExtensionDirs.Add(BaseDir);
			}
			if (bIncludePlatformDirectories)
			{
				ExtensionDirs.AddRange(CachedDirs.Item1);
			}
			if (bIncludeRestrictedDirectories)
			{
				ExtensionDirs.AddRange(CachedDirs.Item2);
			}
			return ExtensionDirs;
		}

		/// <summary>
		/// Finds all the extension directories for the given base directory. This includes platform extensions and restricted folders.f
		/// </summary>
		/// <param name="BaseDir">Location of the base directory</param>
		/// <param name="SubDir">The subdirectory to find</param>
		/// <param name="bIncludePlatformDirectories">If true, platform subdirectories are included (will return platform directories under Restricted dirs, even if bIncludeRestrictedDirectories is false)</param>
		/// <param name="bIncludeRestrictedDirectories">If true, restricted (NotForLicensees, NoRedist) subdirectories are included</param>
		/// <param name="bIncludeBaseDirectory">If true, BaseDir is included</param>
		/// <returns>List of extension directories, including the given base directory</returns>
		public static List<DirectoryReference> GetExtensionDirs(DirectoryReference BaseDir, string SubDir, bool bIncludePlatformDirectories = true, bool bIncludeRestrictedDirectories = true, bool bIncludeBaseDirectory = true)
		{
			return GetExtensionDirs(BaseDir, bIncludePlatformDirectories, bIncludeRestrictedDirectories, bIncludeBaseDirectory)
				.Select(x => DirectoryReference.Combine(x, SubDir))
				.Where(x => DirectoryReference.Exists(x))
				.ToList();
		}

		/// <summary>
		/// Returns the Application Settings Directory path. This matches FPlatformProcess::ApplicationSettingsDir().
		/// </summary>
		private static DirectoryReference GetApplicationSettingDirectory()
		{
			if (OperatingSystem.IsMacOS() || OperatingSystem.IsLinux())
			{
				return new DirectoryReference(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Epic"));
			}

			return new DirectoryReference(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "Epic"));
		}

		/// <summary>
		/// Returns the User Settings Directory path. This matches FPlatformProcess::UserSettingsDir().
		/// </summary>
		private static DirectoryReference GetUserSettingDirectory()
		{
			if (OperatingSystem.IsMacOS() || OperatingSystem.IsLinux())
			{
				// Mac and Linux use the same folder for UserSettingsDir and ApplicationSettingsDir
				return GetApplicationSettingDirectory();
			}
			else
			{
				// Not all user accounts have a local application data directory (eg. SYSTEM, used by Jenkins for builds).
				List<Environment.SpecialFolder> DataFolders = [
					Environment.SpecialFolder.LocalApplicationData,
					Environment.SpecialFolder.CommonApplicationData
				];
				foreach (Environment.SpecialFolder DataFolder in DataFolders)
				{
					string DirectoryName = Environment.GetFolderPath(DataFolder);
					if (!String.IsNullOrEmpty(DirectoryName))
					{
						return new DirectoryReference(DirectoryName);
					}
				}
			}

			return DirectoryReference.Combine(EngineDirectory, "Saved");
		}

		/// <summary>
		/// Returns the User Directory path. This matches FPlatformProcess::UserDir().
		/// </summary>
		private static DirectoryReference? GetUserDirectory()
		{
			// Some user accounts (eg. SYSTEM on Windows) don't have a home directory. Ignore them if Environment.GetFolderPath() returns an empty string.
			string PersonalFolder = Environment.GetFolderPath(Environment.SpecialFolder.Personal);
			if (!String.IsNullOrEmpty(PersonalFolder))
			{
				if (OperatingSystem.IsMacOS() || OperatingSystem.IsLinux())
				{
					return new DirectoryReference(System.IO.Path.Combine(PersonalFolder, "Documents"));
				}
				else
				{
					return new DirectoryReference(PersonalFolder);
				}
			}
			return null;
		}

		/// <summary>
		/// The current Machine name
		/// </summary>
		public static string MachineName => _MachineName.Value;
		private static readonly Lazy<string> _MachineName = new(() =>
		{
			try
			{
				// this likely can't fail, but just in case, fallback to preview implementation
				string machineName = System.Net.Dns.GetHostName();

				if (OperatingSystem.IsMacOS() && machineName.EndsWith(".local"))
				{
					machineName = machineName.Replace(".local", "");
				}
				return machineName;
			}
			catch (Exception)
			{
			}
			return Environment.MachineName;
		});
	}
}
