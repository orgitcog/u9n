// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	class LinuxToolChain : ClangToolChain
	{
		protected class LinuxToolChainInfo : ClangToolChainInfo
		{
			// cache the location of NDK tools
			public bool bIsCrossCompiling { get; init; }
			public DirectoryReference? BaseLinuxPath { get; init; }
			public DirectoryReference? MultiArchRoot { get; init; }

			public FileReference Objcopy { get; init; }
			public FileReference DumpSyms { get; init; }
			public FileReference BreakpadEncoder { get; init; }

			public LinuxToolChainInfo(DirectoryReference? BaseLinuxPath, DirectoryReference? MultiArchRoot, FileReference Clang, FileReference Archiver, FileReference Objcopy, ILogger Logger)
				: base(BaseLinuxPath, Clang, Archiver, Logger)
			{
				this.BaseLinuxPath = BaseLinuxPath;
				this.MultiArchRoot = MultiArchRoot;
				this.Objcopy = Objcopy;
				// these are supplied by the engine and do not change depending on the circumstances
				DumpSyms = FileReference.Combine(Unreal.EngineDirectory, "Binaries", "Linux", $"dump_syms{BuildHostPlatform.Current.BinarySuffix}");
				BreakpadEncoder = FileReference.Combine(Unreal.EngineDirectory, "Binaries", "Linux", $"BreakpadSymbolEncoder{BuildHostPlatform.Current.BinarySuffix}");
			}
		}

		/** Flavor of the current build (will map to target triplet)*/
		UnrealArch Architecture;

		/** Pass --gdb-index option to linker to generate .gdb_index section. */
		protected bool bGdbIndexSection = true;

		/** Allows you to override the maximum binary size allowed to be passed to objcopy.exe when cross building on Windows. */
		/** Max value is 2GB, due to bat file limitation */
		protected ulong MaxBinarySizeOverrideForObjcopy = 0;

		/** Platform SDK to use */
		protected LinuxPlatformSDK PlatformSDK;

		protected LinuxToolChainInfo LinuxInfo => (Info as LinuxToolChainInfo)!;

		public LinuxToolChain(UnrealArch InArchitecture, LinuxPlatformSDK InSDK, ClangToolChainOptions InOptions, ILogger InLogger)
			: this(UnrealTargetPlatform.Linux, InArchitecture, InSDK, InOptions, InLogger)
		{
			// prevent unknown clangs since the build is likely to fail on too old or too new compilers
			if ((CompilerVersionLessThan(18, 0, 0) || CompilerVersionGreaterOrEqual(19, 0, 0)) && !Options.HasFlag(ClangToolChainOptions.UseAutoRTFMCompiler))
			{
				throw new BuildException(
					String.Format("This version of the Unreal Engine can only be compiled with clang 18.x. clang {0} may not build it - please use a different version.",
						Info.ClangVersion)
					);
			}
		}

		public LinuxToolChain(UnrealTargetPlatform InPlatform, UnrealArch InArchitecture, LinuxPlatformSDK InSDK, ClangToolChainOptions InOptions, ILogger InLogger)
			: base(InOptions, InLogger)
		{
			Architecture = InArchitecture;
			PlatformSDK = InSDK;
		}

		protected override ClangToolChainInfo GetToolChainInfo()
		{
			DirectoryReference? MultiArchRoot = PlatformSDK.GetSDKLocation();
			DirectoryReference? BaseLinuxPath = PlatformSDK.GetBaseLinuxPathForArchitecture(Architecture);

			bool bForceUseSystemCompiler = PlatformSDK.ForceUseSystemCompiler();

			if (bForceUseSystemCompiler)
			{
				// use native linux toolchain
				FileReference? ClangPath = FileReference.FromString(LinuxCommon.WhichClang(Logger));
				FileReference? LlvmArPath = FileReference.FromString(LinuxCommon.Which("llvm-ar", Logger));
				FileReference? ObjcopyPath = FileReference.FromString(LinuxCommon.Which("llvm-objcopy", Logger));

				if (ClangPath == null)
				{
					throw new BuildException("Unable to find system clang; cannot instantiate Linux toolchain");
				}
				else if (LlvmArPath == null)
				{
					throw new BuildException("Unable to find system llvm-ar; cannot instantiate Linux toolchain");
				}
				else if (ObjcopyPath == null)
				{
					throw new BuildException("Unable to find system llvm-objcopy; cannot instantiate Linux toolchain");
				}

				bIsCrossCompiling = false;

				return new LinuxToolChainInfo(null, null, ClangPath, LlvmArPath, ObjcopyPath, Logger);
			}
			else
			{
				if (BaseLinuxPath == null)
				{
					throw new BuildException("LINUX_MULTIARCH_ROOT environment variable is not set; cannot instantiate Linux toolchain");
				}
				if (MultiArchRoot == null)
				{
					MultiArchRoot = BaseLinuxPath;
					Logger.LogInformation("Using LINUX_ROOT (deprecated, consider LINUX_MULTIARCH_ROOT)");
				}

				// set up the path to our toolchain
				FileReference ClangPath = FileReference.Combine(BaseLinuxPath, "bin", $"clang++{BuildHostPlatform.Current.BinarySuffix}");
				FileReference LlvmArPath = FileReference.Combine(BaseLinuxPath, "bin", $"llvm-ar{BuildHostPlatform.Current.BinarySuffix}");
				FileReference ObjcopyPath = FileReference.Combine(BaseLinuxPath, "bin", $"llvm-objcopy{BuildHostPlatform.Current.BinarySuffix}");

				// if we have RTFMCompiler enabled switch the compiler but leave the sysroot to use our toolchain
				// this *shouldnt* cause issues as these compiler should ideally be the same version
				if (Options.HasFlag(ClangToolChainOptions.UseAutoRTFMCompiler))
				{
					DirectoryReference AutoRTFMDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Restricted", "NotForLicensees", "Binaries", BuildHostPlatform.Current.Platform.ToString(), "AutoRTFM", "bin");
					
					// set up the path to our toolchain
					if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
					{
						ClangPath = FileReference.Combine(AutoRTFMDir, $"verse-clang++{BuildHostPlatform.Current.BinarySuffix}");
					}
					else
					{
						// We use verse-clang-cl when the host is Windows just to keep the number of compilers deployed to a minimum.
						ClangPath = FileReference.Combine(AutoRTFMDir, $"verse-clang-cl{BuildHostPlatform.Current.BinarySuffix}");
					}
				}

				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
				{
					Environment.SetEnvironmentVariable("LC_ALL", "C");
				}

				bIsCrossCompiling = true;

				return new LinuxToolChainInfo(BaseLinuxPath, MultiArchRoot, ClangPath, LlvmArPath, ObjcopyPath, Logger);
			}
		}

		protected virtual bool CrossCompiling()
		{
			return bIsCrossCompiling;
		}

		protected internal virtual string GetDumpEncodeDebugCommand(LinkEnvironment LinkEnvironment, FileItem OutputFile, Action action)
		{
			bool bUseCmdExe = BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64;
			string DumpCommand = bUseCmdExe ? "\"{0}\" \"{1}\" \"{2}\" 2>NUL" : "\"{0}\" -c -o \"{2}\" \"{1}\"";
			
			Func<string, string> DeleteCommand = fileName =>
				{
					if (bUseCmdExe)
					{
						return $"if exist \"{fileName}\" del /F \"{fileName}\"";
					}
					else
					{
						return $"rm \"{fileName}\"";
					}
				};

			FileItem EncodedBinarySymbolsFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.OutputDirectory!.FullName, OutputFile.Location.GetFileNameWithoutExtension() + ".sym"));
			FileItem SymbolsFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.IntermediateDirectory!.FullName, OutputFile.Location.GetFileName() + ".psym"));
			FileItem StrippedFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.IntermediateDirectory.FullName, OutputFile.Location.GetFileName() + "_nodebug"));
			FileItem DebugFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.OutputDirectory.FullName, OutputFile.Location.GetFileNameWithoutExtension() + ".debug"));

			bool preservePsym = Options.HasFlag(ClangToolChainOptions.PreservePSYM);
			if (preservePsym)
			{
				SymbolsFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.OutputDirectory.FullName, OutputFile.Location.GetFileNameWithoutExtension() + ".psym"));
			}

			StringWriter Out = new StringWriter();
			Out.NewLine = bUseCmdExe ? "\r\n" : "\n";

			if (!Options.HasFlag(ClangToolChainOptions.DisableDumpSyms) || Options.HasFlag(ClangToolChainOptions.PreservePSYM))
			{
				// dump_syms
				Out.WriteLine(DumpCommand,
					LinuxInfo.DumpSyms,
					OutputFile.AbsolutePath,
					SymbolsFile.AbsolutePath
				);

				// encode breakpad symbols
				Out.WriteLine("\"{0}\" \"{1}\" \"{2}\"",
					LinuxInfo.BreakpadEncoder,
					SymbolsFile.AbsolutePath,
					EncodedBinarySymbolsFile.AbsolutePath
				);

				if (preservePsym)
				{
					action.ProducedItems.Add(SymbolsFile);
				}
				else
				{
					Out.WriteLine(DeleteCommand(SymbolsFile.AbsolutePath));
				}
			}
			else
			{
				// we have to create dummy files to prevent packaging errors
				Out.WriteLine("echo DummySyms>> \"{0}\"", EncodedBinarySymbolsFile.AbsolutePath);
			}

			action.ProducedItems.Add(EncodedBinarySymbolsFile);

			if (LinkEnvironment.bCreateDebugInfo)
			{
				if (MaxBinarySizeOverrideForObjcopy > 0 && bUseCmdExe)
				{
					Out.WriteLine("for /F \"tokens=*\" %%F in (\"{0}\") DO set size=%%~zF",
						OutputFile.AbsolutePath
					);

					Out.WriteLine("if %size% LSS {0} (", MaxBinarySizeOverrideForObjcopy);
				}

				// objcopy stripped file
				Out.WriteLine("\"{0}\" --strip-all \"{1}\" \"{2}\"",
					LinuxInfo.Objcopy,
					OutputFile.AbsolutePath,
					StrippedFile.AbsolutePath
				);

				// objcopy debug file
				Out.WriteLine("\"{0}\" --only-keep-debug \"{1}\" \"{2}\"",
					LinuxInfo.Objcopy,
					OutputFile.AbsolutePath,
					DebugFile.AbsolutePath
				);

				// objcopy link debug file to final so
				Out.WriteLine("\"{0}\" --add-gnu-debuglink=\"{1}\" \"{2}\" \"{3}\"",
					LinuxInfo.Objcopy,
					DebugFile.AbsolutePath,
					StrippedFile.AbsolutePath,
					OutputFile.AbsolutePath
				);

				Out.WriteLine(DeleteCommand(StrippedFile.AbsolutePath));

				if (bUseCmdExe)
				{
					if (MaxBinarySizeOverrideForObjcopy > 0)
					{
						// If we have an override size, then we need to create a dummy file if that size is exceeded
						Out.WriteLine(") ELSE (");
						Out.WriteLine("echo DummyDebug >> \"{0}\"", DebugFile.AbsolutePath);
						Out.WriteLine(")");
					}
				}
				else
				{
					// Change the debug file to normal permissions. It was taking on the +x rights from the output file
					Out.WriteLine("chmod 644 \"{0}\"",
						DebugFile.AbsolutePath
					);
				}
			}
			else
			{
				// If we have disabled objcopy then we need to create a dummy debug file
				Out.WriteLine("echo DummyDebug >> \"{0}\"",
					DebugFile.AbsolutePath
				);
			}

			action.ProducedItems.Add(DebugFile);

			return Out.ToString();
		}

		private static bool ShouldUseLibcxx()
		{
			// set UE_LINUX_USE_LIBCXX to either 0 or 1. If unset, defaults to 1.
			string? UseLibcxxEnvVarOverride = Environment.GetEnvironmentVariable("UE_LINUX_USE_LIBCXX");
			if (String.IsNullOrEmpty(UseLibcxxEnvVarOverride) || UseLibcxxEnvVarOverride == "1")
			{
				return true;
			}
			return false;
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_WarningsAndErrors(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_WarningsAndErrors(CompileEnvironment, Arguments);

			//Arguments.Add("-Wunreachable-code");            // additional warning not normally included in Wall: warns if there is code that will never be executed - not helpful due to bIsGCC and similar
			Arguments.Add("-Wno-dangling");		// kinda scary, but matches what we currently do on MS platforms when clang is used

			Arguments.Add("-Wno-undefined-bool-conversion"); // hides checking if 'this' pointer is null
		}

		private void AddLTOFlags(List<string> Arguments, bool bForLinker)
		{
			if (Options.HasFlag(ClangToolChainOptions.EnableLinkTimeOptimization))
			{
				if (Options.HasFlag(ClangToolChainOptions.EnableThinLTO))
				{
					if (bForLinker)
					{
						Arguments.Add( "-flto=thin -Wl,--thinlto-jobs=all, -Wl,-mllvm,-disable-auto-upgrade-debug-info, -Wl,-mllvm,-enable-ext-tsp-block-placement=1");
					}
					else
					{
						Arguments.Add("-flto=thin");
					}
				}
				else
				{
					Arguments.Add("-flto");
				}
			}
		}

		private void AddCompilerLTOFlags(List<string> Arguments)
		{
			AddLTOFlags(Arguments, false);
		}

		private void AddLinkerLTOFlags(List<string> Arguments)
		{
			AddLTOFlags(Arguments, true);
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Optimizations(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Optimizations(CompileEnvironment, Arguments);

			AddCompilerLTOFlags(Arguments);

			if (CompileEnvironment.bCodeCoverage)
			{
				Arguments.Add("-O0");
				if (ShouldUseLibcxx())
				{
					Arguments.Add("--coverage"); // gcov
				}
				else
				{
					Arguments.Add("-fprofile-instr-generate -fcoverage-mapping"); // llvm-cov
				}
			}
			else if (!CompileEnvironment.bOptimizeCode) // optimization level
			{
				Arguments.Add("-O0");
			}
			else
			{
				// Don't over optimise if using Address/MemorySanitizer or you'll get false positive errors due to erroneous optimisation of necessary Address/MemorySanitizer instrumentation.
				if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) || Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer))
				{
					Arguments.Add("-O1 -g");

					// This enables __asan_default_options() in UnixCommonStartup.h which disables the leak detector
					Arguments.Add("-DDISABLE_ASAN_LEAK_DETECTOR=1");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
				{
					Arguments.Add("-O1 -g");
				}
				else
				{
					if (CompileEnvironment.OptimizationLevel == OptimizationMode.Size)
					{
						Arguments.Add("-Oz");
					}
					else if (CompileEnvironment.OptimizationLevel == OptimizationMode.SizeAndSpeed)
					{
						Arguments.Add("-Os");
					}
					else
					{
						if (CompileEnvironment.bPGOOptimize || CompileEnvironment.bPGOProfile)
						{
							// -Os tends to be both faster and smaller than -O3 when PGO is enabled
							Arguments.Add("-Os");
						}
						else
						{
							Arguments.Add("-O3");
						}
					}
				}
			}

			bool bRetainFramePointers = CompileEnvironment.bRetainFramePointers
				|| Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) || Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer) || Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer)
				|| CompileEnvironment.Configuration == CppConfiguration.Debug;


			if (CompileEnvironment.Configuration == CppConfiguration.Shipping)
			{

				if (!bRetainFramePointers)
				{
					Arguments.Add("-fomit-frame-pointer");
				}
			}
			// switches to help debugging
			else if (CompileEnvironment.Configuration == CppConfiguration.Debug)
			{
				Arguments.Add("-fno-inline");                   // disable inlining for better debuggability (e.g. callstacks, "skip file" in gdb)
				Arguments.Add("-fstack-protector");             // detect stack smashing
			}

			if (bRetainFramePointers)
			{
				Arguments.Add("-fno-optimize-sibling-calls");
				Arguments.Add("-fno-omit-frame-pointer");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Debugging(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Debugging(CompileEnvironment, Arguments);

			// debug info
			// bCreateDebugInfo is normally set for all configurations, including Shipping - this is needed to enable callstack in Shipping builds (proper resolution: UEPLAT-205, separate files with debug info)
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Arguments.Add("-gdwarf-4");

				if (bGdbIndexSection)
				{
					// Generate .debug_pubnames and .debug_pubtypes sections in a format suitable for conversion into a
					// GDB index. This option is only useful with a linker that can produce GDB index version 7.
					Arguments.Add("-ggnu-pubnames");
				}

				if (Options.HasFlag(ClangToolChainOptions.TuneDebugInfoForLLDB))
				{
					Arguments.Add("-glldb");
				}

				if (CompileEnvironment.bDebugLineTablesOnly)
				{
					Arguments.Add("-gline-tables-only");
				}
			}

			if (CompileEnvironment.bHideSymbolsByDefault)
			{
				Arguments.Add("-fvisibility-ms-compat");
				Arguments.Add("-fvisibility-inlines-hidden");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompilerArguments_Sanitizers(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompilerArguments_Sanitizers(CompileEnvironment, Arguments);

			// UBSan
			if (Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer))
			{
				Arguments.Add("-fno-sanitize=vptr");
			}

			// MSan
			if (Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer))
			{
				// Force using the ANSI allocator if MSan is enabled
				// -fsanitize-memory-track-origins adds a 1.5x-2.5x slow down ontop of MSan normal amount of overhead
				// -fsanitize-memory-track-origins=1 is faster but collects only allocation points but not intermediate stores
				Arguments.Add("-fsanitize-memory-track-origins");
			}

			// LibFuzzer
			if (Options.HasFlag(ClangToolChainOptions.EnableLibFuzzer))
			{
				Arguments.Add("-fsanitize=fuzzer");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			// build up the commandline common to C and C++

			// These aren't supported on Linux at this time
			Arguments.Add("-DUSE_DEBUG_LOGGING=0");
			Arguments.Add("-DUSE_EVENT_LOGGING=0");

			// always select the driver g++ in-case we are using a different binary for clang, such as clang/clang-cl
			Arguments.Add("--driver-mode=g++");

			if (Options.HasFlag(ClangToolChainOptions.CompressDebugFile))
			{
				Arguments.Add("-gz=zlib");
			}

			if (ShouldUseLibcxx())
			{
				Arguments.Add("-nostdinc++");
				if (PlatformSDK.ForceUseLegacyLibCxx())
				{
					Arguments.Add(GetSystemIncludePathArgument(DirectoryReference.Combine(Unreal.EngineSourceDirectory, "ThirdParty", "Unix", "LibCxx", "include"), CompileEnvironment));
					Arguments.Add(GetSystemIncludePathArgument(DirectoryReference.Combine(Unreal.EngineSourceDirectory, "ThirdParty", "Unix", "LibCxx", "include", "c++", "v1"), CompileEnvironment));
				}
				else
				{
					if (!PlatformSDK.ForceUseSystemCompiler())
					{
						// LinuxInfo.BaseLinuxPath should only be null in the case that we've been forced to use the system compiler.
						// In that case, project build.cs files should be adjusted to include the relevant directories to the system include paths
						if(LinuxInfo.BaseLinuxPath != null)
						{
							Arguments.Add(GetSystemIncludePathArgument(DirectoryReference.Combine(LinuxInfo.BaseLinuxPath, "include"), CompileEnvironment));
							Arguments.Add(GetSystemIncludePathArgument(DirectoryReference.Combine(LinuxInfo.BaseLinuxPath, "include", "c++", "v1"), CompileEnvironment));
						}
					}
				}
			}

			if (CompilerVersionGreaterOrEqual(12, 0, 0))
			{
				Arguments.Add("-fbinutils-version=2.36");
			}

			if (CompileEnvironment.Architecture == UnrealArch.Arm64)
			{
				Arguments.Add("-funwind-tables");               // generate unwind tables as they are needed for backtrace (on x86(64) they are generated implicitly)
			}

			Arguments.Add("-fno-math-errno");               // do not assume that math ops have side effects

			Arguments.Add(GetRTTIFlag(CompileEnvironment)); // flag for run-time type info

			if (CompileEnvironment.Architecture == UnrealArch.X64)
			{
				Arguments.Add("-mssse3"); // enable ssse3 by default for x86. This is default on for MSVC so lets reflect that here
			}

			//Arguments.Add("-DOPERATOR_NEW_INLINE=FORCENOINLINE");

			if (CompileEnvironment.bIsBuildingDLL)
			{
				Arguments.Add("-fPIC");
				// Use local-dynamic TLS model. This generates less efficient runtime code for __thread variables, but avoids problems of running into
				// glibc/ld.so limit (DTV_SURPLUS) for number of dlopen()'ed DSOs with static TLS (see e.g. https://www.cygwin.com/ml/libc-help/2013-11/msg00033.html)
				Arguments.Add("-ftls-model=local-dynamic");
			}
			else
			{
				Arguments.Add("-ffunction-sections");
				Arguments.Add("-fdata-sections");
			}

			// only suppress if happen to be using a system compiler and we have not explicitly requested pie
			if (!CompileEnvironment.bIsBuildingDLL)
			{
				if (CompileEnvironment.bUsePIE)
				{
					Arguments.Add("-fPIE");
				}
				else
				{
					Arguments.Add("-fno-PIE");
				}
			}

			if (CompileEnvironment.bUseStackProtection)
			{
				Arguments.Add("-fstack-protector");
			}

			if (PlatformSDK.bVerboseCompiler)
			{
				Arguments.Add("-v");                            // for better error diagnosis
			}

			if (CrossCompiling())
			{
				Arguments.Add($"-target {CompileEnvironment.Architecture.LinuxName}");        // Set target triple
				Arguments.Add($"--sysroot=\"{NormalizeCommandLinePath(LinuxInfo.BaseLinuxPath!, CompileEnvironment.RootPaths)}\"");
			}

			base.GetCompileArguments_Global(CompileEnvironment, Arguments);
		}

		/// <inheritdoc/>
		protected override string EscapePreprocessorDefinition(string Definition)
		{
			string[] SplitData = Definition.Split('=');
			string? Key = SplitData.ElementAtOrDefault(0);
			string? Value = SplitData.ElementAtOrDefault(1);

			if (String.IsNullOrEmpty(Key))
			{
				return "";
			}
			if (!String.IsNullOrEmpty(Value))
			{
				if (!Value.StartsWith("\"") && (Value.Contains(' ') || Value.Contains('$')))
				{
					Value = Value.Trim('\"');       // trim any leading or trailing quotes
					Value = "\"" + Value + "\"";    // ensure wrap string with double quotes
				}

				// replace double quotes to escaped double quotes if exists
				Value = Value.Replace("\"", "\\\"");
			}

			return Value == null
				? String.Format("{0}", Key)
				: String.Format("{0}={1}", Key, Value);
		}

		public override void PrepareRuntimeDependencies(List<RuntimeDependency> RuntimeDependencies, Dictionary<FileReference, FileReference> TargetFileToSourceFile, DirectoryReference ExeDir)
		{
			// If ASan is enabled we need to copy the companion helper libraries from the MSVC tools bin folder to the
			// target executable folder.
			if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableLibFuzzer))
			{
				bool bInternalBuild = false;
				BuildVersion? Version;
				if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
				{
					bInternalBuild = !Version.IsLicenseeVersion;
				}

				if (bInternalBuild)
				{
					string? InternalSdkPath = UEBuildPlatform.GetSDK(UnrealTargetPlatform.Linux)!.GetInternalSDKPath();
					if (InternalSdkPath != null)
					{
						DirectoryReference InternalSdkPathRef = new DirectoryReference(InternalSdkPath);

						FileReference SymbolizerSourcePath = FileReference.Combine(InternalSdkPathRef, "bin/llvm-symbolizer");
						FileReference SymbolizerTargetPath = FileReference.Combine(ExeDir, "llvm-symbolizer");

						RuntimeDependencies.Add(new RuntimeDependency(SymbolizerTargetPath, StagedFileType.NonUFS));
						TargetFileToSourceFile[SymbolizerTargetPath] = SymbolizerSourcePath;
					}
				}
			}
		}

		protected virtual void GetLinkArguments(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			// always select the driver g++ in-case we are using a different binary for clang, such as clang/clang-cl
			Arguments.Add("--driver-mode=g++");
			Arguments.Add((BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64) ? "-fuse-ld=lld.exe" : "-fuse-ld=lld");

			if (Options.HasFlag(ClangToolChainOptions.CompressDebugFile))
			{
				Arguments.Add("-Wl,--compress-debug-sections=zlib");
			}

			// debugging symbols
			// Applying to all configurations @FIXME: temporary hack for FN to enable callstack in Shipping builds (proper resolution: UEPLAT-205)
			Arguments.Add("-rdynamic");   // needed for backtrace_symbols()...

			if (LinkEnvironment.bIsBuildingDLL)
			{
				Arguments.Add("-shared");
			}
			else
			{
				// ignore unresolved symbols in shared libs
				Arguments.Add("-Wl,--unresolved-symbols=ignore-in-shared-libs");
			}

			if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableLibFuzzer))
			{
				Arguments.Add("-g");

				if (Options.HasFlag(ClangToolChainOptions.EnableSharedSanitizer))
				{
					Arguments.Add("-shared-libsan");

					// LLVM 15 compiler-rt introduced a new bug causing ASan to crash when going over net code
					// https://github.com/llvm/llvm-project/issues/59007
					// adding this fixes it, may be required now?
					Arguments.Add("-lresolv");
				}

				if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer))
				{
					Arguments.Add("-fsanitize=address");
					Arguments.Add("-fsanitize-recover=address"); 
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
				{
					Arguments.Add("-fsanitize=thread");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer))
				{
					Arguments.Add("-fsanitize=undefined");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer))
				{
					// -fsanitize-memory-track-origins adds a 1.5x-2.5x slow ontop of MSan normal amount of overhead
					// -fsanitize-memory-track-origins=1 is faster but collects only allocation points but not intermediate stores
					Arguments.Add("-fsanitize=memory -fsanitize-memory-track-origins");
				}

				if (Options.HasFlag(ClangToolChainOptions.EnableLibFuzzer))
				{
					Arguments.Add("-fsanitize=fuzzer");
				}

				if (CrossCompiling())
				{
					string ClangVersionFolder = (Info.ClangVersion.Major < 16) ? String.Format("{0}.{1}.{2}", Info.ClangVersion.Major, Info.ClangVersion.Minor, Info.ClangVersion.Build) : String.Format("{0}", Info.ClangVersion.Major);

					// x64 only replaced the linux folder with arch, while on arm64 its still linux
					if (LinkEnvironment.Architecture == UnrealArch.Arm64)
					{
						Arguments.Add(String.Format("-Wl,-rpath=\"{0}/lib/clang/{1}/lib/linux\"",
								LinuxInfo.BaseLinuxPath, ClangVersionFolder));
					}
					else
					{
						Arguments.Add(String.Format("-Wl,-rpath=\"{0}/lib/clang/{1}/lib/x86_64-unknown-linux-gnu\"",
								LinuxInfo.BaseLinuxPath, ClangVersionFolder));
					}
				}
			}

			if (LinkEnvironment.bCreateDebugInfo && bGdbIndexSection)
			{
				// Generate .gdb_index section. On my machine, this cuts symbol loading time (breaking at main) from 45
				// seconds to 17 seconds (with gdb v8.3.1).
				Arguments.Add("-Wl,--gdb-index");
			}

			if (LinkEnvironment.bCodeCoverage)
			{
				// Unreal Separates the linking phase and the compilation phase.
				// We pass to clang the flag `--coverage` during the compile time
				// And we link the correct compiler-rt library (shipped by UE, and part of the LLVM toolchain)
				// to every binary produced.
				if (ShouldUseLibcxx())
				{
					Arguments.Add("--coverage"); // gcov
				}
				else
				{
					Arguments.Add("-fprofile-instr-generate"); // llvm-cov
				}
			}
			// RPATH for third party libs
			Arguments.Add("-Wl,-rpath=${ORIGIN}");
			Arguments.Add("-Wl,-rpath-link=${ORIGIN}");
			Arguments.Add("-Wl,-rpath=${ORIGIN}/..");   // for modules that are in sub-folders of the main Engine/Binary/Linux folder
			if (LinkEnvironment.Architecture == UnrealArch.X64)
			{
				Arguments.Add("-Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/Qualcomm/Linux");
			}
			else
			{
				// x86_64 is now using updated ICU that doesn't need extra .so
				Arguments.Add("-Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/ICU/icu4c-53_1/Unix/" + LinkEnvironment.Architecture.LinuxName);
			}

			// @FIXME: Workaround for generating RPATHs for launching on devices UE-54136
			Arguments.Add("-Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/PhysX3/Unix/x86_64-unknown-linux-gnu");

			// Some OS ship ld with new ELF dynamic tags, which use DT_RUNPATH vs DT_RPATH. Since DT_RUNPATH do not propagate to dlopen()ed DSOs,
			// this breaks the editor on such systems. See https://kenai.com/projects/maxine/lists/users/archive/2011-01/message/12 for details
			Arguments.Add("-Wl,--disable-new-dtags");

			// This severely improves runtime linker performance. Without using FixDeps the impact on link time is not as big.
			Arguments.Add("-Wl,--as-needed");

			// Additionally speeds up editor startup by 1-2s
			Arguments.Add("-Wl,--hash-style=gnu");

			// This apparently can help LLDB speed up symbol lookups
			Arguments.Add("-Wl,--build-id");
			if (!LinkEnvironment.bIsBuildingDLL)
			{
				Arguments.Add("-Wl,--gc-sections");

				if (LinkEnvironment.bUsePIE)
				{
					Arguments.Add("-pie");
				}
				else
				{
					Arguments.Add("-Wl,-no-pie");
				}
			}

			// Profile Guided Optimization (PGO) and Link Time Optimization (LTO)
			// Whether we actually can enable that is checked in CanUseAdvancedLinkerFeatures() earlier
			if (LinkEnvironment.bPGOOptimize)
			{
				//
				// Clang emits a warning for each compiled function that doesn't have a matching entry in the profile data.
				// This can happen when the profile data is older than the binaries we're compiling.
				//
				// Disable this warning. It's far too verbose.
				//
				Arguments.Add("-Wno-backend-plugin");

				DirectoryReference? PGODir = DirectoryReference.FromString(LinkEnvironment.PGODirectory!);
				Arguments.Add($"-fprofile-instr-use=\"{NormalizeCommandLinePath(DirectoryReference.Combine(PGODir!, LinkEnvironment.PGOFilenamePrefix!))}\"");
			}
			else if (LinkEnvironment.bPGOProfile)
			{
				Log.TraceInformationOnce("Enabling Profile Guided Instrumentation (PGI). Linking will take a while.");
				Arguments.Add("-fprofile-generate");
			}

			// whether we actually can do that is checked in CanUseAdvancedLinkerFeatures() earlier
			AddLinkerLTOFlags(Arguments);

			if (CrossCompiling())
			{
				Arguments.Add($"-target {LinkEnvironment.Architecture.LinuxName}");        // Set target triple
				DirectoryReference SysRootPath = LinuxInfo.BaseLinuxPath!;
				Arguments.Add($"--sysroot=\"{NormalizeCommandLinePath(SysRootPath)}\"");

				// Linking with the toolchain on linux appears to not search usr/
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
				{
					Arguments.Add($"-B\"{NormalizeCommandLinePath(DirectoryReference.Combine(SysRootPath, "usr", "lib"))}\"");
					Arguments.Add($"-B\"{NormalizeCommandLinePath(DirectoryReference.Combine(SysRootPath, "usr", "lib64"))}\"");
					Arguments.Add($"-L\"{NormalizeCommandLinePath(DirectoryReference.Combine(SysRootPath, "usr", "lib"))}\"");
					Arguments.Add($"-L\"{NormalizeCommandLinePath(DirectoryReference.Combine(SysRootPath, "usr", "lib64"))}\"");
				}
			}

			if (LinkEnvironment.Configuration == CppConfiguration.Shipping)
			{
				Arguments.Add("-Wl,--icf=all"); // Enables ICF (Identical Code Folding). [all, safe] safe == fold functions that can be proven not to have their address taken.
				Arguments.Add("-Wl,-O3");
			}
		}

		string GetArchiveArguments(LinkEnvironment LinkEnvironment)
		{
			return " rcs";
		}

		// cache the location of NDK tools
		protected bool bIsCrossCompiling;

		/// <summary>
		/// Contains converter from output file to temporary output file (the one that will be relinked)
		/// </summary>
		private Dictionary<FileItem, FileItem> LinkToPrelinkFileList = new();


		/// <summary>
		/// Tracks that information about used C++ library is only printed once
		/// </summary>
		private bool bHasPrintedBuildDetails = false;
		protected void PrintBuildDetails(CppCompileEnvironment CompileEnvironment, ILogger Logger)
		{
			Logger.LogInformation("------- Build details --------");
			Logger.LogInformation("Using {ToolchainInfo}.", LinuxInfo.BaseLinuxPath == null ? "system toolchain" : $"toolchain located at '{LinuxInfo.BaseLinuxPath}'");
			Logger.LogInformation("Using clang ({ClangPath}) version '{ClangVersionString}' (string), {ClangVersionMajor} (major), {ClangVersionMinor} (minor), {ClangVersionPatch} (patch)",
				Info.Clang, Info.ClangVersionString, Info.ClangVersion.Major, Info.ClangVersion.Minor, Info.ClangVersion.Build);

			// inform the user which C++ library the engine is going to be compiled against - important for compatibility with third party code that uses STL
			Logger.LogInformation("Using {Lib} standard C++ library.", ShouldUseLibcxx() ? "bundled libc++" : "compiler default (most likely libstdc++)");
			Logger.LogInformation("Using lld linker");
			Logger.LogInformation("Using llvm-ar ({LlvmAr}) version '{LlvmArVersionString} (string)'", Info.Archiver, Info.ArchiverVersionString);

			if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableLibFuzzer))
			{
				string SanitizerInfo = "Building with:";
				string StaticOrShared = Options.HasFlag(ClangToolChainOptions.EnableSharedSanitizer) ? " dynamically" : " statically";

				SanitizerInfo += Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) ? StaticOrShared + " linked AddressSanitizer" : "";
				SanitizerInfo += Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer) ? StaticOrShared + " linked ThreadSanitizer" : "";
				SanitizerInfo += Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer) ? StaticOrShared + " linked UndefinedBehaviorSanitizer" : "";
				SanitizerInfo += Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer) ? StaticOrShared + " linked MemorySanitizer" : "";
				SanitizerInfo += Options.HasFlag(ClangToolChainOptions.EnableLibFuzzer) ? StaticOrShared + " linked LibFuzzer" : "";

				Logger.LogInformation("{SanitizerInfo}", SanitizerInfo);
			}

			if (Options.HasFlag(ClangToolChainOptions.CompressDebugFile))
			{
				Logger.LogInformation("Compressing debug files");
			}

			Logger.LogInformation("Targeted minimum CPU architecture: {0}", (CompileEnvironment.Architecture == UnrealArch.X64) ? CompileEnvironment.MinCpuArchX64 : CompileEnvironment.MinArm64CpuTarget);

			if (CompileEnvironment.bPGOOptimize)
			{
				Logger.LogInformation("Using PGO (profile guided optimization).");
				Logger.LogInformation("  Directory for PGO data files='{CompileEnvironmentPGODirectory}'", CompileEnvironment.PGODirectory);
				Logger.LogInformation("  Prefix for PGO data files='{CompileEnvironmentPGOFilenamePrefix}'", CompileEnvironment.PGOFilenamePrefix);
			}

			if (CompileEnvironment.bCodeCoverage)
			{
				Logger.LogInformation("Using --coverage build flag");
			}

			if (CompileEnvironment.bPGOProfile)
			{
				Logger.LogInformation("Using PGI (profile guided instrumentation).");
			}

			if (Options.HasFlag(ClangToolChainOptions.EnableLinkTimeOptimization))
			{
				if (Options.HasFlag(ClangToolChainOptions.EnableThinLTO))
				{
					Logger.LogInformation("Using ThinLTO (link-time optimization).");
				}
				else
				{
					Logger.LogInformation("Using LTO (link-time optimization).");
				}
			}

			if (CompileEnvironment.bUsePIE)
			{
				Logger.LogInformation("Using position independent executables (PIE)");
			}

			if (CompileEnvironment.bUseStackProtection)
			{
				Logger.LogInformation("Using stack protection");
			}

			Logger.LogInformation("------------------------------");
		}

		protected override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, IEnumerable<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph)
		{
			if (ShouldSkipCompile(CompileEnvironment))
			{
				return new CPPOutput();
			}

			List<string> GlobalArguments = new();
			GetCompileArguments_Global(CompileEnvironment, GlobalArguments);

			//var BuildPlatform = UEBuildPlatform.GetBuildPlatform(CompileEnvironment.Platform);

			if (!bHasPrintedBuildDetails)
			{
				PrintBuildDetails(CompileEnvironment, Logger);

				bHasPrintedBuildDetails = true;
			}

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in InputFiles)
			{
				CompileCPPFile(CompileEnvironment, SourceFile, OutputDir, ModuleName, Graph, GlobalArguments, Result);
			}

			return Result;
		}

		public FileItem GetPrelinkOutput(DirectoryReference intermediateDir, FileItem output)
		{
			return FileItem.GetItemByFileReference(FileReference.Combine(intermediateDir, "Prelink", output.Name));
		}

		/// <summary>
		/// Creates an action to archive all the .o files into single .a file
		/// </summary>
		public FileItem CreateArchiveAndIndex(LinkEnvironment LinkEnvironment, IActionGraphBuilder Graph, ILogger Logger)
		{
			// Create an archive action
			Action ArchiveAction = Graph.CreateAction(ActionType.Link);
			ArchiveAction.WorkingDirectory = Unreal.EngineSourceDirectory;
			ArchiveAction.CommandPath = Info.Archiver;

			ArchiveAction.bCanExecuteInUBA = OperatingSystem.IsWindows(); // Linker on native linux uses vfork/exec which is not handled in uba right now

			// this will produce a final library
			ArchiveAction.bProducesImportLibrary = true;

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			ArchiveAction.ProducedItems.Add(OutputFile);
			ArchiveAction.CommandDescription = "Archive";
			ArchiveAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);
			ArchiveAction.CommandArguments += String.Format("{0} \"{1}\"", GetArchiveArguments(LinkEnvironment), OutputFile.AbsolutePath);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				string InputAbsolutePath = InputFile.AbsolutePath.Replace("\\", "/");
				InputFileNames.Add(String.Format("\"{0}\"", InputAbsolutePath));
				ArchiveAction.PrerequisiteItems.Add(InputFile);
			}

			// this won't stomp linker's response (which is not used when compiling static libraries)
			FileReference ResponsePath = GetResponseFileName(LinkEnvironment, OutputFile);
			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				FileItem ResponseFileItem = Graph.CreateIntermediateTextFile(ResponsePath, InputFileNames);
				ArchiveAction.PrerequisiteItems.Add(ResponseFileItem);
			}
			ArchiveAction.CommandArguments += String.Format(" @\"{0}\"", ResponsePath.FullName);

			// Add the additional arguments specified by the environment.
			ArchiveAction.CommandArguments += LinkEnvironment.AdditionalArguments;
			ArchiveAction.CommandArguments = ArchiveAction.CommandArguments.Replace("\\", "/");

			if (BuildHostPlatform.Current.ShellType == ShellType.Sh)
			{
				ArchiveAction.CommandArguments += "'";
			}
			else
			{
				ArchiveAction.CommandArguments += "\"";
			}

			// Only execute linking on the local PC.
			ArchiveAction.bCanExecuteRemotely = false;

			return OutputFile;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph)
		{
			Debug.Assert(!bBuildImportLibraryOnly);

			List<string> RPaths = new List<string>();

			if (LinkEnvironment.bIsBuildingLibrary || bBuildImportLibraryOnly)
			{
				return CreateArchiveAndIndex(LinkEnvironment, Graph, Logger);
			}

			// Create an action that invokes the linker.
			Action LinkAction = Graph.CreateAction(ActionType.Link);
			LinkAction.RootPaths = LinkEnvironment.RootPaths;
			LinkAction.WorkingDirectory = Unreal.EngineSourceDirectory;

			string LinkCommandString;
			LinkCommandString = "\"" + Info.Clang + "\"";

			// Get link arguments.
			List<string> LinkArguments = new List<string>();
			GetLinkArguments(LinkEnvironment, LinkArguments);
			LinkCommandString += " " + String.Join(' ', LinkArguments);

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = LinkEnvironment.bIsBuildingDLL;

			// Add profdata as a prerequisite for pgo optimize
			if (LinkEnvironment.bPGOOptimize && LinkEnvironment.PGODirectory != null && LinkEnvironment.PGOFilenamePrefix != null)
			{
				DirectoryReference PGODir = new DirectoryReference(LinkEnvironment.PGODirectory);
				LinkAction.PrerequisiteItems.Add(FileItem.GetItemByFileReference(FileReference.Combine(PGODir, LinkEnvironment.PGOFilenamePrefix)));
			}

			bool needsRelink = LinkEnvironment.bIsBuildingDLL && LinkEnvironment.bIsCrossReferenced;

			// If there will be a relink we let the link step write to a file under intermediate folder
			FileItem RealOutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			FileItem OutputFile = RealOutputFile;
			if (needsRelink)
			{
				OutputFile = GetPrelinkOutput(LinkEnvironment.IntermediateDirectory!, OutputFile);
			}

			// Add the output file as a production of the link action.
			LinkAction.ProducedItems.Add(OutputFile);

			// LTO/PGO can take a lot of time, make it clear for the user
			if (LinkEnvironment.bPGOProfile)
			{
				LinkAction.CommandDescription = "Link-PGI";
			}
			else if (LinkEnvironment.bPGOOptimize)
			{
				LinkAction.CommandDescription = "Link-PGO";
			}
			else if (LinkEnvironment.bAllowLTCG)
			{
				LinkAction.CommandDescription = "Link-LTO";
			}
			else
			{
				LinkAction.CommandDescription = "Link";
			}

			if (Options.HasFlag(ClangToolChainOptions.EnableLinkTimeOptimization) && Options.HasFlag(ClangToolChainOptions.EnableThinLTO))
			{
				// Set the weight to number of logical cores as lld can max out the available cores
				LinkAction.Weight = Utils.GetLogicalProcessorCount();

				// Disallow remote to prevent this long running action from running on an agent if remote linking is enabled
				LinkAction.bCanExecuteRemotely = false;
			}

			// because the logic choosing between lld and ld is somewhat messy atm (lld fails to link .DSO due to bugs), make the name of the linker clear
			LinkAction.CommandDescription += (LinkCommandString.Contains("-fuse-ld=lld")) ? " (lld)" : " (ld)";
			LinkAction.CommandVersion = Info.ClangVersionString;
			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);
			LinkAction.CacheBucket = GetCacheBucket(TargetRules, null);
			LinkAction.ArtifactMode = ArtifactMode.Enabled;

			// Add the output file to the command-line.
			LinkCommandString += String.Format(" -o \"{0}\"", OutputFile.AbsolutePath);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> ResponseLines = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				if (InputFile.HasExtension(".ldscript"))
				{
					ResponseLines.Add($"--version-script=\"{NormalizeCommandLinePath(InputFile)}\"");
				}
				else
				{
					ResponseLines.Add($"\"{NormalizeCommandLinePath(InputFile)}\"");
				}
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			if (LinkEnvironment.bIsBuildingDLL)
			{
				ResponseLines.Add(String.Format(" -soname=\"{0}\"", OutputFile.Location.GetFileName()));
			}

			// Start with the configured LibraryPaths and also add paths to any libraries that
			// we depend on (libraries that we've build ourselves).
			List<DirectoryReference> AllLibraryPaths = LinkEnvironment.SystemLibraryPaths;

			IEnumerable<string> AdditionalLibraries = Enumerable.Concat(LinkEnvironment.SystemLibraries, LinkEnvironment.Libraries.Select(x => x.FullName));
			foreach (string AdditionalLibrary in AdditionalLibraries)
			{
				string PathToLib = Path.GetDirectoryName(AdditionalLibrary)!;
				if (!String.IsNullOrEmpty(PathToLib))
				{
					// make path absolute, because FixDependencies script may be executed in a different directory
					DirectoryReference AbsolutePathToLib = new DirectoryReference(PathToLib);
					if (!AllLibraryPaths.Contains(AbsolutePathToLib))
					{
						AllLibraryPaths.Add(AbsolutePathToLib);
					}
				}

				if ((AdditionalLibrary.Contains("Plugins") || AdditionalLibrary.Contains("Binaries/ThirdParty") || AdditionalLibrary.Contains("Binaries\\ThirdParty")) && Path.GetDirectoryName(AdditionalLibrary) != Path.GetDirectoryName(RealOutputFile.AbsolutePath))
				{
					string RelativePath = new FileReference(AdditionalLibrary).Directory.MakeRelativeTo(RealOutputFile.Location.Directory);

					if (LinkEnvironment.bIsBuildingDLL)
					{
						// Remove the root Unreal.RootDirectory from the RuntimeLibaryPath
						string AdditionalLibraryRootPath = new FileReference(AdditionalLibrary).Directory.MakeRelativeTo(Unreal.RootDirectory);

						// Figure out how many dirs we need to go back
						string RelativeRootPath = Unreal.RootDirectory.MakeRelativeTo(RealOutputFile.Location.Directory);

						// Combine the two together ie. number of ../ + the path after the root
						RelativePath = Path.Combine(RelativeRootPath, AdditionalLibraryRootPath);
					}

					// On Windows, MakeRelativeTo can silently fail if the engine and the project are located on different drives
					if (CrossCompiling() && RelativePath.StartsWith(Unreal.RootDirectory.FullName))
					{
						// do not replace directly, but take care to avoid potential double slashes or missed slashes
						string PathFromRootDir = RelativePath.Replace(Unreal.RootDirectory.FullName, "");
						// Path.Combine doesn't combine these properly
						RelativePath = ((PathFromRootDir.StartsWith("\\") || PathFromRootDir.StartsWith("/")) ? "..\\..\\.." : "..\\..\\..\\") + PathFromRootDir;
					}

					if (!RPaths.Contains(RelativePath))
					{
						RPaths.Add(RelativePath);
						ResponseLines.Add(String.Format(" -rpath=\"${{ORIGIN}}/{0}\"", RelativePath.Replace('\\', '/')));
					}
				}
			}

			foreach (string RuntimeLibaryPath in LinkEnvironment.RuntimeLibraryPaths)
			{
				string RelativePath = RuntimeLibaryPath;

				if (!RelativePath.StartsWith("$"))
				{
					if (LinkEnvironment.bIsBuildingDLL)
					{
						// Remove the root Unreal.RootDirectory from the RuntimeLibaryPath
						string RuntimeLibraryRootPath = new DirectoryReference(RuntimeLibaryPath).MakeRelativeTo(Unreal.RootDirectory);

						// Figure out how many dirs we need to go back
						string RelativeRootPath = Unreal.RootDirectory.MakeRelativeTo(RealOutputFile.Location.Directory);

						// Combine the two together ie. number of ../ + the path after the root
						RelativePath = Path.Combine(RelativeRootPath, RuntimeLibraryRootPath);
					}
					else
					{
						string RelativeRootPath = new DirectoryReference(RuntimeLibaryPath).MakeRelativeTo(Unreal.RootDirectory);

						// We're assuming that the binary will be placed according to our ProjectName/Binaries/Platform scheme
						RelativePath = Path.Combine("..", "..", "..", RelativeRootPath);
					}
				}

				// On Windows, MakeRelativeTo can silently fail if the engine and the project are located on different drives
				if (CrossCompiling() && RelativePath.StartsWith(Unreal.RootDirectory.FullName))
				{
					// do not replace directly, but take care to avoid potential double slashes or missed slashes
					string PathFromRootDir = RelativePath.Replace(Unreal.RootDirectory.FullName, "");
					// Path.Combine doesn't combine these properly
					RelativePath = ((PathFromRootDir.StartsWith("\\") || PathFromRootDir.StartsWith("/")) ? "..\\..\\.." : "..\\..\\..\\") + PathFromRootDir;
				}

				if (!RPaths.Contains(RelativePath))
				{
					RPaths.Add(RelativePath);
					ResponseLines.Add(String.Format(" -rpath=\"${{ORIGIN}}/{0}\"", RelativePath.Replace('\\', '/')));
				}
			}

			ResponseLines.Add(String.Format(" -rpath-link=\"{0}\"", Path.GetDirectoryName(RealOutputFile.AbsolutePath)));

			// Add the library paths to the argument list.
			foreach (DirectoryReference LibraryPath in AllLibraryPaths)
			{
				// use absolute paths because of FixDependencies script again
				ResponseLines.Add(String.Format(" -L\"{0}\"", LibraryPath.FullName.Replace('\\', '/')));
			}

			List<string> EngineAndGameLibrariesLinkFlags = new List<string>();
			List<FileItem> EngineAndGameLibrariesFiles = new List<FileItem>();

			// Pre-2.25 ld has symbol resolution problems when .so are mixed with .a in a single --start-group/--end-group
			// when linking with --as-needed.
			// Move external libraries to a separate --start-group/--end-group to fix it (and also make groups smaller and faster to link).
			// See https://github.com/EpicGames/UnrealEngine/pull/2778 and https://github.com/EpicGames/UnrealEngine/pull/2793 for discussion
			string ExternalLibraries = "";

			// add libraries in a library group
			ResponseLines.Add(String.Format(" --start-group"));

			foreach (string AdditionalLibrary in AdditionalLibraries)
			{
				if (String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)))
				{
					// library was passed just like "jemalloc", turn it into -ljemalloc
					ExternalLibraries += String.Format(" -l{0}", AdditionalLibrary);
				}
				else if (Path.GetExtension(AdditionalLibrary) == ".a")
				{
					// static library passed in, pass it along but make path absolute, because FixDependencies script may be executed in a different directory
					string AbsoluteAdditionalLibrary = Path.GetFullPath(AdditionalLibrary);
					if (AbsoluteAdditionalLibrary.Contains(' '))
					{
						AbsoluteAdditionalLibrary = String.Format("\"{0}\"", AbsoluteAdditionalLibrary);
					}
					AbsoluteAdditionalLibrary = AbsoluteAdditionalLibrary.Replace('\\', '/');

					// libcrypto/libssl contain number of functions that are being used in different DSOs. FIXME: generalize?
					if (LinkEnvironment.bIsBuildingDLL && (AbsoluteAdditionalLibrary.Contains("libcrypto") || AbsoluteAdditionalLibrary.Contains("libssl")))
					{
						ResponseLines.Add(" --whole-archive " + AbsoluteAdditionalLibrary + " --no-whole-archive");
					}
					else
					{
						ResponseLines.Add(" " + AbsoluteAdditionalLibrary);
					}

					LinkAction.PrerequisiteItems.Add(FileItem.GetItemByPath(AdditionalLibrary));
				}
				else
				{
					// Skip over full-pathed library dependencies when building DLLs to avoid circular
					// dependencies.
					FileItem LibraryDependency = FileItem.GetItemByPath(AdditionalLibrary);

					if (LinkToPrelinkFileList.TryGetValue(LibraryDependency, out FileItem? ToRelinkItem))
					{
						LibraryDependency = ToRelinkItem;
						ExternalLibraries += $" -L\"{ToRelinkItem.Directory.FullName.Replace("\\", "/")}\"";
					}

					string LibName = Path.GetFileNameWithoutExtension(AdditionalLibrary);
					if (LibName.StartsWith("lib"))
					{
						// Remove lib prefix
						LibName = LibName.Remove(0, 3);
					}
					else if (LibraryDependency.Exists)
					{
						// The library exists, but it is not prefixed with "lib", so force the
						// linker to find it without that prefix by prepending a colon to
						// the file name.
						LibName = String.Format(":{0}", LibraryDependency.Name);
					}
					string LibLinkFlag = String.Format(" -l{0}", LibName);

					if (needsRelink)
					{
						// We are building a cross referenced DLL so we can't actually include
						// dependencies at this point. Instead we add it to the list of
						// libraries to be used in the FixDependencies step.
						EngineAndGameLibrariesLinkFlags.Add(LibLinkFlag);
						EngineAndGameLibrariesFiles.Add(LibraryDependency);
						// it is important to add this exactly to the same place where the missing libraries would have been, it will be replaced later
						if (!ExternalLibraries.Contains("--allow-shlib-undefined"))
						{
							ExternalLibraries += String.Format(" -Wl,--allow-shlib-undefined");
						}
					}
					else
					{
						LinkAction.PrerequisiteItems.Add(LibraryDependency);
						ExternalLibraries += LibLinkFlag;
					}
				}
			}
			ResponseLines.Add(" --end-group");

			FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);
			FileItem ResponseFileItem = Graph.CreateIntermediateTextFile(ResponseFileName, ResponseLines);

			LinkCommandString += String.Format(" -Wl,@\"{0}\"", ResponseFileName);
			LinkAction.PrerequisiteItems.Add(ResponseFileItem);

			LinkCommandString += " -Wl,--start-group";
			LinkCommandString += ExternalLibraries;

			// make unresolved symbols an error, unless a) building a cross-referenced DSO  b) we opted out
			if ((!LinkEnvironment.bIsBuildingDLL || !LinkEnvironment.bIsCrossReferenced) && !LinkEnvironment.bIgnoreUnresolvedSymbols)
			{
				// This will make the linker report undefined symbols the current module, but ignore in the dependent DSOs.
				// It is tempting, but may not be possible to change that report-all - due to circular dependencies between our libs.
				LinkCommandString += " -Wl,--unresolved-symbols=ignore-in-shared-libs";
			}
			LinkCommandString += " -Wl,--end-group";

			LinkCommandString += " -lrt"; // needed for clock_gettime()
			LinkCommandString += " -lm"; // math

			if (ShouldUseLibcxx())
			{
				// libc++ and its abi lib
				LinkCommandString += " -nodefaultlibs";
				string LibCxxLibDir = LinuxInfo.BaseLinuxPath + "/lib64/";
				if(PlatformSDK.ForceUseLegacyLibCxx())
				{
					LibCxxLibDir = "ThirdParty/Unix/LibCxx/lib/Unix/" + LinkEnvironment.Architecture.LinuxName + "/";
				}
				LinkCommandString += " " +  LibCxxLibDir + "libc++.a";
				LinkCommandString += " " +  LibCxxLibDir + "libc++abi.a";
				LinkCommandString += " -lm";
				LinkCommandString += " -lc";
				LinkCommandString += " -lpthread"; // pthread_mutex_trylock is missing from libc stubs
				LinkCommandString += " -lgcc_s";
				LinkCommandString += " -lgcc";
			}

			// these can be helpful for understanding the order of libraries or library search directories
			if (PlatformSDK.bVerboseLinker)
			{
				LinkCommandString += " -Wl,--verbose";
				LinkCommandString += " -Wl,--trace";
				LinkCommandString += " -v";
			}

			// Add the additional arguments specified by the environment.
			LinkCommandString += LinkEnvironment.AdditionalArguments;
			LinkCommandString = LinkCommandString.Replace("\\\\", "/");
			LinkCommandString = LinkCommandString.Replace("\\", "/");

			bool bUseCmdExe = BuildHostPlatform.Current.ShellType == ShellType.Cmd;
			FileReference ShellBinary = BuildHostPlatform.Current.Shell;
			string ExecuteSwitch = bUseCmdExe ? " /C" : ""; // avoid -c so scripts don't need +x

			// Linux has issues with scripts and parameter expansion from curely brakets
			if (!bUseCmdExe)
			{
				LinkCommandString = LinkCommandString.Replace("{", "'{");
				LinkCommandString = LinkCommandString.Replace("}", "}'");
				LinkCommandString = LinkCommandString.Replace("$'{", "'${");    // fixing $'{ORIGIN}' to be '${ORIGIN}'
			}

			string LinkScriptName = String.Format((bUseCmdExe ? "Link-{0}.bat" : "Link-{0}.sh"), OutputFile.Location.GetFileName());
			FileReference LinkScriptFullPath = FileReference.Combine(LinkEnvironment.IntermediateDirectory!, LinkScriptName);

			LinkAction.PrerequisiteItems.Add(FileItem.GetItemByFileReference(LinkScriptFullPath));

			{
				StringWriter LinkWriter = new();
				if (bUseCmdExe)
				{
					LinkWriter.NewLine = "\r\n";
					LinkWriter.WriteLine("@echo off");
					LinkWriter.WriteLine("rem Automatically generated by UnrealBuildTool");
					LinkWriter.WriteLine("rem *DO NOT EDIT*");
					LinkWriter.WriteLine();
					LinkWriter.WriteLine("set Retries=0");
					LinkWriter.WriteLine(":linkloop");
					LinkWriter.WriteLine("if %Retries% GEQ 1 goto failedtorelink");
					LinkWriter.WriteLine(LinkCommandString);
					LinkWriter.WriteLine("if %errorlevel% neq 0 goto sleepandretry");
					if (!needsRelink)
					{
						LinkWriter.WriteLine(GetDumpEncodeDebugCommand(LinkEnvironment, OutputFile, LinkAction));
					}
					LinkWriter.WriteLine("exit 0");
					LinkWriter.WriteLine(":sleepandretry");
					LinkWriter.WriteLine("ping 127.0.0.1 -n 1 -w 5000 >NUL 2>NUL");     // timeout complains about lack of redirection
					LinkWriter.WriteLine("set /a Retries+=1");
					LinkWriter.WriteLine("goto linkloop");
					LinkWriter.WriteLine(":failedtorelink");
					LinkWriter.WriteLine("echo Failed to link {0} after %Retries% retries", OutputFile.AbsolutePath);
					LinkWriter.WriteLine("exit 1");
				}
				else
				{
					LinkWriter.NewLine = "\n";
					LinkWriter.WriteLine("#!/bin/sh");
					LinkWriter.WriteLine("# Automatically generated by UnrealBuildTool");
					LinkWriter.WriteLine("# *DO NOT EDIT*");
					LinkWriter.WriteLine();
					LinkWriter.WriteLine("set -o errexit");
					LinkWriter.WriteLine(LinkCommandString);
					if (!needsRelink)
					{
						LinkWriter.WriteLine(GetDumpEncodeDebugCommand(LinkEnvironment, OutputFile, LinkAction));
					}
				}
				Directory.CreateDirectory(Path.GetDirectoryName(LinkScriptFullPath.FullName)!);
				Graph.CreateIntermediateTextFile(LinkScriptFullPath, LinkWriter.ToString());
			}

			LinkAction.CommandPath = ShellBinary;

			// This must maintain the quotes around the LinkScriptFullPath
			LinkAction.CommandArguments = ExecuteSwitch + " \"" + LinkScriptFullPath.FullName + "\"";

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			// Prepare a script that will run later, once all shared libraries and the executable
			// are created. This script will be called by action created in FixDependencies()
			if (needsRelink)
			{
				// Create the action to relink the library. This actions does not overwrite the source file so it can be executed in parallel
				Action RelinkAction = Graph.CreateAction(ActionType.Link);
				RelinkAction.RootPaths = LinkEnvironment.RootPaths;
				RelinkAction.WorkingDirectory = LinkAction.WorkingDirectory;
				RelinkAction.StatusDescription = LinkAction.StatusDescription;
				RelinkAction.CommandDescription = "Relink";
				RelinkAction.bCanExecuteRemotely = false;
				RelinkAction.CacheBucket = GetCacheBucket(TargetRules, null);
				RelinkAction.ArtifactMode = ArtifactMode.Enabled;
				RelinkAction.ProducedItems.Add(RealOutputFile);

				foreach (FileItem Dependency in EngineAndGameLibrariesFiles)
				{
					RelinkAction.PrerequisiteItems.Add(Dependency);
				}
				RelinkAction.PrerequisiteItems.Add(OutputFile); // also depend on the first link action's output

				string LinkOutputFileForwardSlashes = OutputFile.AbsolutePath.Replace("\\", "/");
				string RelinkedFileForwardSlashes = RealOutputFile.AbsolutePath.Replace("\\", "/");

				string EngineAndGameLibrariesString = "";
				foreach (string Library in EngineAndGameLibrariesLinkFlags)
				{
					EngineAndGameLibrariesString += Library;
				}

				// create the relinking step
				string RelinkScriptName = String.Format((bUseCmdExe ? "Relink-{0}.bat" : "Relink-{0}.sh"), RealOutputFile.Location.GetFileName());
				FileReference RelinkScriptFullPath = FileReference.Combine(LinkEnvironment.IntermediateDirectory!, RelinkScriptName);

				RelinkAction.PrerequisiteItems.UnionWith(LinkAction.PrerequisiteItems);
				RelinkAction.PrerequisiteItems.Add(FileItem.GetItemByFileReference(RelinkScriptFullPath));

				
				{
					StringWriter RelinkWriter = new();
					string RelinkInvocation = LinkCommandString;
					string Replace = "-Wl,--allow-shlib-undefined";
					RelinkInvocation = RelinkInvocation.Replace(Replace, EngineAndGameLibrariesString);

					// should be the same as RelinkedFileRef
					RelinkInvocation = RelinkInvocation.Replace(LinkOutputFileForwardSlashes, RelinkedFileForwardSlashes);
					RelinkInvocation = RelinkInvocation.Replace("$", "\\$");

					if (bUseCmdExe)
					{
						RelinkWriter.WriteLine("@echo off");
						RelinkWriter.WriteLine("rem Automatically generated by UnrealBuildTool");
						RelinkWriter.WriteLine("rem *DO NOT EDIT*");
						RelinkWriter.WriteLine();
						RelinkWriter.WriteLine("set Retries=0");
						RelinkWriter.WriteLine(":relinkloop");
						RelinkWriter.WriteLine("if %Retries% GEQ 10 goto failedtorelink");
						RelinkWriter.WriteLine(RelinkInvocation);
						RelinkWriter.WriteLine("if %errorlevel% neq 0 goto sleepandretry");
						RelinkWriter.WriteLine(GetDumpEncodeDebugCommand(LinkEnvironment, RealOutputFile, RelinkAction));
						RelinkWriter.WriteLine("exit 0");
						RelinkWriter.WriteLine(":sleepandretry");
						RelinkWriter.WriteLine("ping 127.0.0.1 -n 1 -w 5000 >NUL 2>NUL");     // timeout complains about lack of redirection
						RelinkWriter.WriteLine("set /a Retries+=1");
						RelinkWriter.WriteLine("goto relinkloop");
						RelinkWriter.WriteLine(":failedtorelink");
						RelinkWriter.WriteLine("echo Failed to relink {0} after %Retries% retries", OutputFile.AbsolutePath);
						RelinkWriter.WriteLine("exit 1");
					}
					else
					{
						RelinkWriter.NewLine = "\n";
						RelinkWriter.WriteLine("#!/bin/sh");
						RelinkWriter.WriteLine("# Automatically generated by UnrealBuildTool");
						RelinkWriter.WriteLine("# *DO NOT EDIT*");
						RelinkWriter.WriteLine();
						RelinkWriter.WriteLine("set -o errexit");
						RelinkWriter.WriteLine(RelinkInvocation);
						RelinkWriter.WriteLine(GetDumpEncodeDebugCommand(LinkEnvironment, RealOutputFile, RelinkAction));
					}

					Directory.CreateDirectory(Path.GetDirectoryName(RelinkScriptFullPath.FullName)!);
					Graph.CreateIntermediateTextFile(RelinkScriptFullPath, RelinkWriter.ToString());
				}

				RelinkAction.CommandPath = ShellBinary;
				RelinkAction.CommandArguments = ExecuteSwitch + " \"" + RelinkScriptFullPath + "\"";
			}
			return RealOutputFile;
		}

		public override void SetupBundleDependencies(ReadOnlyTargetRules Target, IEnumerable<UEBuildBinary> Binaries, string GameName)
		{
			// Populate the lookup table from real output to prelink output
			lock (LinkToPrelinkFileList)
			{
				foreach (UEBuildBinary binary in Binaries)
				{
					if (binary.bCreateImportLibrarySeparately)
					{
						FileItem output = FileItem.GetItemByFileReference(binary.OutputFilePath);
						LinkToPrelinkFileList.Add(output, GetPrelinkOutput(binary.IntermediateDirectory, output));
					}
				}
			}
		}

		public void StripSymbols(FileReference SourceFile, FileReference TargetFile, ILogger Logger)
		{
			if (SourceFile != TargetFile)
			{
				// Strip command only works in place so we need to copy original if target is different
				File.Copy(SourceFile.FullName, TargetFile.FullName, true);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = LinuxInfo.Objcopy.FullName;
			StartInfo.Arguments = "--strip-debug \"" + TargetFile.FullName + "\"";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo, Logger);
		}

		public override void AddExtraToolArguments(IList<string> ExtraArguments)
		{
			// We explicitly include the clang include directories so tools like IWYU can run outside of the clang directory.
			// More info: https://github.com/include-what-you-use/include-what-you-use#how-to-install
			string? InternalSdkPath = UEBuildPlatform.GetSDK(UnrealTargetPlatform.Linux)!.GetInternalSDKPath();
			if (InternalSdkPath != null)
			{
				// starting with clang 16.x the directory naming changed to include major version only
				string ClangVersionString = (Info.ClangVersion.Major >= 16) ? Info.ClangVersion.Major.ToString() : Info.ClangVersion.ToString();
				ExtraArguments.Add(String.Format("-isystem {0}", System.IO.Path.Combine(InternalSdkPath, "lib", "clang", ClangVersionString, "include").Replace("\\", "/")));
				ExtraArguments.Add(String.Format("-isystem {0}", System.IO.Path.Combine(InternalSdkPath, "usr", "include").Replace("\\", "/")));
			}
		}

		public override string GetExtraLinkFileExtension()
		{
			return "ldscript";
		}
	}
}
