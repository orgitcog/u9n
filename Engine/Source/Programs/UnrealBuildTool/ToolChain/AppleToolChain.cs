// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Helper class for managing Xcode paths, versions, etc. Helps to differentiate between Apple xcode platforms
	/// </summary>
	public abstract class AppleToolChainSettings
	{
		/// <summary>
		/// Cached copy of ApplePlatformSDK.GetToolchainDirectory, for existing code to work
		/// </summary>
		public DirectoryReference ToolchainDir => ApplePlatformSDK.GetToolchainDirectory();

		/// <summary>
		/// Name for Xcode plaform directory under Toolchains
		/// </summary>
		public string PlatformDirName;
		
		/// <summary>
		/// Name for Xcode simulator platform directory under Toolchains
		/// </summary>
		public string SimulatorPlatformDirName = "";

		/// <summary>
		/// A portion of the target "tuple"
		/// </summary>
		private string TargetOSName;

		/// <summary>
		/// The name apple uses for SDK directories
		/// </summary>
		private string TargetSDKName;

		/// <summary>
		/// The version of the SDK being used to build with
		/// </summary>
		public string SDKVersion;

		/// <summary>
		/// The version of the iOS SDK to target at build time.
		/// </summary>
		public string MinTargetVersion;

		/// <summary>
		/// The build version in a floating point value for easy comparison
		/// </summary>
		public readonly float SDKVersionFloat;

		/// <summary>
		/// The target version in a floating point value for easy comparison
		/// </summary>
		public readonly float MinTargetVersionFloat;

		/// <summary>
		/// For platforms that need a different min version for editor vs game, this can be overridden
		/// </summary>
		/// <param name="Type"></param>
		/// <returns></returns>
		public virtual string GetTargetVersionForTargetType(TargetType Type) => MinTargetVersion;

		/// <summary>
		/// Cache SDK dir for device and simulator (for non-Mac)
		/// </summary>
		DirectoryReference SDKDir;
		DirectoryReference? SimulatorSDKDir = null;

		/// <summary>
		/// Dummy UUID for running iOS binary natively on Mac
		/// </summary>
		public static readonly string LocalMacUUID = "10CA18AC-10CA18AC10CA18AC";

		/// <summary>
		/// Constructor, called by platform sybclasses
		/// </summary>
		/// <param name="OSPrefix">SDK name (like "MacOSX")</param>
		/// <param name="SimulatorOSPrefix">Sinulator SDK name (like "iPhoneOSSimulator")</param>
		/// <param name="TargetOSName">Platform name used in the -target parameter</param>
		/// <param name="TargetOSVersion">min OS version this project wants to target (not what it's being built with)</param>
		/// <param name="bVerbose"></param>
		/// <param name="Logger"></param>
		protected AppleToolChainSettings(string OSPrefix, string? SimulatorOSPrefix, string TargetOSName, string TargetOSVersion, bool bVerbose, ILogger Logger)
		{
			this.TargetOSName = TargetOSName;
			this.TargetSDKName = OSPrefix;
			PlatformDirName = OSPrefix.ToLower();

			// set up directories
			SDKDir = ApplePlatformSDK.GetPlatformSDKDirectory(OSPrefix);
			if (SimulatorOSPrefix != null)
			{
				SimulatorSDKDir = ApplePlatformSDK.GetPlatformSDKDirectory(SimulatorOSPrefix);
				SimulatorPlatformDirName = SimulatorOSPrefix.ToLower();
			}

			MinTargetVersion = TargetOSVersion;
			MinTargetVersionFloat = Single.Parse(MinTargetVersion, System.Globalization.CultureInfo.InvariantCulture);

			// cache some info for this OS
			SDKVersion = ApplePlatformSDK.GetPlatformSDKVersion(OSPrefix) ?? "";
			SDKVersionFloat = ApplePlatformSDK.GetPlatformSDKVersionFloat(OSPrefix);

			// Xcode sdk files are not allowed to be transferred over the network, ensure UBA will not do so
			EpicGames.UBA.Utils.RegisterDisallowedPaths(ApplePlatformSDK.DeveloperDir.FullName);

			TestXcode(bVerbose, Logger);
		}

		/// <summary>
		/// Get the path to the SDK diretory in xcode for the given architecture
		/// </summary>
		/// <param name="Architecture"></param>
		/// <returns></returns>
		public DirectoryReference GetSDKPath(UnrealArch Architecture)
		{
			// note that VisionOS uses IOSSimulator (as TVOS should eventually do as well)
			if (Architecture == UnrealArch.IOSSimulator || Architecture == UnrealArch.TVOSSimulator)
			{
				return SimulatorSDKDir!;
			}
			return SDKDir;
		}

		/// <summary>
		/// Gets the string used by xcode to taget a platform and version (will return something like "arm64-apple-ios17.0-simulator"
		/// </summary>
		/// <param name="Architecture"></param>
		/// <param name="Platform"></param>
		/// <param name="TargetType"></param>
		/// <param name="ForcedVersion"></param>
		/// <returns></returns>
		public virtual string GetTargetTuple(UnrealArch Architecture, UnrealTargetPlatform Platform, TargetType TargetType, string? ForcedVersion=null)
		{
			string Prefix = Architecture.AppleName;
			string Suffix = (Architecture == UnrealArch.IOSSimulator || Architecture == UnrealArch.TVOSSimulator) ? "-simulator" : "";
			string TargetVersion = ForcedVersion ?? GetTargetVersionForTargetType(TargetType);

			return $"{Prefix}-apple-{TargetOSName}{TargetVersion}{Suffix}";
		}

		/// <summary>
		/// Find the Xcode developer directory
		/// </summary>
		/// <param name="bVerbose"></param>
		/// <param name="Logger"></param>
		/// <exception cref="BuildException"></exception>
		private static void TestXcode(bool bVerbose, ILogger Logger)
		{
			DirectoryReference XcodeDeveloperDir = ApplePlatformSDK.DeveloperDir;
			// make sure we get a full path
			if (DirectoryReference.Exists(XcodeDeveloperDir) == false)
			{
				throw new BuildException("Selected Xcode ('{0}') doesn't exist, cannot continue.", XcodeDeveloperDir);
			}

			if (XcodeDeveloperDir.ContainsName("CommandLineTools", 0))
			{
				throw new BuildException($"Your Mac is set to use CommandLineTools for its build tools ({XcodeDeveloperDir}). Unreal expects Xcode as the build tools. Please install Xcode if it's not already, then do one of the following:\n" +
					"  - Run Xcode, go to Settings, and in the Locations tab, choose your Xcode in Command Line Tools dropdown.\n" +
					"  - In Terminal, run 'sudo xcode-select -s /Applications/Xcode.app' (or an alternate location if you installed Xcode to a non-standard location)\n" +
					"Either way, you will need to enter your Mac password.");
			}

			if (bVerbose && !XcodeDeveloperDir.FullName.StartsWith("/Applications/Xcode.app"))
			{
				Log.TraceInformationOnce("Compiling with non-standard Xcode: {0}", XcodeDeveloperDir);
			}

			// Installed engine requires Xcode 13
			if (Unreal.IsEngineInstalled())
			{
				string? InstalledSdkVersion = ApplePlatformSDK.InstalledXcodeVersion.Value;
				if (String.IsNullOrEmpty(InstalledSdkVersion))
				{
					throw new BuildException("Unable to get xcode version");
				}
				if (Int32.Parse(InstalledSdkVersion.Substring(0, 2)) < 13)
				{
					throw new BuildException("Building for macOS, iOS and tvOS requires Xcode 13.4.1 or newer, Xcode " + InstalledSdkVersion + " detected");
				}
			}
		}
	}

	abstract class AppleToolChain : ClangToolChain
	{
		protected class AppleToolChainInfo : ClangToolChainInfo
		{
			Tuple<Version, Version>[] AppleVersionToLLVMVersion;

			public AppleToolChainInfo(UnrealTargetPlatform Platform, DirectoryReference DeveloperDir, FileReference Clang, FileReference Archiver, ILogger Logger)
				: base(DeveloperDir, Clang, Archiver, Logger)
			{
				// get the mapping from Apple_SDK.json (turning "version ranges" into a mapping list)
				UEBuildPlatformSDK SDK = UEBuildPlatformSDK.GetSDKForPlatform(Platform.ToString())!;
				
				AppleVersionToLLVMVersion = SDK.GetVersionNumberRangeArrayFromConfig("AppleVersionToLLVMVersions").
					Select(x => new Tuple<Version, Version>(new Version(x.Min.ToString()), new Version(x.Max.ToString()))).ToArray();
			}

			// libtool doesn't provide version, just use clang's version string
			/// <inheritdoc/>
			protected override string QueryArchiverVersionString() => ClangVersionString;

			protected override Version QueryClangVersion()
			{
				// get the clang version from the clang -v
				Version AppleVersion = base.QueryClangVersion();

					// now look up this version in mappings
				for (int MappingIndex = AppleVersionToLLVMVersion.Length - 1; MappingIndex >= 0; MappingIndex--)
				{
					if (AppleVersion >= AppleVersionToLLVMVersion[MappingIndex].Item1)
					{
						Version LLVMVersion = AppleVersionToLLVMVersion[MappingIndex].Item2;
						Logger.LogDebug("Converted Apple version {AppleVersion} to LLVM version {LLVMVersion}", AppleVersion, LLVMVersion);
						return LLVMVersion;
					}
				}

				throw new BuildException($"Failed to find mapping of Apple clang version {AppleVersion} in Apple_SDK.json");
			}

			// get the actual Apple version, not the LLVM version (only Apple platform code should use this)
			public VersionNumber AppleClangVersion => VersionNumber.Parse(base.QueryClangVersion().ToString());
		}

		private Lazy<AppleToolChainSettings> ToolChainSettings;
		
		protected AppleToolChainSettings Settings => ToolChainSettings.Value;


		/// <summary>
		/// The SDK version being used to compile with the toolchain
		/// </summary>
		public float SDKVersionFloat => Settings.SDKVersionFloat;

		public readonly ReadOnlyTargetRules? Target;

		// cache some ini settings
		readonly bool bUseSwiftUIMain;
		readonly bool bCreateSwiftBridgingHeader;

		protected bool bUseModernXcode => AppleExports.UseModernXcode(ProjectFile);

		public AppleToolChain(ReadOnlyTargetRules? Target, Func<AppleToolChainSettings> InCreateSettings, ClangToolChainOptions InOptions, ILogger InLogger) : base(InOptions, InLogger)
		{
			this.Target = Target;
			ProjectFile = Target?.ProjectFile;
			ToolChainSettings = new Lazy<AppleToolChainSettings>(InCreateSettings);

			AppleExports.GetSwiftIntegrationSettings(ProjectFile, Target == null ? TargetType.Game : Target.Type, Target == null ? UnrealTargetPlatform.Mac : Target.Platform, out bUseSwiftUIMain, out bCreateSwiftBridgingHeader);
		}

		/// <summary>
		/// Takes an architecture string as provided by UBT for the target and formats it for Clang. Supports
		/// multiple architectures joined with '+'
		/// </summary>
		/// <param name="InArchitectures"></param>
		/// <returns></returns>
		protected string FormatArchitectureArg(UnrealArchitectures InArchitectures)
		{
			return "-arch " + String.Join('+', InArchitectures.Architectures.Select(x => x.AppleName));
		}

		public override string GetSDKVersion()
		{
			return Settings.SDKVersion;
		}

		protected DirectoryReference GetMacDevSrcRoot()
		{
			return Unreal.EngineSourceDirectory;
		}

		protected override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, IEnumerable<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph)
		{
			if (ShouldSkipCompile(CompileEnvironment))
			{
				return new CPPOutput();
			}

			// log some iseful info
			Log.TraceInformationOnce($"Compiling with {CompileEnvironment.Platform} SDK {Settings.SDKVersion}, minimum Target OS version {Settings.GetTargetVersionForTargetType(Target!.Type)}");

			List<string> GlobalArguments = new();

			GetCompileArguments_Global(CompileEnvironment, GlobalArguments);

			List<FileItem> FrameworkTokenFiles = new List<FileItem>();
			foreach (UEBuildFramework Framework in CompileEnvironment.AdditionalFrameworks)
			{
				if (Framework.ZipFile != null)
				{
					// even in modern, we have to try to unzip in UBT, so that commandline builds extract the frameworks _before_ building (the Xcode finalize
					// happens after building). We still need the unzip in Xcode pre-build phase, because Xcode wants the frameworks around for its build graph
					ExtractFramework(Framework, Graph, Logger);
					FrameworkTokenFiles.Add(Framework.ExtractedTokenFile!);
				}
			}

			CPPOutput Result = new CPPOutput();
			// Create a compile action for each source file.
			foreach (FileItem SourceFile in InputFiles)
			{
				Action CompileAction = CompileCPPFile(CompileEnvironment, SourceFile, OutputDir, ModuleName, Graph, GlobalArguments, Result);
				CompileAction.PrerequisiteItems.UnionWith(FrameworkTokenFiles);
			}
			return Result;
		}

		protected void StripSymbolsWithXcode(FileReference SourceFile, FileReference TargetFile, DirectoryReference ToolchainDir)
		{
			if (SourceFile != TargetFile)
			{
				// Strip command only works in place so we need to copy original if target is different
				File.Copy(SourceFile.FullName, TargetFile.FullName, true);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = Path.Combine(ToolchainDir.FullName, "strip");
			StartInfo.Arguments = String.Format("\"{0}\" -S", TargetFile.FullName);
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo, Logger);
		}

		/// <summary>
		/// Writes a versions.xcconfig file for xcode to pull in when making an app plist
		/// </summary>
		/// <param name="LinkEnvironment"></param>
		/// <param name="Prerequisite">FileItem describing the Prerequisite that this will this depends on (executable or similar) </param>
		/// <param name="Graph">List of actions to be executed. Additional actions will be added to this list.</param>
		protected FileItem UpdateVersionFile(LinkEnvironment LinkEnvironment, FileItem Prerequisite, IActionGraphBuilder Graph)
		{
			FileItem DestFile;

			// Make the compile action
			Action UpdateVersionAction = Graph.CreateAction(ActionType.CreateAppBundle);
			UpdateVersionAction.WorkingDirectory = GetMacDevSrcRoot();
			UpdateVersionAction.CommandPath = BuildHostPlatform.Current.Shell;
			UpdateVersionAction.CommandDescription = "";

			// @todo programs right nhow are sharing the Engine build version - one reason for this is that we can't get to the Engine/Programs directory from here
			// (we can't even get to the Engine/Source/Programs directory without searching on disk), and if we did, we would create a _lot_ of Engine/Programs directories
			// on disk that don't exist in p4. So, we just re-use Engine version, not Project version
			//				DirectoryReference ProductDirectory = FindProductDirectory(ProjectFile, LinkEnvironment.OutputDirectory!, Graph.Makefile.TargetType);
			DirectoryReference ProductDirectory = (ProjectFile?.Directory) ?? Unreal.EngineDirectory;
			FileReference OutputVersionFile = FileReference.Combine(ProductDirectory, "Intermediate/Build/Versions.xcconfig");
			DestFile = FileItem.GetItemByFileReference(OutputVersionFile);

			// grab a changlist version if we have it to pass to the script to use if desired
			int Changelist = 0;
			BuildVersion? Version;
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				Changelist = Version.Changelist;
			}

			// make path to the script
			FileReference VersionScript = FileReference.Combine(ProductDirectory, "Build/BatchFiles/Mac/UpdateVersionAfterBuild.sh");
			if (!FileReference.Exists(VersionScript))
			{
				VersionScript = FileReference.Combine(Unreal.EngineDirectory, "Build/BatchFiles/Mac/UpdateVersionAfterBuild.sh");
			}
			FileItem BundleScript = FileItem.GetItemByFileReference(VersionScript);
			UpdateVersionAction.CommandArguments = $"\"{BundleScript.AbsolutePath}\" \"{ProductDirectory}\" {LinkEnvironment.Platform} {Changelist}";
			UpdateVersionAction.PrerequisiteItems.Add(Prerequisite);
			UpdateVersionAction.PrerequisiteItems.Add(BundleScript);
			UpdateVersionAction.ProducedItems.Add(DestFile);
			UpdateVersionAction.StatusDescription = $"Updating version file: {OutputVersionFile}";

			return DestFile;
		}

		protected FileItem ExtractFramework(UEBuildFramework Framework, IActionGraphBuilder Graph, ILogger Logger)
		{
			if (Framework.ZipFile == null)
			{
				throw new BuildException("Unable to extract framework '{0}' - no zip file specified", Framework.Name);
			}
			if (!Framework.bHasMadeUnzipAction)
			{
				Framework.bHasMadeUnzipAction = true;

				FileItem InputFile = FileItem.GetItemByFileReference(Framework.ZipFile);

				StringBuilder ExtractScript = new StringBuilder();
				ExtractScript.AppendLine("#!/bin/sh");
				ExtractScript.AppendLine("set -e");
				// ExtractScript.AppendLine("set -x"); // For debugging
				ExtractScript.AppendLine(String.Format("[ -d {0} ] && rm -rf {0}", Utils.MakePathSafeToUseWithCommandLine(Framework.ZipOutputDirectory!.FullName)));
				ExtractScript.AppendLine(String.Format("unzip -q -o {0} -d {1}", Utils.MakePathSafeToUseWithCommandLine(Framework.ZipFile.FullName), Utils.MakePathSafeToUseWithCommandLine(Framework.ZipOutputDirectory.ParentDirectory!.FullName))); // Zip contains folder with the same name, hence ParentDirectory
				ExtractScript.AppendLine(String.Format("touch {0}", Utils.MakePathSafeToUseWithCommandLine(Framework.ExtractedTokenFile!.AbsolutePath)));

				FileItem ExtractScriptFileItem = Graph.CreateIntermediateTextFile(new FileReference(Framework.ZipOutputDirectory.FullName + ".sh"), ExtractScript.ToString());

				Action UnzipAction = Graph.CreateAction(ActionType.BuildProject);
				UnzipAction.CommandPath = new FileReference("/bin/sh");
				UnzipAction.CommandArguments = Utils.MakePathSafeToUseWithCommandLine(ExtractScriptFileItem.AbsolutePath);
				UnzipAction.WorkingDirectory = Unreal.EngineDirectory;
				UnzipAction.PrerequisiteItems.Add(InputFile);
				UnzipAction.PrerequisiteItems.Add(ExtractScriptFileItem);
				UnzipAction.ProducedItems.Add(Framework.ExtractedTokenFile);
				UnzipAction.DeleteItems.Add(Framework.ExtractedTokenFile);
				UnzipAction.StatusDescription = String.Format("Unzipping : {0} -> {1}", Framework.ZipFile, Framework.ZipOutputDirectory);
				UnzipAction.bCanExecuteRemotely = false;
			}
			return Framework.ExtractedTokenFile!;
		}

		/// <summary>
		/// If the project is a UnrealGame project, Target.ProjectDirectory refers to the engine dir, not the actual dir of the project. So this method gets the 
		/// actual directory of the project whether it is a UnrealGame project or not.
		/// </summary>
		/// <returns>The actual project directory.</returns>
		/// <param name="ProjectFile">The path to the project file</param>
		internal static DirectoryReference GetActualProjectDirectory(FileReference? ProjectFile)
		{
			DirectoryReference ProjectDirectory = (ProjectFile == null ? Unreal.EngineDirectory : DirectoryReference.FromFile(ProjectFile)!);
			return ProjectDirectory;
		}

		/// <inheritdoc/>
		protected override string EscapePreprocessorDefinition(string Definition)
		{
			return Definition.Contains('"') ? Definition.Replace("\"", "\\\"") : Definition;
		}

		protected override void GetCppStandardCompileArgument(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			if (CompileEnvironment.bEnableObjCAutomaticReferenceCounting)
			{
				Arguments.Add("-fobjc-arc");
			}

			base.GetCppStandardCompileArgument(CompileEnvironment, Arguments);
		}

		protected override void GetCompileArguments_CPP(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c++");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");
			Arguments.Add("-fmodules");
			Arguments.Add("-fno-implicit-modules");
			Arguments.Add("-fimplicit-module-maps");
			Arguments.Add("-Wno-module-import-in-extern-c");
			//Arguments.Add("-fcxx-modules"); // For some reason this does not work
		}

		protected override void GetCompileArguments_MM(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c++");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");
		}

		protected override void GetCompileArguments_M(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");
		}

		protected override void GetCompileArguments_PCH(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c++-header");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
			Arguments.Add("-stdlib=libc++");
			Arguments.Add("-fpch-instantiate-templates");
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_WarningsAndErrors(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_WarningsAndErrors(CompileEnvironment, Arguments);

			Arguments.Add("-Wno-unknown-warning-option");
			Arguments.Add("-Wno-range-loop-analysis");
			Arguments.Add("-Wno-single-bit-bitfield-constant-conversion");

			// Various new warning from Xcode16.3. Disable for now.
			VersionNumber AppleClangVersion = ((AppleToolChainInfo)Info).AppleClangVersion;
			if (AppleClangVersion >= new VersionNumber(17))
			{
				// "implicit conversion loses integer precision:" in many files
				Arguments.Add("-Wno-shorten-64-to-32");
				
				// clang17 is detecting some uses of cxx-extensions,
				// such as: "error: variable length arrays in C++ are a Clang extension" 
				Arguments.Add("-Wno-vla-extension");
				
				// From Engine/Plugins/Runtime/Metasound/Source/MetasoundFrontend/Public/MetasoundPrimitives/Time/AudioBuffers.h
				Arguments.Add("-Wno-extra-qualification");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Optimizations(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Optimizations(CompileEnvironment, Arguments);

			bool bStaticAnalysis = false;
			string? StaticAnalysisMode = Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE");
			if (!String.IsNullOrEmpty(StaticAnalysisMode))
			{
				bStaticAnalysis = true;
			}

			// Optimize non- debug builds.
			if (CompileEnvironment.bOptimizeCode && !bStaticAnalysis)
			{
				// Don't over optimise if using AddressSanitizer or you'll get false positive errors due to erroneous optimisation of necessary AddressSanitizer instrumentation.
				if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer))
				{
					Arguments.Add("-O1");
					Arguments.Add("-g");
					Arguments.Add("-fno-optimize-sibling-calls");
					Arguments.Add("-fno-omit-frame-pointer");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
				{
					Arguments.Add("-O1");
					Arguments.Add("-g");
				}
				else if (CompileEnvironment.OptimizationLevel == OptimizationMode.Size)
				{
					Arguments.Add("-Oz");
				}
				else if (CompileEnvironment.OptimizationLevel == OptimizationMode.SizeAndSpeed)
				{
					Arguments.Add("-Os");

					if (CompileEnvironment.Architecture == UnrealArch.Arm64)
					{
						Arguments.Add("-moutline");
					}
				}
				else
				{
					Arguments.Add("-O3");
				}
			}
			else
			{
				Arguments.Add("-O0");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Debugging(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Debugging(CompileEnvironment, Arguments);

			// Create DWARF format debug info if wanted,
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Arguments.Add("-gdwarf-4");

				if (CompileEnvironment.bDebugLineTablesOnly)
				{
					Arguments.Add("-gline-tables-only");
				}
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Analyze(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Analyze(CompileEnvironment, Arguments);

			// Disable all clang tidy checks
			Arguments.Add($"-Xclang -analyzer-tidy-checker=-*");
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Global(CompileEnvironment, Arguments);

			Arguments.Add(GetRTTIFlag(CompileEnvironment));

			Arguments.Add("-fmessage-length=0");
			Arguments.Add("-fpascal-strings");

			string? Override = (CompileEnvironment.bEnableOSX109Support && CompileEnvironment.Platform == UnrealTargetPlatform.Mac) ? "10.9" : null;
			Arguments.Add($"-target {ToolChainSettings.Value.GetTargetTuple(CompileEnvironment.Architecture, Target!.Platform, Target!.Type, Override)}");
		}

		public override CppCompileEnvironment CreateSharedResponseFile(CppCompileEnvironment CompileEnvironment, FileReference OutResponseFile, IActionGraphBuilder Graph)
		{
			// Temporarily turn of shared response files for apple toolchains
			return CompileEnvironment;
		}

		private FileItem GetBridgingHeader(string SourceFile, DirectoryReference OutputDir)
		{
			string Filename = $"{Path.GetFileNameWithoutExtension(SourceFile)}-Swift.h";
			return FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, "Bridging", Filename));
		}

		protected override Action CompileCPPFile(CppCompileEnvironment CompileEnvironment, FileItem SourceFile, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph, IReadOnlyCollection<string> GlobalArguments, CPPOutput Result)
		{
			if (SourceFile.HasExtension(".swift"))
			{
				return CompileSwiftFile(CompileEnvironment, SourceFile, OutputDir, Graph, Result);
			}

			return base.CompileCPPFile(CompileEnvironment, SourceFile, OutputDir, ModuleName, Graph, GlobalArguments, Result);
		}

		private void GetSharedSwiftArguments(CppCompileEnvironment CompileEnvironment, List<string> Arguments, string ModuleName)
		{
			// TODO: Fix so swiftc path supports vfs
			CppRootPaths rootPaths = new(CompileEnvironment.RootPaths);
			rootPaths.bUseVfs = false;

			Arguments.Add($"-target {ToolChainSettings.Value.GetTargetTuple(CompileEnvironment.Architecture, Target!.Platform, Target!.Type)}");
			Arguments.Add($"-sdk {ToolChainSettings.Value.GetSDKPath(CompileEnvironment.Architecture)}");

			Arguments.Add($"-swift-version 5");
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Arguments.Add($"-g");
			}

			Arguments.Add($"-module-name {ModuleName}Swift");

			if (CompileEnvironment.ForceIncludeFiles.Count() > 0)
			{
				Arguments.Add($"-import-objc-header \"{NormalizeCommandLinePath(CompileEnvironment.ForceIncludeFiles.First(), rootPaths)}\"");
			}

			// tell swift to define PLATFORM_FOO, and tell C++ to define PLATFORM_FOO=1
			// swift doesn't pass it's defines along to C++ when importing a C header, and it also doesn't have values for defines
			Arguments.Add($"-DPLATFORM_{Target.Platform.ToString().ToUpper()}");
			Arguments.Add($"-Xcc -DPLATFORM_{Target.Platform.ToString().ToUpper()}=1");
			if (bUseSwiftUIMain)
			{
				Arguments.Add("-DUE_USE_SWIFT_UI_MAIN");
				if (ToolChainSettings.Value.SDKVersionFloat < 2.0)
				{
					Arguments.Add("-DUE_SDK_VERSION_1");
				}
			}

			// pass the UE definitions to C++ land for the imported obj-c header
			Arguments.AddRange(CompileEnvironment.Definitions.Select(x => $"-Xcc -D{x}"));

			Console.WriteLine("Arguments: {0}", string.Join(", ", Arguments));
		}

		private Action CompileSwiftFile(CppCompileEnvironment CompileEnvironment, FileItem SourceFile, DirectoryReference OutputDir, IActionGraphBuilder Graph, CPPOutput CompileResult)
		{
			string ModuleName = Path.GetFileNameWithoutExtension(SourceFile.FullName);
			FileItem OutputFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, GetFileNameFromExtension(SourceFile.Name, ".o")));

			List<string> Arguments = new();
			Arguments.Add($"\"{SourceFile}\"");

			GetSharedSwiftArguments(CompileEnvironment, Arguments, ModuleName);
			// output file settings
			Arguments.Add("-emit-object"); // same as -c
			Arguments.Add($"-parse-as-library"); // don't create a main function (even if we use the SwiftUI @main, this still seems to be correctly working)
			Arguments.Add($"-o \"{OutputFile}\"");

			// platform settings
			Arguments.Add($"-target {Settings.GetTargetTuple(CompileEnvironment.Architecture, Target!.Platform, Target!.Type)}");
			Arguments.Add($"-sdk {Settings.GetSDKPath(CompileEnvironment.Architecture)}");

			// misc settings copied from Xcode
			Arguments.Add("-Xllvm");
			Arguments.Add("-aarch64-use-tbi");
			Arguments.Add("-stack-check");

			Arguments.Add("-enable-objc-interop");
			Arguments.Add("-cxx-interoperability-mode=default");

			Action CompileAction = Graph.CreateAction(ActionType.Compile);
			CompileAction.Weight = CompileActionWeight;
			CompileAction.CommandArguments = String.Join(" ", Arguments);
			CompileAction.CommandPath = FileReference.Combine(ToolChainSettings.Value.ToolchainDir, "swift-frontend");
			CompileAction.PrerequisiteItems.Add(SourceFile);
			CompileAction.ProducedItems.Add(OutputFile);
			CompileResult.ObjectFiles.Add(OutputFile);
			CompileAction.WorkingDirectory = Unreal.EngineSourceDirectory;
			CompileAction.CommandDescription = "Compile";
			UnrealArchitectureConfig ArchConfig = UnrealArchitectureConfig.ForPlatform(CompileEnvironment.Platform);
			if (ArchConfig.Mode != UnrealArchitectureMode.SingleArchitecture)
			{
				string ReadableArch = ArchConfig.ConvertToReadableArchitecture(CompileEnvironment.Architecture);
				CompileAction.CommandDescription += $" [{ReadableArch}]";
			}
			CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
			CompileAction.bIsGCCCompiler = true;
			CompileAction.bCanExecuteRemotely = true;
			CompileAction.bCanExecuteInUBA = false;


			if (CompileEnvironment.ForceIncludeFiles.Count() > 0)
			{
				FileItem OutputInteropHeader = GetBridgingHeader(SourceFile.FullName, OutputDir);

				// obj-c bridging header settings
				Arguments.Clear();
				Arguments.Add($"\"{SourceFile}\"");
				GetSharedSwiftArguments(CompileEnvironment, Arguments, ModuleName);

				Arguments.Add("-parse"); // this will allow it to generate the header without writing out any .o/executable
				Arguments.Add("-emit-objc-header");
				Arguments.Add($"-emit-objc-header-path \"{OutputInteropHeader}\"");
				Arguments.Add("-parse-as-library");

				//Arguments.Add("-enable-objc-interop");
				//Arguments.Add("-cxx-interoperability-mode=default");


				// now make an action to export the swift code as a header Obj-C bridging
				Action HeaderAction = Graph.CreateAction(ActionType.Compile);
				HeaderAction.CommandPath = FileReference.Combine(ToolChainSettings.Value.ToolchainDir, "swiftc");
				HeaderAction.CommandArguments = String.Join(" ", Arguments);
				HeaderAction.PrerequisiteItems.Add(SourceFile);
				HeaderAction.ProducedItems.Add(OutputInteropHeader);
				// swiftc won't touch the output header if the generated content doesn't change, so always delete the output so that 
				// the timestamp is correct (it must be newer than the input .swift file, or it will build over and over)
				HeaderAction.DeleteItems.Add(OutputInteropHeader);
				CompileResult.GeneratedHeaderFiles.Add(OutputInteropHeader);
				HeaderAction.CommandDescription = "Generate SwiftToCPP Header";
				HeaderAction.StatusDescription = Path.GetFileName(OutputInteropHeader.AbsolutePath);
				if (ArchConfig.Mode != UnrealArchitectureMode.SingleArchitecture)
				{
					string ReadableArch = ArchConfig.ConvertToReadableArchitecture(CompileEnvironment.Architecture);
					HeaderAction.CommandDescription += $" [{ReadableArch}]";
				}

				HeaderAction.WorkingDirectory = CompileAction.WorkingDirectory;
				HeaderAction.bIsGCCCompiler = CompileAction.bIsGCCCompiler;
				HeaderAction.bCanExecuteRemotely = CompileAction.bCanExecuteRemotely;
				HeaderAction.bCanExecuteInUBA = CompileAction.bCanExecuteInUBA;
			}
			// this is likely ignored, but the Compile action is the important one
			return CompileAction;
		}

		public override ICollection<FileItem> PostBuild(ReadOnlyTargetRules Target, FileItem Executable, LinkEnvironment BinaryLinkEnvironment, IActionGraphBuilder Graph)
		{
			List<FileItem> OutputFiles = new List<FileItem>(base.PostBuild(Target, Executable, BinaryLinkEnvironment, Graph));

			bool bIsBuildingAppBundle = !BinaryLinkEnvironment.bIsBuildingDLL && !BinaryLinkEnvironment.bIsBuildingLibrary && !BinaryLinkEnvironment.bIsBuildingConsoleApplication;
			if (AppleExports.UseModernXcode(Target.ProjectFile) && bIsBuildingAppBundle)
			{
				Action PostBuildAction = ApplePostBuildSyncMode.CreatePostBuildSyncAction(Target, Executable, BinaryLinkEnvironment.IntermediateDirectory!, Graph, AppleExports.ForceNoEntitlements());

				PostBuildAction.PrerequisiteItems.UnionWith(OutputFiles);
				OutputFiles.AddRange(PostBuildAction.ProducedItems);

				OutputFiles.Add(UpdateVersionFile(BinaryLinkEnvironment, FileItem.GetItemByFileReference(BinaryLinkEnvironment.OutputFilePath), Graph));
			}

			return OutputFiles;
		}

		protected virtual void GetLinkArguments_Global(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			VersionNumber AppleClangVersion = ((AppleToolChainInfo)Info).AppleClangVersion;
			// Temp solution for UE-191350
			if (AppleClangVersion >= new VersionNumber(15) && AppleClangVersion < new VersionNumber(16))
			{
				Arguments.Add(" -ld_classic");
			}

			if (!LinkEnvironment.bIsBuildingDLL)
			{
				// Todo: This is currently only used when linking binary and swift files have not been tested with dylibs.
				// The right fix is to move the vfs root one step up to be able to handle the ../lib/ path
				DirectoryReference dir = DirectoryReference.Combine(ApplePlatformSDK.GetToolchainDirectory(), "../lib/swift", Settings.PlatformDirName);
				Arguments.Add($"-L{dir}");
			}

			Arguments.Add("-L/usr/lib/swift");
		}

		#region Stub Xcode Projects

		internal static bool GenerateProjectFiles(FileReference? ProjectFile, string[] Arguments, string? SingleTargetName, ILogger Logger, out DirectoryReference? XcodeProjectFile)
		{
			ProjectFileGenerator.bGenerateProjectFiles = true;
			try
			{
				CommandLineArguments CmdLine = new CommandLineArguments(Arguments);

				PlatformProjectGeneratorCollection PlatformProjectGenerators = new PlatformProjectGeneratorCollection();
				PlatformProjectGenerators.RegisterPlatformProjectGenerator(UnrealTargetPlatform.Mac, new MacProjectGenerator(CmdLine, Logger), Logger);
				PlatformProjectGenerators.RegisterPlatformProjectGenerator(UnrealTargetPlatform.IOS, new IOSProjectGenerator(CmdLine, Logger), Logger);
				PlatformProjectGenerators.RegisterPlatformProjectGenerator(UnrealTargetPlatform.TVOS, new TVOSProjectGenerator(CmdLine, Logger), Logger);

				XcodeProjectFileGenerator Generator = new XcodeProjectFileGenerator(ProjectFile, CmdLine);
				// this could be improved if ProjectFileGenerator constructor took a Arguments param, and it could parse it there instead of the GenerateProjectFilesMode
				Generator.SingleTargetName = SingleTargetName;
				// don't need the editor data since these are stub projects
				ProjectFileGenerator.Current = Generator;
				bool bSucces = Generator.GenerateProjectFiles(PlatformProjectGenerators, Arguments, Logger);
				ProjectFileGenerator.Current = null;
				XcodeProjectFile = Generator.XCWorkspace;
				return bSucces;
			}
			catch (Exception ex)
			{
				XcodeProjectFile = null;
				Logger.LogError(ex.ToString());
			}
			finally
			{
				ProjectFileGenerator.bGenerateProjectFiles = false;
			}
			return false;
		}

		/// <summary>
		/// Genearate an run-only Xcode project, that is not meant to be used for anything else besides code-signing/running/etc of the native .app bundle
		/// </summary>
		/// <param name="UProjectFile">Location of .uproject file (or null for the engine project</param>
		/// <param name="Platform">The platform to generate a project for</param>
		/// <param name="TargetName">The name of the target being built, so we can generate a more minimal project</param>
		/// <param name="bForDistribution">True if this is making a bild for uploading to app store</param>
		/// <param name="bNoEntitlements">True if we shouldn't use entitlements</param>
		/// <param name="Logger">Logging object</param>
		/// <param name="GeneratedProjectFile">Returns the .xcworkspace that was made</param>
		internal static void GenerateRunOnlyXcodeProject(FileReference? UProjectFile, UnrealTargetPlatform Platform, string TargetName, bool bForDistribution, bool bNoEntitlements, ILogger Logger, out DirectoryReference? GeneratedProjectFile)
		{
			List<string> Options = new()
			{
				$"-platforms={Platform}",
				"-DeployOnly",
				"-NoIntellisense",
				"-NoDotNet",
				"-IgnoreJunk",
				bNoEntitlements ? "-noEntitlements" : "",
				bForDistribution ? "-distribution" : "-development",
				"-IncludeTempTargets",
				"-projectfileformat=XCode",
				"-automated",
			};

			if (!String.IsNullOrEmpty(TargetName))
			{
				Options.Add($"-singletarget={TargetName}");
			}

			if (UProjectFile == null || UProjectFile.IsUnderDirectory(Unreal.EngineDirectory))
			{
				// @todo do we need these? where would the bundleid come from if there's no project?
				//				Options.Add("-bundleID=" + BundleID);
				//				Options.Add("-appname=" + AppName);
				// @todo add an option to only add Engine target?
			}
			else
			{
				Options.Add($"-project=\"{UProjectFile.FullName}\"");
				Options.Add("-game");
			}

			// we need to be in Engine/Source for some build.cs files
			string CurrentCWD = Environment.CurrentDirectory;
			Environment.CurrentDirectory = DirectoryReference.Combine(Unreal.EngineDirectory, "Source").FullName;
			GenerateProjectFiles(UProjectFile, Options.ToArray(), TargetName, Logger, out GeneratedProjectFile);
			Environment.CurrentDirectory = CurrentCWD;
		}

		static string IdOrNameParam(string Id)
		{
			string OldFormat = "^[0-9,a-f]{40}$";
			string NewFormat = "^[0-9,A-F]{8}-[0-9,A-F]{16}$";
			if (Regex.Match(Id, OldFormat).Success || Regex.Match(Id, NewFormat).Success)
			{
				return $"id={Id}";
			}
			return $"name={Id}";
		}

		internal static int FinalizeAppWithXcode(DirectoryReference XcodeProject, UnrealTargetPlatform Platform, UnrealArchitectures Architectures, bool bUseAutomaticCodeSigning, string SchemeName, string Configuration, string Action, string ExtraOptions, ILogger Logger, List<string>? DestinationIds=null)
		{
			// Acquire a different mutex to the regular UBT instance, since this mode will be called as part of a build. We need the mutex to ensure that building two modular configurations 
			// in parallel don't clash over writing shared *.modules files (eg. DebugGame and Development editors).
			string MutexName = GlobalSingleInstanceMutex.GetUniqueMutexForPath("UnrealBuildTool_XcodeBuild", Unreal.RootDirectory);
			using (new GlobalSingleInstanceMutex(MutexName, true))
			{
				string DestinationParam;
				// use generic:
				//   - for Mac, the destination seems to want name="My Mac", not the real name of the Mac
				//   - when no destinations were supplied
				//   - when trying to build iOS app to run on local mac 
				//   - when using manual codesigning because the main use of this is to update provisioning information (reducing potential for issues)
				if (Platform == UnrealTargetPlatform.Mac || DestinationIds == null || DestinationIds.Count == 0 || DestinationIds[0] == "" || DestinationIds[0].Contains(AppleToolChainSettings.LocalMacUUID) || !bUseAutomaticCodeSigning)
				{
					DestinationParam = $"-destination generic/platform=\"{AppleExports.GetDestinationPlatform(Platform, Architectures)}\"";
				}
				else
				{
					string DestPlatform = AppleExports.GetDestinationPlatform(Platform, Architectures);
					DestinationParam = string.Join(" ", DestinationIds.Select(x => $"-destination \"platform={DestPlatform},{IdOrNameParam(x)}\""));
				}
				List<string> Arguments = new()
				{
					"UBT_NO_POST_DEPLOY=true",
					FileReference.Combine(ApplePlatformSDK.DeveloperDir, "usr/bin/xcodebuild").FullName,
					Action,
					$"-workspace \"{XcodeProject.FullName}\"",
					$"-scheme \"{SchemeName}\"",
					$"-configuration \"{Configuration}\"",
					DestinationParam,
					"-hideShellScriptEnvironment",
					// xcode gets confused it we _just_ wrote out entitlements while generating the temp project, and it thinks it was modified _during_ building
					// but it wasn't, it was written before the build started
					"CODE_SIGN_ALLOW_ENTITLEMENTS_MODIFICATION=YES",
					ExtraOptions,
					//$"-sdk {SDKName}",
				};

				Console.WriteLine("params: {0}", String.Join(" ", Arguments));

				Process LocalProcess = new Process();
				LocalProcess.StartInfo = new ProcessStartInfo("/usr/bin/env", String.Join(" ", Arguments));
				LocalProcess.OutputDataReceived += (Sender, Args) => { LocalProcessOutput(Args, false, Logger); };
				LocalProcess.ErrorDataReceived += (Sender, Args) =>
				{
					if (Args != null && Args.Data != null
					&& Args.Data.Contains("Failed to load profile") && Args.Data.Contains("<stdin>"))
					{
						Logger.LogInformation("Silencing the following provision profile error, it is not affecting code signing:");
						LocalProcessOutput(Args, false, Logger);
					}
					else
					{
						LocalProcessOutput(Args, true, Logger);
					}
				};
				return Utils.RunLocalProcess(LocalProcess);
			}
		}

		static void LocalProcessOutput(DataReceivedEventArgs? Args, bool bIsError, ILogger Logger)
		{
			if (Args != null && Args.Data != null)
			{
				if (bIsError)
				{
					Logger.LogError("{Message}", Args.Data.TrimEnd());
				}
				else
				{
					Logger.LogInformation("{Message}", Args.Data.TrimEnd());
				}
			}
		}

		#endregion
	};

	[Serializable]
	class ApplePostBuildSyncTarget
	{
		public FileReference? ProjectFile;
		public UnrealTargetPlatform Platform;
		public UnrealArchitectures Architectures;
		public UnrealTargetConfiguration Configuration;
		public string TargetName;

		// For iOS/TVOS
		public bool bCreateStubIPA;
		public string? RemoteImportProvision;
		public string? RemoteImportCertificate;
		public string? RemoteImportCertificatePassword;
		public DirectoryReference ProjectIntermediateDirectory;
		public FileReference StubOutputPath;

		public ApplePostBuildSyncTarget(ReadOnlyTargetRules Target, FileItem Executable, DirectoryReference IntermediateDir)
		{
			Platform = Target.Platform;
			Configuration = Target.Configuration;
			Architectures = Target.Architectures;
			ProjectFile = Target.ProjectFile;
			TargetName = Target.Name;

			bCreateStubIPA = Target.IOSPlatform.bCreateStubIPA;
			RemoteImportProvision = Target.IOSPlatform.ImportProvision;
			RemoteImportCertificate = Target.IOSPlatform.ImportCertificate;
			RemoteImportCertificatePassword = Target.IOSPlatform.ImportCertificatePassword;
			ProjectIntermediateDirectory = IntermediateDir;

			StubOutputPath = Executable.Location;
		}
	}

	[ToolMode("ApplePostBuildSync", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms)]
	class ApplePostBuildSyncMode : ToolMode
	{
		[CommandLine("-Input=", Required = true)]
		public FileReference? InputFile = null;

		[CommandLine("-XmlConfigCache=")]
		public FileReference? XmlConfigCache = null;

		// this isn't actually used, but is helpful to pass -modernxcode along in CreatePostBuildSyncAction, and UBT won't
		// complain that nothing is using it, because where we _do_ use it is outside the normal cmdline parsing functionality
		[CommandLine("-ModernXcode")]
		public bool bModernXcode;

		[CommandLine("-noEntitlements")]
		public bool bNoEntitlements;

		public override Task<int> ExecuteAsync(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);
			Arguments.CheckAllArgumentsUsed();

			// Run the PostBuildSync command
			ApplePostBuildSyncTarget Target = BinaryFormatterUtils.Load<ApplePostBuildSyncTarget>(InputFile!);
			int ExitCode = PostBuildSync(Target, Logger);

			return Task.FromResult(ExitCode);
		}

		private int PostBuildSync(ApplePostBuildSyncTarget Target, ILogger Logger)
		{
			// generate the IOS plist file every time
			if (Target.Platform.IsInGroup(UnrealPlatformGroup.IOS))
			{
				string GameName = Target.ProjectFile == null ? "UnrealGame" : Target.ProjectFile.GetFileNameWithoutAnyExtensions();
				// most of these params are uused in modern
				UEDeployIOS.GenerateIOSPList(Target.ProjectFile, Target.Configuration, AppleToolChain.GetActualProjectDirectory(Target.ProjectFile).FullName, Target.ProjectFile == null, GameName, bIsClient: false,
					GameName, Unreal.EngineDirectory.FullName, "", null, null, false, Logger);
			}

			// if xcode is building this, it will also do the Run stuff anyway, so no need to do it here as well
			if (Environment.GetEnvironmentVariable("UE_BUILD_FROM_XCODE") == "1")
			{
				return 0;
			}

			string ExtraOptions = "";
			// for mobile builds (which need real codesigning to be able to run), we use dummy codesigning when making a .stub (which will
			// be sent to Windows an re-codesigned), or when making UnrealGame.app without a .uproject (we will always make a .app
			// again with a .uproject on the commandline to be able to get the staged data that will be pulled in to the app - we can't run 
			// without a Staged directory, so this dummy codesigned .app won't be used directly)

			// NOTE: Actually for _now_ we are using legacy-style signing with temp keychain because IPhonePackager cannot codesign
			// Frameworks, so we have to do full non-dummy signing of stubs until we get IPP working
			bool bUseDummySigning = Target.Platform != UnrealTargetPlatform.Mac && (Target.ProjectFile == null);
			bool bCreateStub = Target.Platform.IsInGroup(UnrealPlatformGroup.IOS) && Target.bCreateStubIPA;
			// if we want dummy signing (no project at all) then we don't want to use legacy signing - that is only used for remote builds from Windows
			bool bUseLegacyStubSigning = bCreateStub && !bUseDummySigning;

			if (bUseLegacyStubSigning)
			{
				// create and run a script that will make a temp keychain, and return the options needed to pass to xcodebuild to use it
				ExtraOptions += SetupRemoteCodesigning(Target);
			}

			int ExitCode = AppleExports.BuildWithStubXcodeProject(Target.ProjectFile, Target.Platform, Target.Architectures, Target.Configuration, Target.TargetName,
				AppleExports.XcodeBuildMode.PostBuildSync, Logger, ExtraOptions, bForceDummySigning: bUseDummySigning);

			// restore the keychain as soon as possible
			if (bUseLegacyStubSigning)
			{
				// cleanup the 
				CleanupRemoteCodesigning(Target);
			}

			if (ExitCode != 0)
			{
				Logger.LogError("ERROR: Failed to finalize the .app with Xcode. Check the log for more information");
			}

			if (bCreateStub)
			{
				IOSToolChain.PackageStub(Target.StubOutputPath.Directory.FullName, Target.TargetName, Target.StubOutputPath.GetFileNameWithoutExtension(), true, !bUseLegacyStubSigning);

			}

			return ExitCode;
		}

		// This is hopefully until we can get codesigning of Frameworks working in IPhonePackager, then we can go back to dummy codesigning, without needing
		// to mess with keychains and what not
		private static string SetupRemoteCodesigning(ApplePostBuildSyncTarget Target)
		{
			FileReference TempKeychain = FileReference.Combine(Target.ProjectIntermediateDirectory!, "TempKeychain.keychain");
			FileReference SignProjectScript = FileReference.Combine(Target.ProjectIntermediateDirectory!, "SignProject.sh");
			string MobileProvisionUUID = "";
			string SigningCertificate = "";

			using (StreamWriter Writer = new StreamWriter(SignProjectScript.FullName))
			{
				// Boilerplate
				Writer.WriteLine("#!/bin/sh");
				Writer.WriteLine("set -e");
				Writer.WriteLine("set -x");
				// Copy the mobile provision into the system store
				if (Target.RemoteImportProvision == null || Target.RemoteImportCertificate == null)
				{
					throw new BuildException("Expecting stub to be run with -ImportCertificate and -ImportProvision when using modern xcode");
				}

				// copy the provision into standard location
				Writer.WriteLine("cp -f {0} ~/Library/MobileDevice/Provisioning\\ Profiles/", Utils.EscapeShellArgument(Target.RemoteImportProvision));
				MobileProvisionContents MobileProvision = MobileProvisionContents.Read(new FileReference(Target.RemoteImportProvision));
				MobileProvisionUUID = MobileProvision.GetUniqueId();

				// Get the signing certificate to use
				X509Certificate2 Certificate;
				try
				{
					Certificate = new X509Certificate2(Target.RemoteImportCertificate, Target.RemoteImportCertificatePassword ?? "");
				}
				catch (Exception Ex)
				{
					throw new BuildException(Ex, "Unable to read certificate '{0}': {1}", Target.RemoteImportCertificate, Ex.Message);
				}
				// Read the name from the certificate
				SigningCertificate = Certificate.GetNameInfo(X509NameType.SimpleName, false);

				// Install a certificate given on the command line to a temporary keychain
				Writer.WriteLine("security delete-keychain \"{0}\" || true", TempKeychain);
				Writer.WriteLine("security create-keychain -p \"A\" \"{0}\"", TempKeychain);
				Writer.WriteLine("security list-keychains -s \"{0}\"", TempKeychain);
				Writer.WriteLine("security list-keychains");
				Writer.WriteLine("security set-keychain-settings -t 3600 -l  \"{0}\"", TempKeychain);
				Writer.WriteLine("security -v unlock-keychain -p \"A\" \"{0}\"", TempKeychain);
				Writer.WriteLine("security import {0} -P {1} -k \"{2}\" -T /usr/bin/codesign -T /usr/bin/security -t agg", Utils.EscapeShellArgument(Target.RemoteImportCertificate), Utils.EscapeShellArgument(Target.RemoteImportCertificatePassword!), TempKeychain);
				Writer.WriteLine("security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k \"A\" -D '{0}' -t private {1}", SigningCertificate, TempKeychain);
			}

			// run the script
			Utils.RunLocalProcessAndReturnStdOut("/bin/sh", $"\"{SignProjectScript.FullName}\"");

			// Set parameters to make sure it uses the correct identity and keychain
			// pass back the comandline arguments to xcodebuild to use these certicicates
			return $" CODE_SIGN_STYLE=Manual CODE_SIGN_IDENTITY=\"{SigningCertificate}\" PROVISIONING_PROFILE_SPECIFIER={MobileProvisionUUID}";
		}

		private static void CleanupRemoteCodesigning(ApplePostBuildSyncTarget Target)
		{
			FileReference TempKeychain = FileReference.Combine(Target.ProjectIntermediateDirectory!, "TempKeychain.keychain");

			FileReference CleanProjectScript = FileReference.Combine(Target.ProjectIntermediateDirectory!, "CleanProject.sh");
			using (StreamWriter CleanWriter = new StreamWriter(CleanProjectScript.FullName))
			{
				CleanWriter.WriteLine("#!/bin/sh");
				CleanWriter.WriteLine("set -e");
				CleanWriter.WriteLine("set -x");
				// Remove the temporary keychain from the search list
				CleanWriter.WriteLine("security delete-keychain \"{0}\" || true", TempKeychain);
				// Restore the login keychain as active
				CleanWriter.WriteLine("security list-keychain -s login.keychain");
			}

			Utils.RunLocalProcessAndReturnStdOut("/bin/sh", $"\"{CleanProjectScript.FullName}\"");
		}

		private static FileItem GetPostBuildOutputFile(FileReference Executable, string TargetName, UnrealTargetPlatform Platform)
		{
			FileReference StagedExe;
			if (Platform == UnrealTargetPlatform.Mac)
			{
				StagedExe = FileReference.Combine(Executable.Directory, Executable.GetFileName() + ".app/Contents/PkgInfo");
			}
			else
			{
				StagedExe = FileReference.Combine(Executable.Directory, TargetName + ".app", TargetName);
			}
			return FileItem.GetItemByFileReference(StagedExe);
		}

		public static Action CreatePostBuildSyncAction(ReadOnlyTargetRules Target, FileItem Executable, DirectoryReference IntermediateDir, IActionGraphBuilder Graph, bool bNoEntitlements = false)
		{
			ApplePostBuildSyncTarget PostBuildSync = new(Target, Executable, IntermediateDir);
			FileReference PostBuildSyncFile = FileReference.Combine(IntermediateDir!, "PostBuildSync.dat");
			BinaryFormatterUtils.Save(PostBuildSyncFile, PostBuildSync);			
			string PostBuildSyncArguments = String.Format("-modernxcode -Input=\"{0}\" -XmlConfigCache=\"{1}\" -remoteini=\"{2}\" {3}", PostBuildSyncFile, XmlConfig.CacheFile, UnrealBuildTool.GetRemoteIniPath(), bNoEntitlements ? "-noEntitlements" : "");
			Action PostBuildSyncAction = Graph.CreateRecursiveAction<ApplePostBuildSyncMode>(ActionType.CreateAppBundle, PostBuildSyncArguments);
			PostBuildSyncAction.PrerequisiteItems.Add(Executable);
			PostBuildSyncAction.ProducedItems.Add(GetPostBuildOutputFile(Executable.Location, Target.Name, Target.Platform));
			PostBuildSyncAction.StatusDescription = $"Executing PostBuildSync [{Executable.Location}]";

			if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
			{
				// @todo: do we need one per Target for IOS? Client? I dont think so for Modern
				FileReference PlistFile = FileReference.Combine(AppleToolChain.GetActualProjectDirectory(Target.ProjectFile), "Build/IOS/UBTGenerated/Info.Template.plist");
				PostBuildSyncAction.ProducedItems.Add(FileItem.GetItemByFileReference(PlistFile));

				if (PostBuildSync.bCreateStubIPA)
				{
					FileReference StubFile = FileReference.Combine(Executable.Directory.Location, Executable.Location.GetFileNameWithoutExtension() + ".stub");
					PostBuildSyncAction.ProducedItems.Add(FileItem.GetItemByFileReference(StubFile));
				}
			}

			return PostBuildSyncAction;
		}
	}
}
