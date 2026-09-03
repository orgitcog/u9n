// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Projects;
using EpicGames.UBA;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Management;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Threading;
using System.Threading.Tasks;
using UnrealBuildBase;
using UnrealBuildTool.Artifacts;

namespace UnrealBuildTool
{
	class UBAExecutor : ParallelExecutor
	{
		public UnrealBuildAcceleratorConfig UBAConfig { get; init; } = new();

		public string Crypto { get; private set; } = String.Empty;
		public IServer? Server { get; private set; }
		ISessionServer? _session;
		object _sessionLock = new();
		IEnumerable<UnrealBuildAcceleratorCacheConfig> _cacheConfigs = [];
		Dictionary<UnrealBuildAcceleratorCacheConfig, ICacheClient> _cacheClients = [];
		EpicGames.UBA.ILogger? _ubaLogger;
		readonly List<IUBAAgentCoordinator> _agentCoordinators = new();
		DirectoryReference? _rootDirRef;
		bool _bIsCancelled;
		bool _bIsRemoteActionsAllowed = true;
		readonly object _actionsChangedLock = new();
		bool _bActionsChanged = true;
		uint _errorCount = 0;
		uint _actionsQueuedThatCanRunRemotely = UInt32.MaxValue;
		readonly ThreadedLogger _threadedLogger;

		DateTime _ubaStartTimeUtc = DateTime.UtcNow;
		TimeSpan _ubaDurationWaitingForRemote = TimeSpan.Zero;

		// Tracking for LinkedActions that failed remotely that should be retried locally
		readonly ConcurrentDictionary<LinkedAction, bool> _localRetryActions = new();
		// Tracking for LinkedActions that failed locally that should be retried without UBA
		readonly ConcurrentDictionary<LinkedAction, bool> _forcedRetryActions = new();

		// Tracking for successful coordinator connections
		int _successfulCoordinatorConnections = 0;
		// Tracking for failed coordinator connections
		int _failedCoordinatorConnections = 0;

		// Tracking for all actions processed locally
		int _localProcessedActions = 0;
		// Tracking for all actions processed remotely
		int _remoteProcessedActions = 0;

		int _cacheableActionsLeft = 0;

		// Tracking for remote connection mode
		string _remoteConnectionMode = "Local";

		protected override void Dispose(bool disposing)
		{
			if (disposing)
			{
				foreach (KeyValuePair<UnrealBuildAcceleratorCacheConfig, ICacheClient> item in _cacheClients)
				{
					item.Value.Dispose();
				}
				_cacheClients.Clear();
				_session?.Dispose();
				_session = null;
				_threadedLogger.Dispose();
				_ubaLogger?.Dispose();
				_ubaLogger = null;
			}
			base.Dispose(disposing);
		}

		public override string Name => !UBAConfig.bDisableRemote && _agentCoordinators.Count > 0 ? "Unreal Build Accelerator" : "Unreal Build Accelerator local";

		public static new bool IsAvailable()
		{
			return EpicGames.UBA.Utils.IsAvailable();
		}

		public delegate void SessionCreatedDelegate(ISessionServer Session);

		// Event invoked when ISessionServer is created. Can be used to for example register virtual files etc
		public event SessionCreatedDelegate? OnSessionCreated;

		public static DirectoryReference UbaBinariesDir
		{
			get
			{
				if (OperatingSystem.IsWindows())
				{
					#pragma warning disable CA1308 // Normalize strings to uppercase
					return DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "Win64", "UnrealBuildAccelerator", RuntimeInformation.ProcessArchitecture.ToString().ToLowerInvariant());
					#pragma warning restore CA1308 // Normalize strings to uppercase
				}
				else if (OperatingSystem.IsLinux())
				{
					if (RuntimeInformation.ProcessArchitecture == Architecture.X64)
					{
						return DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "Linux", "UnrealBuildAccelerator");
					}
					else if (RuntimeInformation.ProcessArchitecture == Architecture.Arm64)
					{
						return DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "LinuxArm64", "UnrealBuildAccelerator");
					}
				}
				else if (OperatingSystem.IsMacOS())
				{
					return DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "Mac", "UnrealBuildAccelerator");
				}
				throw new PlatformNotSupportedException();
			}
		}

		public UBAExecutor(int maxLocalActions, bool bAllCores, bool bCompactOutput, Microsoft.Extensions.Logging.ILogger logger, IEnumerable<TargetDescriptor> targetDescriptors)
			: base(maxLocalActions, bAllCores, bCompactOutput, logger)
		{
			XmlConfig.ApplyTo(this);
			XmlConfig.ApplyTo(UBAConfig);
			CommandLine.ParseArguments(Environment.GetCommandLineArgs(), this, logger);

			bool bAnyHotReload = false;
			foreach (TargetDescriptor targetDescriptor in targetDescriptors)
			{
				ConfigCache.ReadSettings(targetDescriptor.ProjectFile?.Directory, BuildHostPlatform.Current.Platform, UBAConfig);
				bAnyHotReload = bAnyHotReload || targetDescriptor.HotReloadMode != HotReloadMode.Disabled;
				targetDescriptor.AdditionalArguments.ApplyTo(this);
				targetDescriptor.AdditionalArguments.ApplyTo(UBAConfig);
			}

			// Disable cache server when using hot reload path (there would likely never be any cache hits anyway
			if (!bAnyHotReload)
			{
				LoadCacheConfigs(targetDescriptors, logger);
			}

			if (UBAConfig.bUseCrypto)
			{
				Crypto = CreateCrypto();
			}

			_agentCoordinators.AddRange(UBAAgentCoordinatorHorde.Init(logger, this, targetDescriptors, UBAConfig, ref _remoteConnectionMode));
			_threadedLogger = new ThreadedLogger(logger);
		}

		private void LoadCacheConfigs(IEnumerable<TargetDescriptor> targetDescriptors, Microsoft.Extensions.Logging.ILogger logger)
		{
			const string ProviderPrefix = "Uba.CacheProvider";

			IEnumerable<string> providers = Unreal.IsBuildMachine()
			? [.. UBAConfig.BuildMachineCacheProviders, .. UBAConfig.IniBuildMachineCacheProviders]
			: [.. UBAConfig.CacheProviders, .. UBAConfig.IniCacheProviders];

			if (!providers.Any())
			{
				providers = [ProviderPrefix];
			}

			List<UnrealBuildAcceleratorCacheConfig> configs = [];
			foreach (string provider in providers.Distinct().Where(x => x.StartsWith(ProviderPrefix)))
			{
				foreach (TargetDescriptor targetDescriptor in targetDescriptors)
				{
					UnrealBuildAcceleratorCacheConfig config = new UnrealBuildAcceleratorCacheConfig();
					if (provider == ProviderPrefix)
					{
						XmlConfig.ApplyTo(config);
						CommandLine.ParseArguments(Environment.GetCommandLineArgs(), this, logger);
					}
					ConfigHierarchy engineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, targetDescriptor.ProjectFile?.Directory, BuildHostPlatform.Current.Platform);
					config.LoadConfigProvider(engineIni, provider);
					targetDescriptor.AdditionalArguments.ApplyTo(config);
					configs.Add(config);
				}
			}

			_cacheConfigs = configs.DistinctBy(x => x.CacheServer).Where(x => !String.IsNullOrEmpty(x.CacheServer));
		}

		public void UpdateStatus(uint statusRow, uint statusColumn, string statusText, LogEntryType statusType, string? statusLink)
		{
			lock (_sessionLock)
			{
				_session?.UpdateStatus(statusRow, statusColumn, statusText, statusType, statusLink);
			}
		}

		private void PrintConfiguration()
		{
			_threadedLogger.LogInformation("  Storage capacity {StoreCapacityGb}Gb", UBAConfig.StoreCapacityGb);
			if (UBAConfig.bStoreObjFilesCompressed)
			{
				_threadedLogger.LogInformation("  Object Compression Allowed");
			}
			_threadedLogger.LogDebug($"  RootDir {_rootDirRef}");
		}

		private async Task WriteActionOutputFileAsync(IEnumerable<LinkedAction> inputActions)
		{
			if (String.IsNullOrEmpty(UBAConfig.ActionsOutputFile))
			{
				return;
			}

			if (!UBAConfig.ActionsOutputFile.EndsWith(".yaml", StringComparison.OrdinalIgnoreCase))
			{
				_threadedLogger.LogError("UBA actions output file needs to have extension .yaml for UbaCli to understand it");
			}
			using System.IO.StreamWriter streamWriter = new(UBAConfig.ActionsOutputFile);
			using System.CodeDom.Compiler.IndentedTextWriter writer = new(streamWriter, "  ");
			await writer.WriteAsync("environment: ");
			await writer.WriteLineAsync(Environment.GetEnvironmentVariable("PATH"));
			await writer.WriteLineAsync("processes:");
			writer.Indent++;
			int index = 0;
			foreach (LinkedAction action in inputActions)
			{
				action.SortIndex = index++;
				await writer.WriteLineAsync($"- id: {action.SortIndex}");
				writer.Indent++;
				await writer.WriteLineAsync($"app: {action.CommandPath}");
				await writer.WriteLineAsync($"arg: {action.CommandArguments}");
				await writer.WriteLineAsync($"dir: {action.WorkingDirectory}");
				await writer.WriteLineAsync($"desc: {action.StatusDescription}");
				// TODO: Add cache roots
				if (action.Weight != 1.0f)
				{
					await writer.WriteLineAsync($"weight: {action.Weight}");
				}
				if (!action.bCanExecuteInUBA)
				{
					await writer.WriteLineAsync("detour: false");
				}
				else if (!action.bCanExecuteRemotely)
				{
					await writer.WriteLineAsync("remote: false");
				}
				if (action.PrerequisiteActions.Any())
				{
					await writer.WriteAsync("dep: [");
					await writer.WriteAsync(String.Join(", ", action.PrerequisiteActions.Select(x => x.SortIndex)));
					await writer.WriteLineAsync("]");
				}
				writer.Indent--;
				await writer.WriteLineNoTabsAsync(null);
			}
		}

		[System.Diagnostics.CodeAnalysis.SuppressMessage("Globalization", "CA1308:Normalize strings to uppercase", Justification = "lowercase crypto string")]
		public static string CreateCrypto()
		{
			byte[] bytes = new byte[16];
			using System.Security.Cryptography.RandomNumberGenerator random = System.Security.Cryptography.RandomNumberGenerator.Create();
			random.GetBytes(bytes);
			return BitConverter.ToString(bytes).Replace("-", "", StringComparison.OrdinalIgnoreCase).ToLowerInvariant(); // "1234567890abcdef1234567890abcdef";
		}

		public void AgentCoordinatorInitialized(IUBAAgentCoordinator coordinator, bool successful)
		{
			if (successful)
			{
				Interlocked.Add(ref _successfulCoordinatorConnections, 1);
			}
			else
			{
				Interlocked.Add(ref _failedCoordinatorConnections, 1);
			}
		}

		class UBAArtifactCache : IArtifactCache
		{
			public ArtifactCacheState State => ArtifactCacheState.Available;
			public Task<ArtifactCacheState> WaitForReadyAsync() => Task.FromResult(ArtifactCacheState.Available);
			public Task<ArtifactAction[]> QueryArtifactActionsAsync(IoHash[] partialKeys, CancellationToken cancellationToken) => Task.FromResult(Array.Empty<ArtifactAction>());
			public Task<bool[]?> QueryArtifactOutputsAsync(ArtifactAction[] artifactActions, CancellationToken cancellationToken) => Task.FromResult<bool[]?>(null);
			public Task SaveArtifactActionsAsync(ArtifactAction[] artifactActions, CancellationToken cancellationToken) => Task.CompletedTask;
			public Task FlushChangesAsync(CancellationToken cancellationToken) => Task.CompletedTask;
		}

		private static void RemoveLogLineSpam(List<string> logLines)
		{
			logLines.RemoveAll(line => 
				line.StartsWith("   Creating library ", StringComparison.Ordinal) ||
				line.StartsWith("   Creating object ", StringComparison.Ordinal) ||
				line.EndsWith("file(s) copied.", StringComparison.Ordinal));
		}

		class UBAActionArtifactCache : IActionArtifactCache
		{
			readonly UBAExecutor _executor;
			readonly UBAArtifactCache _cache = new UBAArtifactCache();
			public UBAActionArtifactCache(UBAExecutor executor) { _executor = executor; }
			public IArtifactCache ArtifactCache => _cache;
			public bool EnableReads { get => true; set { } }
			public bool EnableWrites { get => true; set { } }
			public bool LogCacheMisses { get => true; set { } }
			public DirectoryReference? EngineRoot { get => null; set { } }
			public DirectoryReference[]? DirectoryRoots { get => null; set { } }
			public Task<ActionArtifactResult> CompleteActionFromCacheAsync(LinkedAction action, CancellationToken cancellationToken)
			{
				return Task.Factory.StartNew(() =>
				{
					ProcessStartInfo startInfo = _executor.GetActionStartInfo(action);
					uint bucket = _executor.GetCacheBucket(action);
					ulong rootsHandle = _executor.GetActionRootsHandle(action);

					IEnumerable<KeyValuePair<UnrealBuildAcceleratorCacheConfig, ICacheClient>> cacheClients;
					if (_executor.UBAConfig.bCacheShuffle && _executor._cacheClients.Count > 1)
					{
						KeyValuePair<UnrealBuildAcceleratorCacheConfig, ICacheClient>[] array = [.. _executor._cacheClients];
						Random r = new Random();
						r.Shuffle(array);
						cacheClients = array;
					}
					else
					{
						cacheClients = _executor._cacheClients;
					}

					foreach (KeyValuePair<UnrealBuildAcceleratorCacheConfig, ICacheClient> item in cacheClients)
					{
						if (item.Key.bRequireVfs && !action.RootPaths.bUseVfs)
						{
							continue;
						}
						FetchFromCacheResult result = item.Value.FetchFromCache(rootsHandle, bucket, startInfo);
						int exitCode = 0;
						RemoveLogLineSpam(result.LogLines);
						if (result.Success && action.bForceWarningsAsError && result.LogLines.Any(x => x.Contains("): warning: ")))
						{
							result = new(true, result.LogLines.Select(x => x.Replace("): warning: ", "): error: ", StringComparison.OrdinalIgnoreCase)).ToList());
							exitCode = 1;
							++_executor._errorCount;
						}
						if (result.Success)
						{
							_executor.DecrementCacheableAction();
							return new ActionArtifactResult(result.Success, result.LogLines, exitCode);
						}
					}
					return new ActionArtifactResult(false, [], 0);
				}, cancellationToken, TaskCreationOptions.LongRunning | TaskCreationOptions.PreferFairness, TaskScheduler.Default);
			}

			public Task ActionCompleteAsync(LinkedAction action, CancellationToken cancellationToken) => Task.CompletedTask;
			public Task FlushChangesAsync(CancellationToken cancellationToken) => Task.CompletedTask;
		}

		private void ActionQueueCanceled(IStorageServer? ubaStorage)
		{
			_bIsCancelled = true;
			Server?.StopServer(); // Make sure all remove processes are returned. We can't have any callbacks after this
			_session?.CancelAll(); // Cancel all processes native side
			ubaStorage?.SaveCasTable();

			// We need the lock here since things are happening in parallel.
			lock (_sessionLock)
			{
				foreach (IUBAAgentCoordinator coordinator in _agentCoordinators)
				{
					_agentCoordinators.ForEach(ac => ac.CloseAsync().Wait(2000)); // Give coordinators some time to close (this makes coordinators like horde return resources faster)
				}
			}
		}

		public override async Task<bool> ExecuteActionsAsync(IEnumerable<LinkedAction> inputActions, Microsoft.Extensions.Logging.ILogger logger, IActionArtifactCache? actionArtifactCache)
		{
			if (!inputActions.Any())
			{
				return true;
			}

			if (inputActions.Count() < NumParallelProcesses && !UBAConfig.bForceBuildAllRemote)
			{
				UBAConfig.bDisableRemote = true;
			}

			if (UBAConfig.bDisableRemote)
			{
				UBAConfig.Zone = "local";
			}

			if (UBAConfig.bForcedRetry)
			{
				UBAConfig.bAllowRetry = true;
			}

			_cacheableActionsLeft = inputActions.Count(x => x.ArtifactMode.HasFlag(ArtifactMode.Enabled));

			await WriteActionOutputFileAsync(inputActions);

			logger = _threadedLogger;

			if (!String.IsNullOrEmpty(UBAConfig.RootDir))
			{
				_rootDirRef = DirectoryReference.FromString(UBAConfig.RootDir);
			}
			else
			{
				_rootDirRef = DirectoryReference.FromString(Environment.GetEnvironmentVariable("UBA_ROOT") ?? Environment.GetEnvironmentVariable("BOX_ROOT"));
			}

			if (_rootDirRef == null)
			{
				foreach (IUBAAgentCoordinator coordinator in _agentCoordinators)
				{
					_rootDirRef = _rootDirRef == null ? coordinator.GetUBARootDir() : _rootDirRef;
				}
			}

			if (_rootDirRef == null)
			{
				if (OperatingSystem.IsWindows())
				{
					_rootDirRef = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData)!, "Epic", "UnrealBuildAccelerator");
				}
				else
				{
					_rootDirRef = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.UserProfile)!, ".epic", "UnrealBuildAccelerator");
				}
			}

			List<Task> coordinatorInitTasks = new();
			if (!UBAConfig.bDisableRemote)
			{
				foreach (IUBAAgentCoordinator coordinator in _agentCoordinators)
				{
					// Do not await here, it will block local tasks from running
					coordinatorInitTasks.Add(coordinator.InitAsync(this));
				}
			}

			PrintConfiguration();

			DirectoryReference.CreateDirectory(_rootDirRef);

			FileReference ubaTraceFile;
			if (!String.IsNullOrEmpty(UBAConfig.TraceFile))
			{
				ubaTraceFile = new FileReference(UBAConfig.TraceFile);
			}
			else if (Unreal.IsBuildMachine())
			{
				ubaTraceFile = FileReference.Combine(Unreal.EngineProgramSavedDirectory, "AutomationTool", "Saved", "Logs", "Trace.uba");
			}
			else if (Log.OutputFile != null)
			{
				ubaTraceFile = Log.OutputFile.ChangeExtension(".uba");
			}
			else
			{
				ubaTraceFile = FileReference.Combine(Unreal.EngineProgramSavedDirectory, "UnrealBuildTool", "Trace.uba");
			}

			DirectoryReference.CreateDirectory(ubaTraceFile.Directory);
			Log.BackupLogFile(ubaTraceFile);

			IStorageServer? ubaStorage = null;

			try
			{
				if (UBAConfig.bLaunchVisualizer && OperatingSystem.IsWindows())
				{
					_ = Task.Run(LaunchVisualizer);
				}

				string arch = RuntimeInformation.ProcessArchitecture.ToString().ToLowerInvariant();
				FileReference configFile = FileReference.Combine(UbaBinariesDir, "UbaHost.toml");
				IConfig config = EpicGames.UBA.IConfig.LoadConfig(configFile.FullName);
				if (!String.IsNullOrEmpty(UBAConfig.CompressionLevel))
				{
					config.AddValue("Storage", "CompressionLevel", UBAConfig.CompressionLevel);
				}

				using EpicGames.UBA.ILogger ubaLogger = EpicGames.UBA.ILogger.CreateLogger(logger);
				using (Server = IServer.CreateServer(UBAConfig.MaxWorkers, UBAConfig.SendSize, ubaLogger, UBAConfig.bUseQuic))
				{
					_ubaLogger = ubaLogger;
					using IStorageServer ubaStorageServer = IStorageServer.CreateStorageServer(Server, ubaLogger, new StorageServerCreateInfo(_rootDirRef.FullName, ((ulong)UBAConfig.StoreCapacityGb) * 1000 * 1000 * 1000, !UBAConfig.bStoreRaw, UBAConfig.Zone));
					using ISessionServerCreateInfo serverCreateInfo = ISessionServerCreateInfo.CreateSessionServerCreateInfo(ubaStorageServer, Server, ubaLogger, new SessionServerCreateInfo(_rootDirRef.FullName, ubaTraceFile.FullName.Replace('\\', '/'), UBAConfig.bDisableCustomAlloc, false, UBAConfig.bResetCas, UBAConfig.bWriteToDisk, UBAConfig.bDetailedTrace, !UBAConfig.bDisableWaitOnMem, UBAConfig.bAllowKillOnMem, UBAConfig.bStoreObjFilesCompressed));
					{
						try
						{
							_session = ISessionServer.CreateSessionServer(serverCreateInfo);

							OnSessionCreated?.Invoke(_session);

							if (_cacheableActionsLeft != 0 && _cacheConfigs.Any())
							{
								_session.UpdateStatus(1, 1, "Cache", LogEntryType.Info, null);
								_session.UpdateStatus(1, 6, "Connecting...", LogEntryType.Info);

								foreach (UnrealBuildAcceleratorCacheConfig cacheConfig in _cacheConfigs)
								{
									ICacheClient cacheClient = ICacheClient.CreateCacheClient(_session, UBAConfig.bReportCacheMissReason, "", _cacheableActionsLeft != 0 ? GetCacheClientHint() : "");
									string[] nameAndPort = cacheConfig.CacheServer.Split(':');
									int port = 1347;
									if (nameAndPort.Length > 1)
									{
										port = Int32.Parse(nameAndPort[1]);
									}

									System.Diagnostics.Stopwatch stopwatch = System.Diagnostics.Stopwatch.StartNew();
									bool cacheSuccess = cacheClient.Connect(nameAndPort[0], port);
									long totalMs = stopwatch.ElapsedMilliseconds;
									string successText = cacheSuccess ? "Connected to" : "Failed to connect to";
									logger.LogInformation("UbaCache - {SuccessText} {Name}:{Port} ({Seconds}.{Milliseconds}s)", successText, nameAndPort[0], port, totalMs / 1000, totalMs % 1000);
									if (cacheSuccess)
									{
										_cacheClients.Add(cacheConfig, cacheClient);
										actionArtifactCache ??= new UBAActionArtifactCache(this);
									}
									else
									{
										cacheClient.Dispose();
									}
								}
								_session.UpdateStatus(1, 6, _cacheClients.Any() ? "Connected" : "Failed to connect", LogEntryType.Info);
							}

							ubaStorage = ubaStorageServer;

							if (!UBAConfig.bDisableRemote)
							{
								Server.StartServer(UBAConfig.Host, UBAConfig.Port, Crypto);
							}
							else
							{
								_session.DisableRemoteExecution();
							}

							bool success = ExecuteActionsInternal(inputActions, _session, logger, actionArtifactCache, () => ActionQueueCanceled(ubaStorage));

							if (!UBAConfig.bDisableRemote)
							{
								Server.StopServer();
							}

							if (UBAConfig.bPrintSummary)
							{
								_session.PrintSummary();
							}

							return success && !_bIsCancelled;
						}
						finally
						{
							foreach (KeyValuePair<UnrealBuildAcceleratorCacheConfig, ICacheClient> item in _cacheClients)
							{
								item.Value.Dispose();
							}
							_cacheClients.Clear();
							lock (_sessionLock)
							{
								_agentCoordinators.ForEach(ac => ac.Done());
								_session?.Dispose();
								_session = null;
							}
						}
					}
				}
			}
			finally
			{
				foreach (IUBAAgentCoordinator coordinator in _agentCoordinators)
				{
					await coordinator.CloseAsync();
				}
				await _threadedLogger.FinishAsync();
				_session = null;
			}
		}

		[SupportedOSPlatform("windows")]
		static void LaunchVisualizer()
		{
			FileReference visaulizerPath = FileReference.Combine(Unreal.EngineDirectory, "Binaries", "Win64", "UnrealBuildAccelerator", RuntimeInformation.ProcessArchitecture.ToString(), "UbaVisualizer.exe");
			FileReference tempPath = FileReference.Combine(new DirectoryReference(System.IO.Path.GetTempPath()), visaulizerPath.GetFileName());
			if (!FileReference.Exists(visaulizerPath))
			{
				return;
			}

			try
			{
				// Check if a listening visualizer is already running
				foreach (System.Diagnostics.Process process in System.Diagnostics.Process.GetProcessesByName(visaulizerPath.GetFileNameWithoutAnyExtensions()))
				{
					using ManagementObjectSearcher searcher = new($"SELECT CommandLine FROM Win32_Process WHERE ProcessId = {process.Id}");
					using ManagementObjectCollection objects = searcher.Get();
					string args = objects.Cast<ManagementBaseObject>().SingleOrDefault()?["CommandLine"]?.ToString() ?? "";
					if (args.Contains("-listen", StringComparison.OrdinalIgnoreCase))
					{
						return;
					}
				}
				if (!FileReference.Exists(tempPath) || tempPath.ToFileInfo().LastWriteTime < visaulizerPath.ToFileInfo().LastWriteTime)
				{
					FileReference.Copy(visaulizerPath, tempPath, true);
				}
				if (FileReference.Exists(tempPath))
				{
					System.Diagnostics.ProcessStartInfo psi = new(BuildHostPlatform.Current.Shell.FullName, $" /C start \"\" \"{tempPath.FullName}\" -listen -nocopy")
					{
						WorkingDirectory = System.IO.Path.GetTempPath(),
						WindowStyle = System.Diagnostics.ProcessWindowStyle.Hidden,
						UseShellExecute = true,
					};
					System.Diagnostics.Process.Start(psi);
				}
			}
			catch (Exception)
			{
			}
		}

		public override bool VerifyOutputs => UBAConfig.bWriteToDisk;

		/// <summary>
		/// Executes the provided actions
		/// </summary>
		/// <returns>True if all the tasks successfully executed, or false if any of them failed.</returns>
		bool ExecuteActionsInternal(IEnumerable<LinkedAction> inputActions, ISessionServer session, Microsoft.Extensions.Logging.ILogger logger, IActionArtifactCache? actionArtifactCache, System.Action onCancel)
		{
			_ubaStartTimeUtc = DateTime.UtcNow;
			using ImmediateActionQueue queue = CreateActionQueue(inputActions, actionArtifactCache, UBAConfig.CacheMaxWorkers, logger);
			int actionLimit = Math.Min(NumParallelProcesses, queue.TotalActions);
			queue.CreateAutomaticRunner(action => RunActionLocal(queue, action), bUseActionWeights, actionLimit, NumParallelProcesses);
			ImmediateActionQueueRunner remoteRunner = queue.CreateManualRunner(action => RunActionRemote(queue, action, false));
			ImmediateActionQueueRunner crossArchitectureRemoteRunner = queue.CreateManualRunner(action => RunActionRemote(queue, action, true));
			queue.CancellationToken.Register(onCancel);

			if (actionArtifactCache != null)
			{
				queue.OnArtifactsRead += (action) => { UpdateCacheProgress(queue); UpdateProgress(queue); };
				queue.OnArtifactsMiss += (action) => UpdateCacheProgress(queue);
			}

			UpdateProgress(queue);

			// Start the queue
			queue.Start();

			// Handle process available from remote
			session!.RemoteProcessSlotAvailable += (sender, args) =>
			{
				ImmediateActionQueueRunner runner = args.IsCrossArchitecture ? crossArchitectureRemoteRunner : remoteRunner;
				if (!queue.TryStartOneAction(runner) && (_bIsRemoteActionsAllowed && !UBAConfig.bForceBuildAllRemote))
				{
					uint count = ActionsLeftThatCanRunRemotely(queue);

					// We didn't find an action to start, let's check how many queued items are left.. if there are less than NumParallelProcesses, then disconnect
					if (count <= NumParallelProcesses)
					{
						_bIsRemoteActionsAllowed = false;
						_agentCoordinators.ForEach(ac => ac.Stop());
						session!.DisableRemoteExecution();
					}
					else
					{
						// Tell UBA max number of remote processes left. Providing this information makes it possible for UBA to start disconnecting clients
						session!.SetMaxRemoteProcessCount(count);
					}
				}
			};

			session!.RemoteProcessReturned += (sender, args) =>
			{
				args.Process.Cancel(true);
				LinkedAction action = (LinkedAction)args.Process.UserData!;
				//logger.LogInformation("REQUEUING " + action.ProducedItems.FirstOrDefault()!.Name);
				queue.RequeueAction(action);
			};

			// Add all actions we can add
			queue.StartManyActions();

			try
			{
				uint statusUpdateCounter = 10;
				foreach (IUBAAgentCoordinator coordinator in _agentCoordinators)
				{
					uint statusUpdateIndex = statusUpdateCounter;
					coordinator.Start(queue, CanRunRemotely, (sr, sc, st, t, sl) => UpdateStatus(statusUpdateIndex + sr, sc, st, t, sl));
					statusUpdateCounter += 10;
				}

				bool res = queue.RunTillDone().Result; // Using inline wait to avoid possible thread switch

				queue.GetActionResultCounts(out int totalActions, out int succeededActions, out int failedActions, out int cacheHitActions, out int cacheMissActions);
				telemetryEvent = new TelemetryExecutorUBAEvent(Name, _ubaStartTimeUtc, res, totalActions, succeededActions, failedActions, cacheHitActions, cacheMissActions,
					_localProcessedActions, _remoteProcessedActions,
					_localRetryActions.Count, _forcedRetryActions.Count,
					!UBAConfig.bDisableRemote ? _agentCoordinators.Count : 0, !UBAConfig.bDisableRemote ? _successfulCoordinatorConnections : 0, !UBAConfig.bDisableRemote ? _failedCoordinatorConnections : 0,
					!UBAConfig.bDisableRemote ? _ubaDurationWaitingForRemote : TimeSpan.Zero,
					!UBAConfig.bDisableRemote || _agentCoordinators.Count == 0 ? "Local" : _remoteConnectionMode,
					DateTime.UtcNow);

				return res;
			}
			finally
			{
				if (_bIsRemoteActionsAllowed)
				{
					_agentCoordinators.ForEach(ac => ac.Stop());
				}
			}
		}

		/// <summary>
		/// Determine if an action must be run locally and with no detouring
		/// </summary>
		/// <param name="action">The action to check</param>
		/// <returns>If this action must be local, non-detoured</returns>
		bool ForceLocalNoDetour(LinkedAction action)
		{
			if (!UBAConfig.bAllowDetour)
			{
				return true;
			}

			if (!OperatingSystem.IsMacOS()) // Below code is slow, so early out
			{
				return false;
			}
			// Don't let Mac run shell commands through Uba as interposing dylibs into
			// the shell results in dyld errors about no matching architecture.
			// The shell is used to run various commands during a build like copy/ditto.
			// So for these actions we need to make sure UBA is not used.
			bool bIsShellAction = action.CommandPath == BuildHostPlatform.Current.Shell;
			return bIsShellAction;
		}

		/// <summary>
		/// Determine if an action is able to be run remotely
		/// </summary>
		/// <param name="action">The action to check</param>
		/// <returns>If this action can be run remotely</returns>
		bool CanRunRemotely(LinkedAction action) =>
			action.bCanExecuteInUBA &&
			action.bCanExecuteRemotely &&
			(UBAConfig.bLinkRemote || action.ActionType != ActionType.Link) &&
			!_localRetryActions.ContainsKey(action) &&
			!_forcedRetryActions.ContainsKey(action) &&
			!ForceLocalNoDetour(action);

		ProcessStartInfo GetActionStartInfo(LinkedAction action)
		{
			string description = action.StatusDescription;
			if (!String.IsNullOrEmpty(action.CommandDescription))
				description = $"{action.StatusDescription} ({action.CommandDescription})";

			// Let pch compile at higher priority
			System.Diagnostics.ProcessPriorityClass priority = ProcessPriority;
			if (ProcessPriority == System.Diagnostics.ProcessPriorityClass.BelowNormal)
			{
				if (action.StatusDescription.Contains("PCH."))
				{
					priority = System.Diagnostics.ProcessPriorityClass.Normal;
				}
			}
				
			ProcessStartInfo startInfo = new()
			{
				Application = action.CommandPath.FullName,
				WorkingDirectory = action.WorkingDirectory.FullName,
				Arguments = action.CommandArguments,
				Priority = priority,
				UserData = action,
				Description = description,
				Configuration = action.bIsGCCCompiler ? EpicGames.UBA.ProcessStartInfo.CommonProcessConfigs.CompileClang : EpicGames.UBA.ProcessStartInfo.CommonProcessConfigs.CompileMsvc,
				LogFile = UBAConfig.bLogEnabled ? (action.Inner.ProducedItems.First().Location.GetFileName() + ".log") : null,
			};

			return startInfo;
		}

		void DecrementCacheableAction()
		{
			if (Interlocked.Decrement(ref _cacheableActionsLeft) == 0)
			{
				foreach (KeyValuePair<UnrealBuildAcceleratorCacheConfig, ICacheClient> item in _cacheClients)
				{
					item.Value.Dispose();
				}
				_cacheClients.Clear();
			}
		}

		string GetCacheClientHint()
		{
			// This is extra information sent to cache server when fetching stuff. This way it will be possible to backtrack from cache server log to who connected
			// These environment variables are set when UBT is running on a horde machine.
			string? jobId = Environment.GetEnvironmentVariable("UE_HORDE_JOBID");
			if (jobId == null)
			{
				return Environment.MachineName;
			}
			string hordeUrl = Environment.GetEnvironmentVariable("UE_HORDE_URL")??"";
			string stepId = Environment.GetEnvironmentVariable("UE_HORDE_STEPID")??"unknown";
			string streamId = Environment.GetEnvironmentVariable("UE_HORDE_STREAMID")??"unknown-stream";
			return $"{hordeUrl}job/{jobId}?step={stepId} ({streamId})";
		}

		HashSet<uint> _usedBuckets = new();

		uint GetCacheBucket(LinkedAction action)
		{
			uint bucket = 0;
			if (action.CacheBucket != 0)
			{
				bucket = action.CacheBucket;
			}
			else if (action.Target == null)
			{
				bucket = 0;
			}
			else
			{
				using (Blake3.Hasher hasher = Blake3.Hasher.New())
				{
					// Use platform and config to chose bucket since there will never be any cache hits between platforms or configs
					hasher.Update(System.Text.Encoding.UTF8.GetBytes(action.Target.Platform.ToString()));
					hasher.Update(new byte[] { (byte)action.Target.Configuration });

					// Absolute path is set for actions that uses pch.
					// And since pch contains absolute paths we unfortunately can't share cache data between machines that have different paths
					if (action.ArtifactMode.HasFlag(ArtifactMode.AbsolutePath))
					{
						foreach ((uint id, DirectoryReference vfs, DirectoryReference local) in action.RootPaths)
						{
							hasher.Update(System.Text.Encoding.UTF8.GetBytes(local.FullName));
						}
					}

					bucket = (uint)IoHash.FromBlake3(hasher).GetHashCode();
				}
			}

			lock (_usedBuckets)
			{
				if (_usedBuckets.Add(bucket))
				{
					_threadedLogger.LogDebug($"Cache bucket {bucket} used. Action: {action.CommandDescription} {action.StatusDescription} ({String.Join(" + ", action.GroupNames)})");
				}
			}

			return bucket;
		}

		ulong GetActionRootsHandle(LinkedAction action)
		{
			using MemoryStream outputsMemory = new(1024);
			using (BinaryWriter writer = new(outputsMemory, System.Text.Encoding.UTF8, true))
			{
				bool useVfs = action.RootPaths.bUseVfs;
				foreach ((uint id, DirectoryReference vfs, DirectoryReference local) in action.RootPaths)
				{
					writer.Write((byte)id);
					if (useVfs)
					{
						writer.Write(vfs.FullName);
					}
					else
					{
						writer.Write("");
					}
					writer.Write(local.FullName);
				}
			}
			return _session!.RegisterRoots(outputsMemory.GetBuffer(), (uint)outputsMemory.Position);
		}

		public class DepsFile
		{
			public class DepsData
			{
				public string? Source { get; init; }
				public string? PCH { get; init; }
				public SortedSet<string>? Includes { get; init; }
			}
			public string? Version { get; init; }
			public DepsData? Data { get; init; }
		}

		bool WriteToCache(LinkedAction action, IProcess process)
		{
			if (process.ExitCode != 0 || !action.ArtifactMode.HasFlag(ArtifactMode.Enabled) || !_cacheClients.Any(x => x.Key.CanWrite))
			{
				return true;
			}

			// If there are no outputs there is nothing to cache
			if (!action.ProducedItems.Any())
			{
				return true;
			}

			// Collect all inputs for action
			// We use prerequisite items plus what we find in dependency list file if it exists.

			using MemoryStream inputsMemory = new(1024);
			using (BinaryWriter writer = new(inputsMemory, System.Text.Encoding.UTF8, true))
			{
				writer.Write(action.CommandPath.FullName);

				foreach (FileItem f in action.PrerequisiteItems)
				{
					if (f.HasExtension(".lib")) // It seems like .lib files can change without dependencies relink
					{
						continue;
					}

					writer.Write(f.FullName);
				}

				if (action.DependencyListFile != null)
				{
					try
					{
						CppDependencyCache.DependencyInfo info = CppDependencyCache.ReadDependencyInfo(action.DependencyListFile);
						foreach (FileItem f in info.Files)
						{
							writer.Write(f.FullName);
						}
					}
					catch (Exception)
					{
						_threadedLogger.LogInformation("{Description} {StatusDescription}: Failed to read dependency file {DependencyFile} for WriteCache", action.CommandDescription, action.StatusDescription, action.DependencyListFile);
						return false;
					}
				}
			}

			// Collect all outputs for action

			using MemoryStream outputsMemory = new(1024);
			using (BinaryWriter writer = new(outputsMemory, System.Text.Encoding.UTF8, true))
			{
				foreach (FileItem f in action.ProducedItems)
				{
					writer.Write(f.FullName);
				}
			}

			uint bucket = GetCacheBucket(action);

			bool success = true;
			foreach (KeyValuePair<UnrealBuildAcceleratorCacheConfig, ICacheClient> item in _cacheClients.Where(x => x.Key.CanWrite))
			{
				if (item.Key.bRequireVfs && !action.RootPaths.bUseVfs)
				{
					continue;
				}

				success = item.Value.WriteToCache(bucket, process, inputsMemory.GetBuffer(), (uint)inputsMemory.Position, outputsMemory.GetBuffer(), (uint)outputsMemory.Position) || success;
			}

			DecrementCacheableAction();

			return success;
		}

		Func<Task>? RunActionLocal(ImmediateActionQueue queue, LinkedAction action)
		{
			if (UBAConfig.bForceBuildAllRemote && CanRunRemotely(action))
			{
				return null;
			}

			return () =>
			{
				if (_bIsCancelled)
				{
					HandleActionCancelled(queue, null, action);
					return Task.CompletedTask;
				}
				bool enableDetour = !ForceLocalNoDetour(action) && action.bCanExecuteInUBA && !_forcedRetryActions.ContainsKey(action);

				ProcessStartInfo startInfo = GetActionStartInfo(action);
				startInfo.RootsHandle = GetActionRootsHandle(action);

				using (IProcess process = _session!.RunProcess(startInfo, false, null, enableDetour))
				{
					Interlocked.Add(ref _localProcessedActions, 1);
					if (!_bIsCancelled && (UBAConfig.bAllowRetry && (process.ExitCode != 0 && UBAConfig.bForcedRetry || (process.ExitCode >= 9000 && process.ExitCode < 10000))))
					{
						if (UBAConfig.bStoreObjFilesCompressed)
						{
							_threadedLogger.LogInformation("{Description} {StatusDescription}: Exited with error code {ExitCode}. This action will retry", action.CommandDescription, action.StatusDescription, process.ExitCode);
						}
						else
						{
							_threadedLogger.LogInformation("{Description} {StatusDescription}: Exited with error code {ExitCode}. This action will retry without UBA", action.CommandDescription, action.StatusDescription, process.ExitCode);
							_forcedRetryActions.AddOrUpdate(action, false, (k, v) => false);
						}
						queue.RequeueAction(action);
						return Task.CompletedTask;
					}

					// If not detoured we manually have to report created files
					if (!enableDetour && process.ExitCode == 0)
					{
						_session!.RegisterNewFiles(action.ProducedItems.Where(x => FileReference.Exists(x.Location)).Select(x => x.FullName).ToArray());
					}

					if (enableDetour)
					{
						WriteToCache(action, process);
					}

					TimeSpan processorTime = process.TotalProcessorTime;
					TimeSpan executionTime = process.TotalWallTime;
					List<string> logLines = process.LogLines;
					RemoveLogLineSpam(logLines);
					string? additionalDescription = !enableDetour ? "(UBA disabled)" : null;
					ActionFinished(queue, new ExecuteResults(logLines, process.ExitCode, executionTime, processorTime, additionalDescription), action, process);
				}
				return Task.CompletedTask;
			};
		}

		Func<Task>? RunActionRemote(ImmediateActionQueue queue, LinkedAction action, bool isCrossArchitecture)
		{
			if (!CanRunRemotely(action))
			{
				return null;
			}

			if (isCrossArchitecture && !action.bCanExecuteInUBACrossArchitecture)
			{
				return null;
			}

			return () =>
			{
				if (_ubaDurationWaitingForRemote == TimeSpan.Zero)
				{
					_ubaDurationWaitingForRemote = DateTime.UtcNow - _ubaStartTimeUtc;
				}
				if (_bIsCancelled)
				{
					HandleActionCancelled(queue, null, action);
					return Task.CompletedTask;
				}

				uint knownInputsCount = 0;
				byte[]? knownInputs = null;
				if (UBAConfig.bUseKnownInputs)
				{
					int sizeOfChar = System.OperatingSystem.IsWindows() ? 2 : 1;

					int byteCount = 0;
					foreach (FileItem item in action.PrerequisiteItems)
					{
						byteCount += (item.FullName.Length + 1) * sizeOfChar;
						++knownInputsCount;
					}

					knownInputs = new byte[byteCount + sizeOfChar];

					int byteOffset = 0;
					foreach (FileItem item in action.PrerequisiteItems)
					{
						string str = item.FullName;
						int strBytes = str.Length * sizeOfChar;
						if (sizeOfChar == 1) // Unmanaged size uses ascii
						{
							System.Buffer.BlockCopy(System.Text.Encoding.ASCII.GetBytes(str.ToCharArray()), 0, knownInputs, byteOffset, strBytes);
						}
						else
						{
							System.Buffer.BlockCopy(str.ToCharArray(), 0, knownInputs, byteOffset, strBytes);
						}

						byteOffset += strBytes + sizeOfChar;
					}
				}

				ProcessStartInfo startInfo = GetActionStartInfo(action);
				startInfo.RootsHandle = GetActionRootsHandle(action);

				_session!.RunProcessRemote(startInfo, (s, e) =>
				{
					if (e.ExitCode == 99999) // Process was cancelled by executor
					{
						return;
					}

					Interlocked.Add(ref _remoteProcessedActions, 1);
					if (e.ExitCode != 0 && !e.LogLines.Any())
					{
						RemoteActionFailedNoOutput(queue, action, e.ExitCode, e.ExecutingHost ?? "Unknown");
						return;
					}
					else if ((uint)e.ExitCode == 0xC0000005)
					{
						RemoteActionFailedCrash(queue, action, e.ExitCode, e.ExecutingHost ?? "Unknown", "Access violation", e.LogLines);
						return;
					}
					else if ((uint)e.ExitCode == 0xC0000409)
					{
						RemoteActionFailedCrash(queue, action, e.ExitCode, e.ExecutingHost ?? "Unknown", "Stack buffer overflow", e.LogLines);
						return;
					}
					else if ((uint)e.ExitCode == 0xC0000602)
					{
						RemoteActionFailedCrash(queue, action, e.ExitCode, e.ExecutingHost ?? "Unknown", "Fail Fast Exception", e.LogLines);
						return;
					}
					else if (e.ExitCode != 0 && e.LogLines.Any(x => x.Contains(" C1001: ", StringComparison.Ordinal)))
					{
						RemoteActionFailedCrash(queue, action, e.ExitCode, e.ExecutingHost ?? "Unknown", "C1001", e.LogLines);
						return;
					}
					else if (e.ExitCode >= 9000 && e.ExitCode < 10000)
					{
						RemoteActionFailedCrash(queue, action, e.ExitCode, e.ExecutingHost ?? "Unknown", "UBA error", e.LogLines);
						return;
					}
					else if (e.ExitCode != 0 && UBAConfig.bForcedRetryRemote)
					{
						RemoteActionFailedCrash(queue, action, e.ExitCode, e.ExecutingHost ?? "Unknown", "Force local retry", e.LogLines);
						return;
					}

					IProcess process = (IProcess)s;

					WriteToCache(action, process);

					string additionalDescription = $"[RemoteExecutor: {e.ExecutingHost}]";
					TimeSpan processorTime = e.TotalProcessorTime;
					TimeSpan executionTime = e.TotalWallTime;
					List<string> logLines = e.LogLines;
					RemoveLogLineSpam(logLines);
					ActionFinished(queue, new ExecuteResults(logLines, e.ExitCode, executionTime, processorTime, additionalDescription), action, process);
				}, action.Weight, knownInputs, knownInputsCount, action.bCanExecuteInUBACrossArchitecture);
				return Task.CompletedTask;
			};
		}

		uint ActionsLeftThatCanRunRemotely(ImmediateActionQueue queue)
		{
			lock (_actionsChangedLock)
			{
				if (_bActionsChanged)
				{
					_actionsQueuedThatCanRunRemotely = queue.GetQueuedActionsCount(CanRunRemotely);
					_bActionsChanged = false;
				}
				return _actionsQueuedThatCanRunRemotely;
			}
		}

		protected void HandleActionCancelled(ImmediateActionQueue queue, ExecuteResults? results, LinkedAction action)
		{
			ExecuteResults cancelResults = new(results?.LogLines ?? new(), Int32.MaxValue, results?.ExecutionTime ?? TimeSpan.Zero, results?.ProcessorTime ?? TimeSpan.Zero, results?.AdditionalDescription);
			queue.OnActionCompleted(action, false, cancelResults);
		}

		protected void ActionFinished(ImmediateActionQueue queue, ExecuteResults results, LinkedAction action, IProcess? process = null)
		{
			if (_bIsCancelled)
			{
				HandleActionCancelled(queue, results, action);
				return;
			}

			bool success = results.ExitCode == 0;

			if (results.ExitCode == 0 && action.bForceWarningsAsError && results.LogLines.Any(x => x.Contains("): warning: ")))
			{
				results = new(results.LogLines.Select(x => x.Replace("): warning: ", "): error: ", StringComparison.OrdinalIgnoreCase)).ToList(), 1, results.ExecutionTime, results.ProcessorTime, results.AdditionalDescription);
				success = false;
			}

			queue.OnActionCompleted(action, success, results);

			if (!success)
			{
				++_errorCount;
			}
			UpdateProgress(queue);

			lock (_actionsChangedLock)
			{
				_bActionsChanged = true;
			}
		}
		void UpdateProgress(ImmediateActionQueue queue)
		{
			int completedActions = queue.CompletedActions;
			int totalActions = queue.TotalActions;
			int percent = completedActions * 100 / totalActions;
			_session!.UpdateProgress((uint)totalActions, (uint)completedActions, _errorCount);
		}
		void UpdateCacheProgress(ImmediateActionQueue queue)
		{
			_session!.UpdateStatus(1, 6, $"Hits {queue.CacheHitActions} Misses {queue.CacheMissActions}", LogEntryType.Info);
		}
		void RemoteActionFailedNoOutput(ImmediateActionQueue queue, LinkedAction action, int exitCode, string executingHost)
		{
			if (_bIsCancelled)
			{
				HandleActionCancelled(queue, null, action);
				return;
			}

			_threadedLogger.LogInformation("{Description} {StatusDescription} [RemoteExecutor: {ExecutingHost}]: Exited with error code {ExitCode} with no output. This action will retry locally", action.CommandDescription, action.StatusDescription, executingHost, exitCode);
			_localRetryActions.AddOrUpdate(action, false, (k, v) => false);
			queue.RequeueAction(action);

			lock (_actionsChangedLock)
			{
				_bActionsChanged = true;
			}
		}

		void RemoteActionFailedCrash(ImmediateActionQueue queue, LinkedAction action, int exitCode, string executingHost, string error, List<string> logLines)
		{
			if (_bIsCancelled)
			{
				HandleActionCancelled(queue, null, action);
				return;
			}

			_threadedLogger.LogInformation("{Description} {StatusDescription} [RemoteExecutor: {ExecutingHost}]: Exited with error code {ExitCode} ({Error}). This action will retry locally", action.CommandDescription, action.StatusDescription, executingHost, exitCode, error);
			foreach (string line in logLines)
			{
				_threadedLogger.LogInformation(line);
			}

			_localRetryActions.AddOrUpdate(action, false, (k, v) => false);
			queue.RequeueAction(action);

			lock (_actionsChangedLock)
			{
				_bActionsChanged = true;
			}
		}
	}
}
