// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Server;
using HordeServer.Plugins;
using HordeServer.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace HordeServer.Server
{
	/// <summary>
	/// Controller managing account status
	/// </summary>
	[ApiController]
	[Authorize]
	[CppApi]
	[Route("[controller]")]
	public class ServerController : HordeControllerBase
	{
		readonly IAgentVersionProvider? _agentVersionProvider;
		readonly IPluginCollection _pluginCollection;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		readonly ILogger<ServerController> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ServerController(IEnumerable<IAgentVersionProvider> agentVersionProviders, IPluginCollection pluginCollection, IOptionsSnapshot<GlobalConfig> globalConfig, ILogger<ServerController> logger)
		{
			_agentVersionProvider = agentVersionProviders.FirstOrDefault();
			_pluginCollection = pluginCollection;
			_globalConfig = globalConfig;
			_logger = logger;
		}

		/// <summary>
		/// Get server version
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/version")]
		public ActionResult GetVersion()
		{
			FileVersionInfo fileVersionInfo = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
			return Ok(fileVersionInfo.ProductVersion);
		}

		/// <summary>
		/// Get server information
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/info")]
		[ProducesResponseType(typeof(GetServerInfoResponse), 200)]
		public async Task<ActionResult<GetServerInfoResponse>> GetServerInfoAsync()
		{
			GetServerInfoResponse response = new GetServerInfoResponse();
			response.ApiVersion = HordeApiVersion.Latest;

			FileVersionInfo versionInfo = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
			response.ServerVersion = versionInfo.ProductVersion ?? String.Empty;

			if (_agentVersionProvider != null)
			{
				response.AgentVersion = await _agentVersionProvider.GetAsync(HttpContext.RequestAborted);
			}

			response.Plugins = _pluginCollection.LoadedPlugins.ConvertAll(plugin => {
				FileVersionInfo pluginVersion = FileVersionInfo.GetVersionInfo(plugin.Assembly.Location);
				return new ServerPluginInfoResponse(plugin.Metadata.Name.ToString(), plugin.Metadata.Description, true, pluginVersion.ProductVersion ?? String.Empty);
			}).ToArray();

			return response;
		}

		/// <summary>
		/// Gets connection information
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/connection")]
		public ActionResult<GetConnectionResponse> GetConnection()
		{
			GetConnectionResponse response = new GetConnectionResponse();
			response.Ip = HttpContext.Connection.RemoteIpAddress?.ToString();
			response.Port = HttpContext.Connection.RemotePort;
			return response;
		}

		/// <summary>
		/// Gets ports used by the server
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/ports")]
		public ActionResult<GetPortsResponse> GetPorts()
		{
			ServerSettings serverSettings = _globalConfig.Value.ServerSettings;

			GetPortsResponse response = new GetPortsResponse();
			response.Http = serverSettings.HttpPort;
			response.Https = serverSettings.HttpsPort;
			response.UnencryptedHttp2 = serverSettings.Http2Port;
			return response;
		}

		private const string ServerAuthRoute = "/api/v1/server/auth";
		
		/// <summary>
		/// Returns settings for automating auth against this server
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route(ServerAuthRoute)]
		public ActionResult<GetAuthConfigResponse> GetAuthConfig()
		{
			ServerSettings settings = _globalConfig.Value.ServerSettings;

			GetAuthConfigResponse response = new GetAuthConfigResponse();
			response.Method = settings.AuthMethod;
			response.ProfileName = settings.OidcProfileName;
			if (settings.AuthMethod == AuthMethod.Horde)
			{
				response.ServerUrl = new Uri(_globalConfig.Value.ServerSettings.ServerUrl, "api/v1/oauth2").ToString();
				response.ClientId = "default";
			}
			else
			{
				if (!String.IsNullOrEmpty(settings.OidcClientSecret))
				{
					_logger.LogWarning(
						"OIDC config mismatch: Command-line auth requires a public OAuth/OIDC client, but a confidential client is configured ({ClientSecret} is set). " +
						"This will prevent Horde's C# client from signing in and block usage of Unreal Build Accelerator. " +
						"To fix: Configure your OAuth/OIDC client as public (SPA/mobile/desktop) and remove the client secret",
						nameof(ServerSettings.OidcClientSecret)
					);
				}
				response.ServerUrl = settings.OidcAuthority;
				response.ClientId = settings.OidcClientId;
				response.Scopes = settings.OidcApiRequestedScopes;
			}
			response.LocalRedirectUrls = settings.OidcLocalRedirectUrls;
			return response;
		}
	}
}
