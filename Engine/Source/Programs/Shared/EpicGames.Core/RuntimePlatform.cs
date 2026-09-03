// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Core
{
	/// <summary>
	/// Allows testing different platforms
	/// </summary>
	public static class RuntimePlatform
	{
		/// <summary>
		/// Whether we are currently running on Linux.
		/// </summary>
		[Obsolete("Replace with OperatingSystem.IsLinux()")]
		public static bool IsLinux => OperatingSystem.IsLinux();

		/// <summary>
		/// Whether we are currently running on a MacOS platform.
		/// </summary>
		[Obsolete("Replace with OperatingSystem.IsMacOS()")]
		public static bool IsMac => OperatingSystem.IsMacOS();

		/// <summary>
		/// Whether we are currently running a Windows platform.
		/// </summary>
		[Obsolete("Replace with OperatingSystem.IsWindows()")]
		public static bool IsWindows => OperatingSystem.IsWindows();

		/// <summary>
		/// The platform type
		/// </summary>
		public enum Type
		{
			/// <summary>
			/// Windows
			/// </summary>
			Windows, 
			
			/// <summary>
			/// Linux
			/// </summary>
			Linux, 
			
			/// <summary>
			/// Mac
			/// </summary>
			Mac
		};

		/// <summary>
		/// The current runtime platform
		/// </summary>
		public static readonly Type Current = OperatingSystem.IsWindows() ? Type.Windows : OperatingSystem.IsMacOS() ? Type.Mac : Type.Linux;

		/// <summary>
		/// The extension executables have on the current platform
		/// </summary>
		public static readonly string ExeExtension = OperatingSystem.IsWindows() ? ".exe" : "";
	}
}
