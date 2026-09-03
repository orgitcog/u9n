// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using EpicGames.Core;

namespace UnrealBuildBase
{
	/// <summary>
	/// System-wide mutex allowing only one instance of the program to run at a time
	/// </summary>
	public class GlobalSingleInstanceMutex : IDisposable
	{
		/// <summary>
		/// The global mutex instance
		/// </summary>
		Mutex? GlobalMutex;

		/// <summary>
		/// Constructor. Attempts to acquire the global mutex
		/// </summary>
		/// <param name="mutexName">Name of the mutex to acquire</param>
		/// <param name="bWaitMutex">Allow waiting for the mutex to be acquired</param>
		public GlobalSingleInstanceMutex(string mutexName, bool bWaitMutex)
		{
			// Try to create the mutex, with it initially locked
			bool bCreatedMutex;
			GlobalMutex = new Mutex(true, mutexName, out bCreatedMutex);

			// If we didn't create the mutex, we can wait for it or fail immediately
			if (!bCreatedMutex)
			{
				if (bWaitMutex)
				{
					try
					{
						GlobalMutex.WaitOne();
					}
					catch (AbandonedMutexException)
					{
					}
				}
				else
				{
					throw new BuildLogEventException(new CompilationResultException(CompilationResult.ConflictingInstance), "A conflicting instance of {Mutex} is already running.", mutexName);
				}
			}
		}

		/// <summary>
		/// Gets the name of a mutex unique for the given path
		/// </summary>
		/// <param name="name">Base name of the mutex</param>
		/// <param name="uniquePath">Path to identify a unique mutex</param>
		public static string GetUniqueMutexForPath(string name, string uniquePath)
		{
			// generate an IoHash of the path, as GetHashCode is not guaranteed to generate a stable hash
			return $"Global\\{name}_{IoHash.Compute(uniquePath.ToUpperInvariant())}";
		}

		/// <summary>
		/// Gets the name of a mutex unique for the given path
		/// </summary>
		/// <param name="name">Base name of the mutex</param>
		/// <param name="uniquePath">Path to identify a unique mutex</param>
		public static string GetUniqueMutexForPath(string name, FileSystemReference? uniquePath) => GetUniqueMutexForPath(name, uniquePath?.FullName ?? String.Empty);

		/// <summary>
		/// Release the mutex and dispose of the object
		/// </summary>
		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		/// <summary>
		/// Overridable dispose method
		/// </summary>
		/// <param name="disposing">Whether the object should be disposed</param>
		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
				GlobalMutex?.ReleaseMutex();
				GlobalMutex?.Dispose();
				GlobalMutex = null;
			}
		}
	}
}
