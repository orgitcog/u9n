// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using Gauntlet;
using System;
using System.Collections.Generic;
using System.Data;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using UnrealBuildBase;
using UnrealBuildTool;

namespace EpicGame
{

	/// <summary>
	/// An additional set of options that pertain to internal epic games.
	/// </summary>
	public class EpicGameTestConfig : UnrealGame.UnrealTestConfig, IAutoParamNotifiable
	{
		/// <summary>
		/// Should this test skip mcp?
		/// </summary>
		[AutoParam]
		public bool NoMCP = false;

		/// <summary>
		/// Tell the server not to authenticate u
		/// </summary>
		[AutoParam]
		public bool DeviceAuthSkip = false;

		[AutoParam]
		public bool FastCook = false;

		/// <summary>
		/// Which backend to use for matchmaking
		/// </summary>
		[AutoParam]
		public string EpicApp = "DevLatest";

		/// <summary>
		/// Unique buildid to avoid matchmaking collisions
		/// </summary>
		[AutoParam]
		protected string BuildIDOverride = "";

		/// <summary>
		/// Unique server port to avoid matchmaking collisions
		/// </summary>
		[AutoParam]
		int ServerPortStart = 7777;

		protected int ServerPort { get; private set; }


		/// <summary>
		/// Unique server beacon port to avoid matchmaking collisions
		/// </summary>
		[AutoParam]
		int BeaconPortStart = 15000;

		protected int BeaconPort { get; private set; }
		
		/// <summary>
		/// Make sure the client gets -logpso when we are collecting them
		/// </summary>
		[AutoParam]
        public bool LogPSO = false;

		/// <summary>
		/// Which Mempro tags we want to track if we need them. Note, should only be used in short runs.
		/// </summary>
		[AutoParam]
		public string MemPro;


		/// <summary>
		/// Should this test assign a random test account?
		/// </summary>
		[AutoParam]
		public bool PreAssignAccount = true;

		/// <summary>
		/// Does the current test require a user to be logged in to function correctly?
		/// </summary>
		[AutoParam]
		public bool RequiresLogin = false;

		/// <summary>
		/// If true, do not apply args to client and server as if they were running on the same host
		/// </summary>
		[AutoParam]
		public bool RemoteServer = false;

		/// <summary>
		/// Optional specifier for client mcp region
		/// </summary>
		[AutoParam]
		public string McpRegion = string.Empty;

		/// <summary>
		/// Optional specifier for client mcp subregion
		/// </summary>
		[AutoParam]
		public string McpSubRegion = string.Empty;

		/// Set traces to collect for editor roles, saved to a generated file.
		/// </summary>
		[AutoParam]
		public string EditorTraces { get; set; }


		// incrementing value to ensure we can assign unique values to ports etc
		static private int NumberOfConfigsCreated = 0;

		public EpicGameTestConfig()
		{
			NumberOfConfigsCreated++;
		}

		~EpicGameTestConfig()
		{
		}


		public void ParametersWereApplied(string[] Params)
		{
			
			if (string.IsNullOrEmpty(BuildIDOverride))
			{
				// pick a default buildid that's the last 4 digits of our IP
				string LocalIP = Dns.GetHostEntry(Dns.GetHostName()).AddressList.Where(o => o.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork).First().ToString();
				LocalIP = LocalIP.Replace(".", "");
				BuildIDOverride = LocalIP.Substring(LocalIP.Length - 4);
			}

			ServerPort = ServerPortStart;
			BeaconPort = BeaconPortStart;

			// techinically this doesn't matter for mcp because the server will pick a free port and tell the backend what its using, but
			// nomcp requires us to know the port and thus we need to make sure ones we pick haven't been previously assigned or grabbed
			if (NumberOfConfigsCreated > 1)
			{
				BuildIDOverride += string.Format("{0}", NumberOfConfigsCreated);
				ServerPort = (ServerPortStart + NumberOfConfigsCreated);
				BeaconPort = (BeaconPortStart + NumberOfConfigsCreated);
			}
		}

		public override void ApplyToConfig(UnrealAppConfig AppConfig, UnrealSessionRole ConfigRole, IEnumerable<UnrealSessionRole> OtherRoles)
		{
			base.ApplyToConfig(AppConfig, ConfigRole, OtherRoles);

			if (ConfigRole.RoleType.IsClient() || ConfigRole.RoleType.IsServer() || RequiresLogin)
			{
				string McpString = "";
				bool bIsBuildMachine = CommandUtils.IsBuildMachine; 

				if (ConfigRole.RoleType.IsServer() && !RemoteServer)
				{
					// set explicit server and beacon port for online services
					// this is important when running tests in parallel to avoid matchmaking collisions
					McpString += string.Format(" -port={0}", ServerPort);
					McpString += string.Format(" -beaconport={0}", BeaconPort);

					AppConfig.CommandLine += " -net.forcecompatible";

					if (!bIsBuildMachine || !NoMCP)
					{
						AppConfig.CommandLineParams.Add("UseLocalIPs");
					}
				}

				// Default to the first address with a valid prefix
				var LocalAddress = Dns.GetHostEntry(Dns.GetHostName()).AddressList
					.Where(o => o.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork
						&& o.GetAddressBytes()[0] != 169)
					.FirstOrDefault();

				var ActiveInterfaces = NetworkInterface.GetAllNetworkInterfaces()
					.Where(I => I.OperationalStatus == OperationalStatus.Up);

				bool MultipleInterfaces = ActiveInterfaces.Count() > 1;

				if (MultipleInterfaces)
				{
					// Now, lots of Epic PCs have virtual adapters etc, so see if there's one that's on our network and if so use that IP
					var PreferredInterface = ActiveInterfaces
						.Where(I => I.GetIPProperties().DnsSuffix.Equals("epicgames.net", StringComparison.OrdinalIgnoreCase))
						.SelectMany(I => I.GetIPProperties().UnicastAddresses)
						.Where(A => A.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
						.FirstOrDefault();

					if (PreferredInterface != null)
					{
						LocalAddress = PreferredInterface.Address;
					}
				}

				if (LocalAddress == null)
				{
					throw new AutomationException("Could not find local IP address");
				}

				string RequestedServerIP = Globals.Params.ParseValue("serverip", "");
				string RequestedClientIP = Globals.Params.ParseValue("clientip", "");
				string ServerIP = string.IsNullOrEmpty(RequestedServerIP) ? LocalAddress.ToString() : RequestedServerIP;
				string ClientIP = string.IsNullOrEmpty(RequestedClientIP) ? LocalAddress.ToString() : RequestedClientIP;


				// Do we need to add the -multihome argument to bind to specific IP?
				if (ConfigRole.RoleType.IsServer() && !RemoteServer && (MultipleInterfaces || !string.IsNullOrEmpty(RequestedServerIP)))
				{
					AppConfig.CommandLine += string.Format(" -multihome={0} -multihomehttp={0}", ServerIP);
				}

				// client too, but only desktop platforms
				if (ConfigRole.RoleType.IsClient() && !RemoteServer && (MultipleInterfaces || !string.IsNullOrEmpty(RequestedClientIP)))
				{
					if (ConfigRole.Platform == UnrealTargetPlatform.Win64 || ConfigRole.Platform == UnrealTargetPlatform.Mac)
					{
						AppConfig.CommandLine += string.Format(" -multihome={0} -multihomehttp={0}", ClientIP);
					}
				}

				if (NoMCP)
				{
					if(RemoteServer)
					{
						throw new AutomationException("Attempted to use a remote server when running a test with nomcp, this is not supported.");
					}

					McpString += " -nomcp -notimeouts";

					// if this is a client, and there is a server role, find our PC's IP address and tell it to connect to us
					if (ConfigRole.RoleType.IsClient() &&
							(RequiredRoles.ContainsKey(UnrealTargetRole.Server) || RequiredRoles.ContainsKey(UnrealTargetRole.EditorServer)))
					{
						McpString += string.Format(" -ExecCmds=\"open {0}:{1}\"", ServerIP, ServerPort);
					}
				}
				else
				{
					if (Globals.Params.ParseParam("nobuildid"))
					{
						McpString += string.Format(" -epicapp={0} ", EpicApp);
					}
					else
					{
						McpString += string.Format(" -epicapp={0} -buildidoverride={1}", EpicApp, BuildIDOverride);
					}

					bool bRequestRegion = !string.IsNullOrEmpty(McpRegion);
					if (ConfigRole.RoleType.IsClient() && bRequestRegion)
					{
						McpString += string.Format(" -McpRegion={0}", McpRegion);

						if (bRequestRegion && !string.IsNullOrEmpty(McpSubRegion))
						{
							McpString += string.Format(" -McpSubRegion={0}", McpSubRegion);
						}
					}
				}

				if (FastCook)
				{
					McpString += " -FastCook";
				}

				AppConfig.CommandLine += McpString;
			}

			if (ConfigRole.RoleType.IsClient() || RequiresLogin)
			{
				bool bNoAccountOverride = DataDrivenPlatformInfo.GetDataDrivenInfoForPlatform(ConfigRole.Platform.ToString())?.bNoAccountOverride ?? false;
				// select an account
				if (!NoMCP)
				{
					if (!bNoAccountOverride && PreAssignAccount)
					{
						Account UserAccount = AccountPool.Instance.ReserveAccount();
						UserAccount.ApplyToConfig(AppConfig);
					}
				}
			}

			if (ConfigRole.RoleType.IsClient())
			{
                if (LogPSO)
                {
                    AppConfig.CommandLine += " -logpso";
                }

				if (!string.IsNullOrEmpty(MemPro))
				{
					AppConfig.CommandLineParams.AddOrAppendParamValue("memprotags", MemPro);
					AppConfig.CommandLineParams.AddUnique("mempro");
					AppConfig.CommandLineParams.AddUnique("llm");
					AppConfig.CommandLineParams.AddUnique("llmcsv");
					AppConfig.CommandLineParams.AddUnique("nothreadtimeout");
				}

                if (ConfigRole.Platform == UnrealTargetPlatform.Win64)
				{
					// turn off skill-based matchmaking, turn off porta;
					AppConfig.CommandLine += " -noepicportal";
				}

				// turn off crashlytics so we get symbolicated tombstone crashes on Android
				if (ConfigRole.Platform == UnrealTargetPlatform.Android)
				{
					AppConfig.CommandLine += " -nocrashlytics";
				}
			}

			if (ConfigRole.RoleType.IsEditor())
			{
				if (!string.IsNullOrEmpty(EditorTraces))
				{
					string GeneratedTraceFile = Path.Combine(Unreal.EngineDirectory.FullName, "Programs", "AutomationTool", "Saved", "Logs", "EditorTraces.utrace");
					AppConfig.CommandLine += $" -trace={EditorTraces} -tracefile={GeneratedTraceFile}";
				}
			}
		}		
	}

	/// <summary>
	/// Generic TestNode class for Epic Games internal projects.
	/// </summary>
	public abstract class EpicGameTestNode<TConfigClass> : UnrealTestNode<TConfigClass>
		where TConfigClass : EpicGameTestConfig, new()
	{
		public EpicGameTestNode(UnrealTestContext InContext) : base(InContext)
		{

		}
	}

}