// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildBase;
using UnrealBuildTool;
using AutomationScripts;
using EpicGames.Core;
using System.Linq;
using Microsoft.Extensions.Logging;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;

using Microsoft.VisualStudio.OLE.Interop;
using System.Runtime.Versioning;

namespace AutomationTool
{
	class ProcessDebugger
	{
		[SupportedOSPlatform("windows")]
		[DllImport("ole32.dll")]
		public static extern int CreateBindCtx(int reserved, out IBindCtx ppbc);

		[SupportedOSPlatform("windows")]
		[DllImport("ole32.dll")]
		public static extern int GetRunningObjectTable(int reserved, out IRunningObjectTable prot);

		public static void DebugProcess(int ProcessID, BuildCommand Command, ProjectParams Params)
		{
			if (OperatingSystem.IsWindows())
			{
				if (CommandUtils.ParseParam(Command.Params, "Rider"))
				{
					bool bUsesUProject = CommandUtils.ParseParam(Command.Params,"RiderUProject");
					DebugProcessWithRider(ProcessID, Params.RawProjectPath, bUsesUProject);
				}
				else
				{
					DebugProcessWithVisualStudio(ProcessID);
				}
			}
		}

		public static void DebugProcessWithRider(int ProcessId, FileReference ProjectPath, bool bUsesUProject)
		{
			// Rider allows to attach to any process using commandline arguments. If you already have a rider instance opened with the provided project path, that
			// running instance will be used instead of starting a new one.

			string FinalSolutionOrProjectPath = bUsesUProject ? ProjectPath.ToString() : null;
			if (!bUsesUProject)
			{
				DirectoryReference CurrentDirectory = ProjectPath.Directory;
				do
				{
					string PathToEvaluate = CurrentDirectory + "\\UE5.sln";
					if (File.Exists(PathToEvaluate))
					{
						FinalSolutionOrProjectPath = PathToEvaluate;
						break;
					}

					CurrentDirectory = CurrentDirectory.ParentDirectory;
				} 
				while(CurrentDirectory != null && !CurrentDirectory.IsRootDirectory());
			}

			if (FinalSolutionOrProjectPath == null)
			{
				Log.Logger.LogInformation($"Failed to find a project solution file path. We cannot attach using Rider.");
				return;
			}

			string RiderPath = Environment.GetEnvironmentVariable("RIDERINSTALLDIR", EnvironmentVariableTarget.Machine);

			if (RiderPath == null)
			{
				Log.Logger.LogError($"Failed to find Rider's binary path. Is Rider executable location added to the RIDERINSTALLDIR system environment variable?.");
				return;
			}

			var RiderProcess = new Process();
			
			RiderProcess.StartInfo.FileName = RiderPath + "/Rider64.exe";
			RiderProcess.StartInfo.UseShellExecute = false;
			RiderProcess.StartInfo.Arguments = $"attach-to-process Native {ProcessId} {FinalSolutionOrProjectPath}";
			if (!RiderProcess.Start())
			{
				Log.Logger.LogError($"Failed to start or connect to Rider. We cannot attach to the selected process.");
			}
		}
		
		public static void DebugProcessWithVisualStudio(int ProcessID)
		{
			if (OperatingSystem.IsWindows())
			{
				EnvDTE._DTE visualStudioInstance = GetVisualStudioInstance();

				if (visualStudioInstance != null)
				{
					AttachVisualStudioToPID(visualStudioInstance, ProcessID);
				}
				else
				{
					Log.Logger.LogInformation($"Failed to find a Visual Studio Instance.");
				}
			}
		}

		[SupportedOSPlatform("windows")]
		private static EnvDTE._DTE GetVisualStudioInstance()
		{
			EnvDTE._DTE visualStudioInstance = null;

			uint numFetched = 0;
			IRunningObjectTable runningObjectTable = null;
			IEnumMoniker monikerEnumerator;
			IMoniker[] monikers = new IMoniker[1];

			GetRunningObjectTable(0, out runningObjectTable);
			runningObjectTable.EnumRunning(out monikerEnumerator);
			monikerEnumerator.Reset();

			while (monikerEnumerator.Next(1, monikers, out numFetched) == 0)
			{
				IBindCtx ctx;
				CreateBindCtx(0, out ctx);

				string runningObjectName;
				monikers[0].GetDisplayName(ctx, null, out runningObjectName);

				object runningObjectVal;
				runningObjectTable.GetObject(monikers[0], out runningObjectVal);

				if (!(runningObjectVal is EnvDTE._DTE) || !runningObjectName.StartsWith("!VisualStudio"))
				{
					continue;
				}

				return (EnvDTE._DTE)runningObjectVal;
			}

			return visualStudioInstance;
		}

		[SupportedOSPlatform("windows")]
		private static void AttachVisualStudioToPID(EnvDTE._DTE visualStudioInstance, int processID)
		{
			int retryCount = 0;
			while (true)
			{
				try
				{
					var processToAttachTo = visualStudioInstance.Debugger.LocalProcesses.Cast<EnvDTE.Process>().FirstOrDefault(process => process.ProcessID == processID);

					if (processToAttachTo == null)
					{
						Log.Logger.LogInformation("Failed to find running Process matching provided Process Name {0}", processID);
						continue;
					}
					else
					{
						processToAttachTo.Attach();
					}

					break;
				}
				catch (COMException e)
				{
					if ((uint)e.ErrorCode == 0x8001010a || (uint)e.ErrorCode == 0x80010001)
					{
						if (++retryCount < 15)
						{
							Log.Logger.LogInformation("Attach Debugger - Got RPC Retry Later exception. Will try again ");
							System.Threading.Thread.Sleep(20);
							continue;
						}
					}
					Log.Logger.LogInformation("Failed to attach debugger. COMException was thrown: " + e.ToString());
					break;
				}
				catch (Exception e)
				{
					Log.Logger.LogInformation("Failed to attach debugger. Exception was thrown: " + e.ToString());
					break;
				}
			}
		}

		[SupportedOSPlatform("windows")]
		private static bool IsDebuggerAttached(EnvDTE._DTE VisualStudio, string processID)
		{
			bool DebuggerAttached = false;

			if (VisualStudio.Debugger.DebuggedProcesses.Count != 0)
			{
				foreach (EnvDTE.Process debuggedProcess in VisualStudio.Debugger.DebuggedProcesses)
				{
					if (debuggedProcess.Name.Contains(processID))
					{
						DebuggerAttached = true;
						break;
					}
				}
			}
			return DebuggerAttached;
		}

		[SupportedOSPlatform("windows")]
		private static bool IsDebuggerAttachedToPID(EnvDTE._DTE VisualStudio, int processID)
		{
			bool DebuggerAttached = false;

			if (VisualStudio.Debugger.DebuggedProcesses.Count != 0)
			{
				foreach (EnvDTE.Process debuggedProcess in VisualStudio.Debugger.DebuggedProcesses)
				{
					if (debuggedProcess.ProcessID == processID)
					{
						DebuggerAttached = true;
						break;
					}
				}
			}
			return DebuggerAttached;
		}
	}

	// Mirrors FMultiServerDefinition from the MultiServerReplication plugin
	class MultiServerDefinition
	{
		public string LocalId = "0";
		public int ListenPort = 0;

		public override string ToString()
		{
			return "MultiServerDefinition: LocalId \"" + LocalId + "\", ListenPort " + ListenPort;
		}
	}

	// MultiServerDefinition plus additional information only used by this command
	class MultiServerInstanceInfo
	{
		public MultiServerDefinition Definition = null;
		public int GameListenPort = 0;

		public MultiServerInstanceInfo(MultiServerDefinition definition)
		{
			Definition = definition;
		}
	}

	[Help("Launches multiple server processes for a project using the MultiServerReplication plugin.")]
	[Help("project=<project>", "Project to open. Will search current path and paths in ueprojectdirs.")]
	[Help("map=<MapName>", "Map to load on startup.")]
	public class LaunchMultiServer : BuildCommand, IProjectParamsHelpers
	{
		private enum ProcessLaunchType
		{
			Server,
			DirectClient,
			ProxyServer,
			ProxyClient
		}

		public override ExitCode Execute()
		{
			Logger.LogInformation("********** RUN MULTISERVER COMMAND STARTED **********");
			var StartTime = DateTime.UtcNow;

			// Parse server configuration from ini files
			ConfigHierarchy ProjectGameConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, DirectoryReference.FromFile(ProjectPath), UnrealTargetPlatform.Win64);

			const String ServerDefConfigSection = "/Script/MultiServerConfiguration.MultiServerSettings";
			const String ProxyConfigSection = "/Script/MultiServerConfiguration.Proxy";

			ProjectGameConfig.TryGetValuesGeneric(ServerDefConfigSection, "ServerDefinitions", out MultiServerDefinition[] ConfigServerDefs);

			if (ConfigServerDefs != null && ConfigServerDefs.Length > 0)
			{
				// Use a dictionary to store ServerDefs by unqiue ID
				Dictionary<string, MultiServerInstanceInfo> UniqueServerInfos = new Dictionary<string, MultiServerInstanceInfo>();

				var GameListenPort = 7777;

				// Check for duplicates and build peer list
				string PeerList = "";
				for (int i = 0; i < ConfigServerDefs.Length; i++)
				{
					if (UniqueServerInfos.TryAdd(ConfigServerDefs[i].LocalId, new MultiServerInstanceInfo(ConfigServerDefs[i])))
					{
						PeerList += String.Format("127.0.0.1:{0}{1}", ConfigServerDefs[i].ListenPort, i < (ConfigServerDefs.Length - 1) ? "," : "");
						UniqueServerInfos[ConfigServerDefs[i].LocalId].GameListenPort = GameListenPort++;
					}
					else
					{
						Logger.LogInformation("MultiServer server definition with duplicate ID {0}, ignoring duplicate entry {1}", ConfigServerDefs[i].LocalId, ConfigServerDefs[i]);
					}
				}

				StartProcessesForServerInfos(UniqueServerInfos, ProcessLaunchType.Server, " -MultiServerPeers=" + PeerList);

				if (ParseParam("client"))
				{
					string[] ClientValues = ParseParamValue("client").Split(',');

					foreach (string ClientValue in ClientValues)
					{
						if (ClientValue == "direct")
						{
							Logger.LogInformation("Starting direct client instances connecting to MultiServer instances");
							StartProcessesForServerInfos(UniqueServerInfos, ProcessLaunchType.DirectClient, "");
						}
						else if (ClientValue == "proxy")
						{
							int ClientCountTotal = 1;
							if (ParseParam("proxyclientcount"))
							{
								string ClientCountStr = ParseParamValue("proxyclientcount", null);
								Int32.TryParse(ClientCountStr, out ClientCountTotal);
							}

							int BotCountTotal = 0;
							if (ParseParam("proxybotcount"))
							{
								string ProxyBotCountStr = ParseParamValue("proxybotcount", null);
								Int32.TryParse(ProxyBotCountStr, out BotCountTotal);
							}

							int ProxyServerCount = 1;
							if (ParseParam("proxyservercount"))
							{
								string ProxyServerCountStr = ParseParamValue("proxyservercount", null);
								Int32.TryParse(ProxyServerCountStr, out ProxyServerCount);
							}

							var FirstProxyListeningPort = 8000;
							ProjectGameConfig.TryGetValue(ProxyConfigSection, "ListenPort", out FirstProxyListeningPort);

							int TotalClientCount = 0;
							int TotalBotCount = 0;

							var ProjectName = ProjectUtils.GetShortProjectName(ProjectPath);

							var ProxyListeningPort = FirstProxyListeningPort;
							for (int ProxyServerIndex = 0; ProxyServerIndex < ProxyServerCount; ++ProxyServerIndex)
							{
								Logger.LogInformation("Starting proxy server on port {0} connected to MultiServer instances", ProxyListeningPort);
								{
									var ProxyServerAdditionalParams = String.Format(" -port={0} -ConsoleTitle=\"{1} Proxy Server {2}\" -SessionName=\"{1} Proxy Server {2}\" -log={1}ProxyServer-{2}.log ", ProxyListeningPort, ProjectName, ProxyServerIndex + 1);
									StartProcessesForServerInfos(UniqueServerInfos, ProcessLaunchType.ProxyServer, ProxyServerAdditionalParams);
								}

								int NumClientsPerServer = ClientCountTotal / ProxyServerCount;
								int NumExtraClients = ClientCountTotal % ProxyServerCount;
								int NumClientsToSpawn = NumClientsPerServer + (ProxyServerIndex < NumExtraClients ? 1 : 0);
								for (int ClientIndex = 0; ClientIndex < NumClientsToSpawn; ClientIndex++)
								{
									++TotalClientCount;
									var ProxyClientAdditionalParams = String.Format("127.0.0.1:{0} -ConsoleTitle=\"{1} Proxy Client {2}\" -SessionName=\"{1} Proxy Client {2}\" -log={1}ProxyClient-{2}.log -SaveWinPos={2} ", ProxyListeningPort, ProjectName, TotalClientCount);
					
									StartProcessesForServerInfos(UniqueServerInfos, ProcessLaunchType.ProxyClient, ProxyClientAdditionalParams);
								}
								
								int NumBotsPerServer = BotCountTotal / ProxyServerCount;
								int NumExtraBots = BotCountTotal % ProxyServerCount;
								int NumBotsToSpawn = NumBotsPerServer + (ProxyServerIndex < NumExtraBots ? 1 : 0);
								for (int BotIndex = 0; BotIndex < NumBotsToSpawn; BotIndex++)
								{
									++TotalBotCount;
									var BotClientAdditionalParams = String.Format("127.0.0.1:{0} -log={2}ProxyBot-{1}.log -log -nullrhi -bot -nosound -unattended -nosplash ", ProxyListeningPort, TotalBotCount, ProjectName);
									BotClientAdditionalParams += String.Format("-ConsoleTitle=\"{0} Proxy Bot Client {1}\" -SessionName=\"{0} Proxy Bot Client {1}\" ", ProjectName, TotalBotCount);
									if (!ParseParam("nonewconsole"))
									{
										BotClientAdditionalParams += " -newconsole ";
									}
									StartProcessesForServerInfos(UniqueServerInfos, ProcessLaunchType.ProxyClient, BotClientAdditionalParams);
								}
								
								++ProxyListeningPort;
							}
						}
					}
				}
			}
			else
			{
				Logger.LogInformation("No server configs found in {0}, not starting any servers.", ServerDefConfigSection);
			}

			Logger.LogInformation("Run command time: {0:0.00} s", (DateTime.UtcNow - StartTime).TotalMilliseconds / 1000);
			Logger.LogInformation("********** RUN MULTISERVER COMMAND COMPLETED **********");

			return ExitCode.Success;
		}

		private void StartProcessesForServerInfos(Dictionary<string, MultiServerInstanceInfo> ServerInfos, ProcessLaunchType LaunchType, string AdditionalParameters)
		{
			const int PauseBetweenProcessMS = 500;

			var Params = new ProjectParams
				(
					Command: this,
					RawProjectPath: ProjectPath,
					DedicatedServer: LaunchType == ProcessLaunchType.Server || LaunchType == ProcessLaunchType.ProxyServer,
					Client: LaunchType == ProcessLaunchType.DirectClient || LaunchType == ProcessLaunchType.ProxyClient
				);

			var DeployContexts = Project.CreateDeploymentContext(Params, LaunchType == ProcessLaunchType.Server || LaunchType == ProcessLaunchType.ProxyServer);

			if (DeployContexts.Count == 0)
			{
				throw new AutomationException("No DeployContexts for launching a process.");
			}

			var DeployContext = DeployContexts[0];

			var App = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/Win64/UnrealEditor.exe");
			if (Params.Cook)
			{
				List<FileReference> Exes = DeployContext.StageTargetPlatform.GetExecutableNames(DeployContext);
				App = Exes[0].FullName;
			}

			var CommonArgs = (Params.Cook ? "" : DeployContext.ProjectArgForCommandLines + " ");
			if (LaunchType == ProcessLaunchType.Server || LaunchType == ProcessLaunchType.ProxyServer)
			{
				CommonArgs += Params.MapToRun + " -server -log -NODEBUGOUTPUT -SuppressConsoleOutputVerboseLogging ";

				// Here are some examples of adding parameters for debugging purposes (note the trailing space within the quotes)
				// CommonArgs += "-dpcvars=net.MaxConstructedPartialBunchSizeBytes=131071,dstm.SimpleCoinBots=0 ";
				// CommonArgs += "-logcmds=\"LogActor Verbose, LogRemoteObject Verbose, LogDstmPushMigration VeryVerbose, LogRepCompares Log, LogNet Verbose\" ";

				if (!ParseParam("nonewconsole"))
				{
					CommonArgs += " -newconsole ";
				}

				// Use this option when logging with VeryVerbose to avoid unusable debugging windows (too much spew)
				// The logging will still go to the output files (and you can use a text editor like Notepad++ to auto-reload them)
				if (ParseParam("noconsole"))
				{
					CommonArgs += " -nodebugoutput -noconsole ";
				}
				
				var Cats = new[] { "LogDstm", "LogDstmObject", "LogDstmDemo", "LogRemoteExec", "LogRemoteObject", "LogDstmPushMigration" };

				foreach (var Cat in Cats)
				{
					if (ParseParam("logdebug")) { CommonArgs += string.Format("-LogCmds=\"{0} Verbose\" ", Cat); }
					else if (ParseParam("logdebugv")) { CommonArgs += string.Format("-LogCmds=\"{0} VeryVerbose\" ", Cat); }
				}

			}

			CommonArgs += " -messaging ";

			PushDir(Path.GetDirectoryName(App));

			try
			{
				if (LaunchType == ProcessLaunchType.DirectClient || LaunchType == ProcessLaunchType.Server)
				{
					int ServerNum = 0;
					foreach (var ServerInfo in ServerInfos.Values)
					{
						Logger.LogInformation("Starting MultiServer instance of type {0} for LocalId {1}, beacon port {2}, and game port {3}", LaunchType, ServerInfo.Definition.LocalId, ServerInfo.Definition.ListenPort, ServerInfo.GameListenPort);

						var Args = CommonArgs;

						if (LaunchType == ProcessLaunchType.Server)
						{
							Args += String.Format("-MultiServerLocalId={0} -MultiServerListenPort={1} -port={2} -log={3}MultiServer-{0}.log ", ServerInfo.Definition.LocalId, ServerInfo.Definition.ListenPort, ServerInfo.GameListenPort, Params.ShortProjectName);
							Args += String.Format("-ConsoleTitle=\"{0} Server ID {1}\" -SessionName=\"{0} Server ID {1}\" ", Params.ShortProjectName, ServerInfo.Definition.LocalId);
							Args += Params.ServerCommandline + " ";
						}
						else if (LaunchType == ProcessLaunchType.DirectClient)
						{
							Args += String.Format("127.0.0.1:{0} ", ServerInfo.GameListenPort) + (Params.Cook ? "" : "-game ");
							Args += String.Format(" -log={1}DirectClient-{0}.log", ServerInfo.Definition.LocalId, Params.ShortProjectName);
							Args += String.Format(" -SessionName=\"{0} Direct Client | Server ID {1}\" ", Params.ShortProjectName, ServerInfo.Definition.LocalId);
							Args += Params.ClientCommandline + " ";

							// Save a separate window position for each client
							Args += "-windowed -SaveWinPos=" + (ServerNum + 1) + " ";
						}

						if (ParseParam("notimeouts"))
						{
							Args += "-notimeouts ";
						}

						if (IsAttachingDebugger(LaunchType))
						{
							Args += "-WaitForDebuggerNoBreak ";
						}

						Args += AdditionalParameters;

						var NewProcess = Run(App, Args, null, ERunOptions.Default | ERunOptions.NoWaitForExit | ERunOptions.NoStdOutRedirect);

						if (NewProcess != null)
						{
							// Remove started process so it won't be killed on UAT exit.
							// Essentially forces the -NoKill command-line option behavior for these.
							ProcessManager.RemoveProcess(NewProcess);
						}

						// Pause between starting processes to enforce startup determinism.
						System.Threading.Thread.Sleep(PauseBetweenProcessMS);

						if (IsAttachingDebugger(LaunchType))
						{
							ProcessDebugger.DebugProcess(NewProcess.ProcessObject.Id, this, Params);
						}

						ServerNum++;
					}
				}
				else if (LaunchType == ProcessLaunchType.ProxyServer)
				{
					var Args = CommonArgs;

					Args += "-NetDriverOverrides=/Script/MultiServerReplication.ProxyNetDriver ";

					if (ServerInfos.Count() > 0)
					{
						// Example of adding additional debug parameters for the proxy
						// Args += "-logcmds=\"LogNetProxy VeryVerbose\" ";

						if (ParseParam("notimeouts"))
						{
							Args += "-notimeouts ";
						}

						if (IsAttachingDebugger(LaunchType))
						{
							Args += "-WaitForDebuggerNoBreak ";
						}

						if (ServerInfos.Values.Count() > 0)
						{
							Args += "-ProxyGameServers=";

							foreach (var ServerInfo in ServerInfos.Values)
							{
								Args += String.Format("127.0.0.1:{0},", ServerInfo.GameListenPort);
							}

							Args += " ";
						}

						if (ParseParam("proxyclientprimary"))
						{
							int ProxyClientPrimary = 0;
							string ProxyClientPrimaryStr = ParseParamValue("proxyclientprimary", null);
							Int32.TryParse(ProxyClientPrimaryStr, out ProxyClientPrimary);

							Args += String.Format("-ProxyClientPrimaryGameServer={0} ", ProxyClientPrimary);
						}

						if (ParseParam("proxycycleprimary"))
						{
							Args += "-ProxyCyclePrimaryGameServer ";
						}
					}

					Args += Params.ServerCommandline + " ";
					Args += AdditionalParameters;

					var NewProcess = Run(App, Args, null, ERunOptions.Default | ERunOptions.NoWaitForExit | ERunOptions.NoStdOutRedirect);

					if (NewProcess != null)
					{
						// Remove started process so it won't be killed on UAT exit.
						// Essentially forces the -NoKill command-line option behavior for these.
						ProcessManager.RemoveProcess(NewProcess);
					}

					// Pause between starting processes to enforce startup determinism.
					System.Threading.Thread.Sleep(PauseBetweenProcessMS);

					if (IsAttachingDebugger(LaunchType))
					{
						ProcessDebugger.DebugProcess(NewProcess.ProcessObject.Id, this, Params);
					}
				}
				else if (LaunchType == ProcessLaunchType.ProxyClient)
				{
					var Args = CommonArgs;

					// The server to connect to should be provided in the AdditionalParameters (e.g. 127.0.0.1:8000).
					Args += AdditionalParameters + " ";
					Args += (Params.Cook ? "" : "-game ");
					Args += Params.ClientCommandline + " ";
					Args += "-windowed ";
					
					if (ParseParam("notimeouts"))
					{
						Args += "-notimeouts ";
					}

					if (IsAttachingDebugger(LaunchType))
					{
						Args += "-WaitForDebuggerNoBreak ";
					}

					var NewProcess = Run(App, Args, null, ERunOptions.Default | ERunOptions.NoWaitForExit | ERunOptions.NoStdOutRedirect);

					if (NewProcess != null)
					{
						// Remove started process so it won't be killed on UAT exit.
						// Essentially forces the -NoKill command-line option behavior for these.
						ProcessManager.RemoveProcess(NewProcess);
					}

					// Pause between starting processes to enforce startup determinism.
					System.Threading.Thread.Sleep(PauseBetweenProcessMS);

					if (IsAttachingDebugger(LaunchType))
					{
						ProcessDebugger.DebugProcess(NewProcess.ProcessObject.Id, this, Params);
					}
				}
			}
			catch
			{
				throw;
			}
			finally
			{
				PopDir();
			}
		}

		private bool IsAttachingDebugger(ProcessLaunchType LaunchType)
		{
			return (ParseParam("attachtoservers") && (LaunchType == ProcessLaunchType.Server || LaunchType == ProcessLaunchType.ProxyServer)) ||
				(ParseParam("attachtoclients") && (LaunchType == ProcessLaunchType.DirectClient || LaunchType == ProcessLaunchType.ProxyClient));
		}

		private FileReference ProjectFullPath;
		public virtual FileReference ProjectPath
		{
			get
			{
				if (ProjectFullPath == null)
				{
					ProjectFullPath = ParseProjectParam();

					if (ProjectFullPath == null)
					{
						throw new AutomationException("No project file specified. Use -project=<project>.");
					}
				}

				return ProjectFullPath;
			}
		}
	}
}
