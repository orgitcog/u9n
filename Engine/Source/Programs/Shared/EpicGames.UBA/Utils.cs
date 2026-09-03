// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;

namespace EpicGames.UBA
{
	/// <summary>
	/// Utils
	/// </summary>
	public static partial class Utils
	{
		/// <summary>
		/// Is UBA available?
		/// </summary>
		public static bool IsAvailable() => s_available.Value;
		static readonly Lazy<bool> s_available = new(() => File.Exists(GetLibraryPath()));

		/// <summary>
		/// Paths that are not allowed to be transferred over the network for UBA remote agents.
		/// </summary>
		/// <returns>enumerable of disallowed paths</returns>
		public static IEnumerable<string> DisallowedPaths => s_disallowedPaths.Distinct().Order();
		static readonly ConcurrentBag<string> s_disallowedPaths = [];

		/// <summary>
		/// Mapping of binary paths for cross architecture host binaries, to allow for using helpers of a different architecture.
		/// </summary>
		/// <returns>dictionary of binary mappings, where the key is the binary for the current host architecture</returns>
		public static IReadOnlyDictionary<string, string> CrossArchitecturePaths => s_crossArchitecturePaths;
		static readonly ConcurrentDictionary<string, string> s_crossArchitecturePaths = [];

		/// <summary>
		/// Mapping of a folder path to a single hash, to allow for hashing an entire folder so each individual file does not need to be processed.
		/// </summary>
		public static IReadOnlyDictionary<string, string> PathHashes => s_pathHashes;
		static readonly ConcurrentDictionary<string, string> s_pathHashes = [];

		/// <summary>
		/// Registers a path that is not allowed to be transferred over the network for UBA remote agents.
		/// </summary>
		/// <param name="paths">The paths to add to the disallowed list</param>
		public static void RegisterDisallowedPaths(params string[] paths)
		{
			foreach (string path in paths)
			{
				if (!s_disallowedPaths.Contains(path))
				{
					s_disallowedPaths.Add(path);
				}
			}
			DisallowedPathRegistered?.Invoke(s_disallowedPaths, new(paths));
		}

		/// <summary>
		/// Registers a path mapping for cross architecture binaries
		/// </summary>
		/// <param name="path">host architecture path</param>
		/// <param name="crossPath">cross architecture path</param>
		public static void RegisterCrossArchitecturePath(string path, string crossPath)
		{
			if (s_crossArchitecturePaths.TryAdd(path, crossPath))
			{
				CrossArchitecturePathRegistered?.Invoke(s_crossArchitecturePaths, new(path, crossPath));
			}
		}

		/// <summary>
		/// Registers a hash for a path
		/// </summary>
		/// <param name="path">The path string</param>
		/// <param name="hash">The hash string</param>
		public static void RegisterPathHash(string path, string hash)
		{
			if (s_pathHashes.TryAdd(path, hash))
			{
				PathHashRegistered?.Invoke(s_pathHashes, new(path, hash));
			}
		}

		/// <summary>
		/// Delegate for registering a remote disallowed path
		/// </summary>
		/// <param name="sender">collection that is being changed</param>
		/// <param name="e">event args containing which paths were added</param>
		public delegate void DisallowedPathRegisteredEventHandler(IReadOnlyCollection<string> sender, DisallowedPathRegisteredEventArgs e);

		/// <summary>
		/// Delegate for registering a cross architecture path
		/// </summary>
		/// <param name="sender">collection that is being changed</param>
		/// <param name="e">event args containing which path was added</param>
		public delegate void CrossArchitecturePathRegisteredEventHandler(IReadOnlyDictionary<string, string> sender, CrossArchitecturePathRegisteredEventArgs e);

		/// <summary>
		/// Delegate for registering a path hash
		/// </summary>
		/// <param name="sender">collection that is being changed</param>
		/// <param name="e">event args containing which path was added</param>
		public delegate void PathHashRegisteredEventHandler(IReadOnlyDictionary<string, string> sender, PathHashRegisteredEventArgs e);

		/// <summary>
		/// Remote disallowed path registered event handler
		/// </summary>
		public static event DisallowedPathRegisteredEventHandler? DisallowedPathRegistered;

		/// <summary>
		/// Cross architecture path registered event handler
		/// </summary>
		public static event CrossArchitecturePathRegisteredEventHandler? CrossArchitecturePathRegistered;

		/// <summary>
		/// Remote disallowed path registered event handler
		/// </summary>
		public static event PathHashRegisteredEventHandler? PathHashRegistered;

		/// <summary>
		/// Get the path to the p/invoke library that would be loaded 
		/// </summary>
		/// <returns>The path to the library</returns>
		/// <exception cref="PlatformNotSupportedException">If the operating system is not supported</exception>
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Globalization", "CA1308:Normalize strings to uppercase", Justification = "folder path is lowercase")]
		static string GetLibraryPath()
		{
			string arch = RuntimeInformation.ProcessArchitecture.ToString().ToLowerInvariant();
			string assemblyFolder = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location)!;
			if (OperatingSystem.IsWindows())
			{
				return Path.Combine(assemblyFolder, "runtimes", $"win-{arch}", "native", "UbaHost.dll");
			}
			else if (OperatingSystem.IsLinux())
			{
				return Path.Combine(assemblyFolder, "runtimes", $"linux-{arch}", "native", "libUbaHost.so");
			}
			else if (OperatingSystem.IsMacOS())
			{
				return Path.Combine(assemblyFolder, "runtimes", $"osx-{arch}", "native", "libUbaHost.dylib");
			}
			throw new PlatformNotSupportedException();
		}
	}

	/// <summary>
	/// Event args for registering a remote disallowed path
	/// </summary>
	public sealed class DisallowedPathRegisteredEventArgs(params string[] paths) : EventArgs
	{
		/// <summary>
		/// The paths being registered
		/// </summary>
		public IEnumerable<string> Paths { get; } = paths;
	}

	/// <summary>
	/// Event args for registering a cross architecture path
	/// </summary>
	public sealed class CrossArchitecturePathRegisteredEventArgs(string path, string crossPath) : EventArgs
	{
		/// <summary>
		/// The host architecture path being registered
		/// </summary>
		public string Path { get; } = path;

		/// <summary>
		/// The cross architecture path being registered
		/// </summary>
		public string CrossPath { get; } = crossPath;
	}

	/// <summary>
	/// Event args for registering a path hash
	/// </summary>
	public sealed class PathHashRegisteredEventArgs(string path, string hash) : EventArgs
	{
		/// <summary>
		/// The path being registered
		/// </summary>
		public string Path { get; } = path;

		/// <summary>
		/// The hash for the path being registered
		/// </summary>
		public string Hash { get; } = hash;
	}
}
