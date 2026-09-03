// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;

namespace UnrealBuildTool
{
	internal static class ClangWarnings
	{
		internal static void GetEnabledWarnings(List<string> Arguments)
		{
			Arguments.Add("-Wdelete-non-virtual-dtor");                 // https://clang.llvm.org/docs/DiagnosticsReference.html#wdelete-non-virtual-dtor
			Arguments.Add("-Wenum-conversion");                         // https://clang.llvm.org/docs/DiagnosticsReference.html#wenum-conversion
			Arguments.Add("-Wbitfield-enum-conversion");                // https://clang.llvm.org/docs/DiagnosticsReference.html#wbitfield-enum-conversion
		}

		internal static void GetDisabledWarnings(CppCompileEnvironment CompileEnvironment, StaticAnalyzer Analyzer, VersionNumber ClangVersion, List<string> Arguments)
		{
			Arguments.AddRange(CompileEnvironment.CppCompileWarnings.GenerateWarningCommandLineArgs(CompileEnvironment, typeof(ClangToolChain), ClangVersion));
		}

		internal static void GetHeaderDisabledWarnings(List<string> Arguments)
		{
			// This warning was to catch #pragma once inside a source file.
			// If we're compiling a header directly, we should always have the pragma once, so we need to ignore this warning.
			Arguments.Add("-Wno-pragma-once-outside-header");
			Arguments.Add("-Wno-#pragma-messages");
		}

		// TODO: Get valid checkers by calling clang -cc1 -analyzer-checker-help
		static Lazy<Dictionary<string, int>> CheckerAddedVersion = new Lazy<Dictionary<string, int>>(() => new()
			{
				{ "core.BitwiseShift", 18 },
				{ "optin.core.EnumCastOutOfRange", 18 },
				{ "security.cert.env.InvalidPtr", 18 },
				{ "unix.Errno", 18 },
				{ "unix.StdCLibraryFunctions", 18 },
				{ "cplusplus.ArrayDelete", 19 },
				{ "optin.taint.TaintedAlloc", 19 },
				{ "security.PutenvStackArray", 19 },
				{ "security.SetgidSetuidOrder", 19 },
				{ "unix.BlockInCriticalSection", 19 },
				{ "unix.Stream", 19 },
			});

		internal static bool IsAvailableAnalyzerChecker(string Checker, VersionNumber ClangVersion) => !CheckerAddedVersion.Value.ContainsKey(Checker) || ClangVersion.Components[0] >= CheckerAddedVersion.Value[Checker];
	}
}
