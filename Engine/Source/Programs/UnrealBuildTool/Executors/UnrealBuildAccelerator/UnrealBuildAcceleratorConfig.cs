// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Generic;

namespace UnrealBuildTool
{
	/// <summary>
	/// Configuration for Unreal Build Accelerator
	/// </summary>
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE1006:Naming Styles", Justification = "UnrealBuildTool naming style")]
	class UnrealBuildAcceleratorConfig
	{
		/// <summary>
		/// List of provider names for remote connections.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		public string[] Providers { get; set; } = [];

		/// <summary>
		/// List of provider names for remote connections from ini
		/// It is recommended to set Providers in ini so the setting can be shared with the uba controller in editor, such as
		/// Windows:	%ProgramData%/Epic/Unreal Engine/Engine/Config/SystemEngine.ini
		/// Mac:		~/Library/Application Support/Epic/Unreal Engine/Engine/Config/SystemEngine.ini
		/// Linux:		~/.config/Epic/Unreal Engine/Engine/Config/SystemEngine.ini
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "UbaController", "Providers")]
		[CommandLine("-UBAProviders=", ListSeparator = '+')]
		public List<string> IniProviders { get; set; } = [];

		/// <summary>
		/// List of provider names for remote connections
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		public string[] BuildMachineProviders { get; set; } = [];

		/// <summary>
		/// List of provider names for remote connections from ini
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "UbaController", "BuildMachineProviders")]
		public List<string> IniBuildMachineProviders { get; set; } = [];

		/// <summary>
		/// List of provider names for remote cache.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		public string[] CacheProviders { get; set; } = [];

		/// <summary>
		/// List of provider names for remote cache from ini
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "UbaController", "CacheProviders")]
		[CommandLine("-UBACacheProviders=", ListSeparator = '+')]
		public List<string> IniCacheProviders { get; set; } = [];

		/// <summary>
		/// List of provider names for remote cache
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		public string[] BuildMachineCacheProviders { get; set; } = [];

		/// <summary>
		/// List of provider names for remote cache from ini
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "UbaController", "BuildMachineCacheProviders")]
		public List<string> IniBuildMachineCacheProviders { get; set; } = [];

		/// <summary>
		/// When set to true, UBA will not use any remote help
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBADisableRemote")]
		public bool bDisableRemote { get; set; } = false;

		/// <summary>
		/// When set to true, UBA will force all actions that can be built remotely to be built remotely. This will hang if there are no remote agents available
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAForceRemote")]
		public bool bForceBuildAllRemote { get; set; } = false;

		/// <summary>
		/// When set to true, actions that fail locally with certain error codes will retry without uba.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBANoRetry", Value = "false")]
		public bool bAllowRetry { get; set; } = true;

		/// <summary>
		/// When set to true, all actions that fail locally with UBA will be retried without UBA.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAForcedRetry")]
		public bool bForcedRetry { get; set; } = false;

		/// <summary>
		/// When set to true, actions that fail remotely with UBA will be retried locally with UBA.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAForcedRetryRemote")]
		public bool bForcedRetryRemote { get; set; } = false;

		/// <summary>
		/// When set to true, all errors and warnings from UBA will be output at the appropriate severity level to the log (rather than being output as 'information' and attempting to continue regardless).
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAStrict")]
		public bool bStrict { get; set; } = false;

		/// <summary>
		/// If UBA should store cas compressed or raw
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAStoreRaw")]
		public bool bStoreRaw { get; set; } = false;

		/// <summary>
		/// If UBA should distribute linking to remote workers. This needs bandwidth but can be an optimization
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBALinkRemote")]
		public bool bLinkRemote { get; set; } = false;

		/// <summary>
		/// The amount of gigabytes UBA is allowed to use to store workset and cached data. It is a good idea to have this >10gb
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAStoreCapacityGb")]
		public int StoreCapacityGb { get; set; } = 40;

		/// <summary>
		/// Max number of worker threads that can handle messages from remotes. 
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAMaxWorkers")]
		public int MaxWorkers { get; set; } = 192;

		/// <summary>
		/// Max size of each message sent from server to client
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBASendSize")]
		public int SendSize { get; set; } = 256 * 1024;

		/// <summary>
		/// Which ip UBA server should listen to for connections
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAHost")]
		public string Host { get; set; } = "0.0.0.0";

		/// <summary>
		/// Which port UBA server should listen to for connections.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAPort")]
		public int Port { get; set; } = 1345;

		/// <summary>
		/// Which directory to store files for UBA.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBARootDir")]
		public string? RootDir { get; set; } = null;

		/// <summary>
		/// Use Quic protocol instead of Tcp (experimental)
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAQuic", Value = "true")]
		public bool bUseQuic { get; set; } = false;

		/// <summary>
		/// Enable logging of UBA processes
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBALog", Value = "true")]
		public bool bLogEnabled { get; set; } = false;

		/// <summary>
		/// Prints summary of UBA stats at end of build
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAPrintSummary", Value = "true")]
		public bool bPrintSummary { get; set; } = false;

		/// <summary>
		/// Launch visualizer application which shows build progress
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAVisualizer", Value = "true")]
		public bool bLaunchVisualizer { get; set; } = false;

		/// <summary>
		/// Resets the cas cache
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAResetCas", Value = "true")]
		public bool bResetCas { get; set; } = false;

		/// <summary>
		/// Provide custom path for trace output file
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBATraceOutputFile")]
		public string TraceFile { get; set; } = String.Empty;

		/// <summary>
		/// Add verbose details to the UBA trace
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBADetailedTrace", Value = "true")]
		public bool bDetailedTrace { get; set; }

		/// <summary>
		/// Disable UBA waiting on available memory before spawning new processes
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBADisableWaitOnMem", Value = "true")]
		public bool bDisableWaitOnMem { get; set; }

		/// <summary>
		/// Let UBA kill running processes when close to out of memory
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAAllowKillOnMem", Value = "true")]
		public bool bAllowKillOnMem { get; set; }

		/// <summary>
		/// Store object (.obj) compressed on disk. Requires uba to do link step where it will decompress obj files again
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAStoreObjFilesCompressed", Value = "true")]
		[CommandLine("-UBADisableStoreObjFilesCompressed", Value = "false")]
		public bool bStoreObjFilesCompressed { get; set; }

		/// <summary>
		/// Skip writing intermediate and output files to disk. Useful for validation builds where we don't need the output
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBANoWrite", Value = "false")]
		public bool bWriteToDisk { get; set; } = true;

		/// <summary>
		/// Set to true to disable mimalloc and detouring of memory allocations.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBANoCustomMalloc", Value = "true")]
		public bool bDisableCustomAlloc { get; set; } = false;

		/// <summary>
		/// The zone to use for UBA.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAZone=")]
		public string Zone { get; set; } = String.Empty;

		/// <summary>
		/// Set to true to enable encryption when transferring files over the network.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBACrypto", Value = "true")]
		public bool bUseCrypto { get; set; } = false;

		/// <summary>
		/// Set to true to provide known inputs to processes that are run remote. This is an experimental feature to speed up build times when ping is higher
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAUseKnownInputs", Value = "true")]
		public bool bUseKnownInputs { get; set; } = true;

		/// <summary>
		/// Write yaml file with all actions that are queued for build. This can be used to replay using "UbaCli.exe local file.yaml"
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAActionsOutputFile")]
		public string ActionsOutputFile { get; set; } = String.Empty;

		/// <summary>
		/// Set to true to see more info about what is happening inside uba and also log output from agents
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBADetailedLog", Value = "true")]
		public bool bDetailedLog { get; set; } = false;

		/// <summary>
		/// Disable all detouring making all actions run outside uba
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator", Name = "AllowDetour")]
		[CommandLine("-UBANoDetour", Value = "false")]
		public bool bAllowDetour { get; set; } = true;

		/// <summary>
		/// Max number of cache download tasks that can execute in parallel
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator", Name = "CacheMaxWorkers")]
		[CommandLine("-UBACacheMaxWorkers")]
		public int CacheMaxWorkers { get; set; } = 32;

		/// <summary>
		/// Shuffle the list of cache servers before each read attempt
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator", Name = "CacheShuffle")]
		[CommandLine("-UBAShuffleCache")]
		public bool bCacheShuffle { get; set; } = false;

		/// <summary>
		/// Report reason a cache miss happened. Useful when searching for determinism/portability issues
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator", Name = "ReportCacheMissReason")]
		[CommandLine("-UBAReportCacheMissReason")]
		public bool bReportCacheMissReason { get; set; }

		/// <summary>
		/// Compression level. Options are None, SuperFast, VeryFast, Fast, Normal, Optimal1 to Optimal5
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBACompressionLevel")]
		public string CompressionLevel { get; set; } = String.Empty;
	}
}