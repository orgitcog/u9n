// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Net.Mime;
using System.Threading.Tasks;
using System.Xml;
using AutomationTool.Tasks.CloudDDC;
using EpicGames.Core;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Commits;
using EpicGames.Horde.Streams;
using EpicGames.Serialization;
using Microsoft.Extensions.Logging;
using UnrealBuildTool;

#nullable enable

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a <see cref="RetrieveCloudArtifactTask"/>.
	/// </summary>
	public class RetrieveCloudArtifactTaskParameters
	{
		/// <summary>
		/// Stream containing the artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public string? StreamId { get; set; } = null!;

		/// <summary>
		/// Change number for the artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public string? Commit { get; set; }

		/// <summary>
		/// Maximum commit for the artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public string? MaxCommit { get; set; }

		/// <summary>
		/// Name of the artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Name { get; set; } = null!;

		/// <summary>
		/// The artifact type. Determines the permissions and expiration policy for the artifact.
		/// </summary>
		[TaskParameter]
		public string Type { get; set; } = null!;

		/// <summary>
		/// Keys for the artifact
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Keys { get; set; } = null!;

		/// <summary>
		/// Output directory for 
		/// </summary>
		[TaskParameter]
		public string? OutputDir { get; set; }

		/// <summary>
		/// The platform the artifact is created for
		/// </summary>
		[TaskParameter]
		public string Platform { get; set; } = null!;

		/// <summary>
		/// The path to the uproject this artifact is created for
		/// </summary>
		[TaskParameter]
		public FileReference Project { get; set; } = null!;

		/// <summary>
		/// The platform the artifact is created for
		/// </summary>
		[TaskParameter(Optional = true)]
		public string? Host { get; set; }

		/// <summary>
		/// The platform the artifact is created for
		/// </summary>
		[TaskParameter(Optional = true)]
		public string? Namespace { get; set; }

		/// <summary>
		/// The access token to use
		/// </summary>
		[TaskParameter(Optional = true)]
		public string? AccessToken { get; set; }

		/// <summary>
		/// Set this to use the latest match if multiple artifacts are possible matches
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool AllowMultipleMatches { get; set; } = false;

		/// <summary>
		/// Enable to use multipart endpoints if valuable
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool AllowMultipart { get; set; } = false;

		/// <summary>
		/// Set the explicit http version to use. None to use http handshaking.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string HttpVersion { get; set; } = "None";

		/// <summary>
		/// Increase the number of worker threads used by zen, may cause machine to be less responsive but will generally improve download times
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool BoostWorkers { get; set; } = false;
	}

	/// <summary>
	/// Retrieves an artifact from Cloud DDC
	/// </summary>
	[TaskElement("RetrieveCloudArtifact", typeof(RetrieveCloudArtifactTaskParameters))]
	public class RetrieveCloudArtifactTask : BgTaskImpl
	{
		readonly RetrieveCloudArtifactTaskParameters _parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="parameters">Parameters for this task.</param>
		public RetrieveCloudArtifactTask(RetrieveCloudArtifactTaskParameters parameters)
			=> _parameters = parameters;

		/// <summary>
		/// ExecuteAsync the task.
		/// </summary>
		/// <param name="job">Information about the current job.</param>
		/// <param name="buildProducts">Set of build products produced by this node.</param>
		/// <param name="tagNameToFileSet">Mapping from tag names to the set of files they include.</param>
		public override async Task ExecuteAsync(JobContext job, HashSet<FileReference> buildProducts, Dictionary<string, HashSet<FileReference>> tagNameToFileSet)
		{
			if (!String.IsNullOrEmpty(_parameters.Commit) && !String.IsNullOrEmpty(_parameters.MaxCommit))
			{
				throw new AutomationException("Cannot specify both Commit and MaxCommit parameters for retrieving an artifact.");
			}

			string project = _parameters.Project.GetFileNameWithoutExtension();
			FileReference? projectPath = _parameters.Project;
			if (!File.Exists(_parameters.Project.FullName))
			{
				// if the project is not an uproject that exists we do the scanning for engine inis without the project specific overrides
				projectPath = null;
				Logger.LogInformation("\'{Project}\' is not a valid file. Loading engine configs without project overrides.", _parameters.Project);
			}
			string platform = _parameters.Platform;

			bool foundConfig = false;
			CloudConfiguration? cloudConfig = null;

			if (projectPath != null)
			{
				(Dictionary<UnrealTargetPlatform, ConfigHierarchy> engineConfigs, Dictionary<UnrealTargetPlatform, ConfigHierarchy> _) = CreateCloudArtifactTask.GetIniConfigs(projectPath);
				ConfigHierarchy config = engineConfigs[HostPlatform.Current.HostEditorPlatform];
				foundConfig = config.TryGetValueGeneric("StorageServers", "Cloud", out CloudConfiguration foundCloudConfig);
				cloudConfig = foundCloudConfig;
			}

			// Figure out the current change and stream id
			StreamId streamId;
			if (!String.IsNullOrEmpty(_parameters.StreamId))
			{
				streamId = new StreamId(_parameters.StreamId);
			}
			else
			{
				string? streamIdEnvVar = Environment.GetEnvironmentVariable("UE_HORDE_STREAMID");
				if (!String.IsNullOrEmpty(streamIdEnvVar))
				{
					streamId = new StreamId(streamIdEnvVar);
				}
				else
				{
					throw new AutomationException("Missing UE_HORDE_STREAMID environment variable; unable to determine current stream.");
				}
			}

			string? cloudHostEnvVar = Environment.GetEnvironmentVariable(OperatingSystem.IsWindows() ? "UE-CloudPublishHost" : "UE_CloudPublishHost");
			
			string host;
			if (!String.IsNullOrEmpty(_parameters.Host))
			{
				host = _parameters.Host;
			}
			else if (!String.IsNullOrEmpty(cloudHostEnvVar))
			{
				host = cloudHostEnvVar;
			}
			else if (foundConfig && cloudConfig != null && !String.IsNullOrEmpty(cloudConfig.Value.Host))
			{
				string cloudConfigHost = cloudConfig.Value.Host;
				if (cloudConfigHost.Contains(';', StringComparison.OrdinalIgnoreCase))
				{
					// if its a list pick the first element
					cloudConfigHost = cloudConfigHost.Split(";").First();
				}

				host = cloudConfigHost;
			}
			else
			{
				throw new AutomationException("Missing UE-CloudPublishHost environment variable; unable to determine cloud host. Specify the environment variable or define it in your StorageServer section in your engine ini");
			}

			string ns;
			string? cloudDefaultNamespaceEnv = Environment.GetEnvironmentVariable(OperatingSystem.IsWindows() ? "UE-CloudPublishNamespace" : "UE_CloudPublishNamespace");
				
			if (!String.IsNullOrEmpty(_parameters.Namespace))
			{
				ns = _parameters.Namespace;
			}
			else if (!String.IsNullOrEmpty(cloudDefaultNamespaceEnv))
			{
				ns = cloudDefaultNamespaceEnv;
			}
			else if (foundConfig && cloudConfig != null && !String.IsNullOrEmpty(cloudConfig.Value.BuildsNamespace))
			{
				ns = cloudConfig.Value.BuildsNamespace;
			}
			else
			{
				throw new AutomationException("Missing UE-CloudPublishNamespace environment variable; unable to default namespace please specify it in the task or define it in your StorageServer section in your engine ini.");
			}
			string httpVersion = _parameters.HttpVersion;
			if (String.IsNullOrEmpty(httpVersion) || httpVersion == "None")
			{
				string? httpVersionEnvironmentVariable = Environment.GetEnvironmentVariable(OperatingSystem.IsWindows() ? "UE-CloudPublishHttpVersion": "UE_CloudPublishHttpVersion");
				if (!String.IsNullOrEmpty(httpVersionEnvironmentVariable))
				{
					httpVersion = httpVersionEnvironmentVariable;
				}
			}

			string accessToken;
			if (!String.IsNullOrEmpty(_parameters.AccessToken))
			{
				accessToken = _parameters.AccessToken;
			}
			else
			{
				string? cloudAccessTokenEnvVar = Environment.GetEnvironmentVariable(OperatingSystem.IsWindows() ? "UE-CloudDataCacheAccessToken" : "UE_CloudDataCacheAccessToken");
				if (!String.IsNullOrEmpty(cloudAccessTokenEnvVar))
				{
					accessToken = cloudAccessTokenEnvVar;
				}
				else
				{
					throw new AutomationException("Missing UE-CloudDataCacheAccessToken environment variable; unable to find access token to use.");
				}
			}

			// Get the current commit id
			CommitId? minCommitId = null;
			CommitId? maxCommitId = null;
			if (!String.IsNullOrEmpty(_parameters.MaxCommit))
			{
				maxCommitId = new CommitId(_parameters.MaxCommit);
			}
			else if (!String.IsNullOrEmpty(_parameters.Commit))
			{
				minCommitId = maxCommitId = new CommitId(_parameters.Commit);
			}
			else
			{
				try
				{
					int change = CommandUtils.P4Env.Changelist;
					if (change > 0)
					{
						minCommitId = maxCommitId = CommitId.FromPerforceChange(CommandUtils.P4Env.Changelist);
					}
				}
				catch (AutomationException)
				{
					// not an error to run without p4
				}
			}

			string? name = null;
			if (!String.IsNullOrEmpty(_parameters.Name))
			{
				name = _parameters.Name;
			}

			ArtifactType type = new ArtifactType(_parameters.Type);
			List<string> keys = (_parameters.Keys ?? String.Empty).Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).ToList();

			CbWriter queryWriter = new CbWriter();
			queryWriter.BeginObject(); // root
			queryWriter.BeginObject("query"); // query
			
			queryWriter.BeginObject("stream");
			queryWriter.WriteString("$eq", CreateCloudArtifactTask.SanitizeBucketValue(streamId.ToString()));
			queryWriter.EndObject();

			if (maxCommitId != null)
			{
				if (minCommitId == maxCommitId)
				{
					queryWriter.BeginObject("commit");
					queryWriter.WriteString("$eq", maxCommitId.ToString());
					queryWriter.EndObject();
				}
				else if (minCommitId != null)
				{
					queryWriter.BeginObject("commit");
					queryWriter.WriteString("$lte", maxCommitId.ToString());
					queryWriter.WriteString("$gte", minCommitId.ToString());
					queryWriter.EndObject();
				}
				else
				{
					queryWriter.BeginObject("commit");
					queryWriter.WriteString("$lte", maxCommitId.ToString());
					queryWriter.EndObject();
				}
			}

			if (name != null)
			{
				queryWriter.BeginObject("name");
				queryWriter.WriteString("$eq", name);
				queryWriter.EndObject();
			}

			queryWriter.BeginObject("type");
			queryWriter.WriteString("$eq", type.ToString());
			queryWriter.EndObject();

			if (keys.Count > 0)
			{
				queryWriter.BeginObject("keys");
				queryWriter.BeginUniformArray("$in", CbFieldType.String);
				foreach (string key in keys)
				{
					queryWriter.WriteStringValue(key);
				}
				queryWriter.EndUniformArray();
				queryWriter.EndObject();
			}
			
			queryWriter.EndObject(); // end query
			queryWriter.BeginObject("options");
			queryWriter.WriteInteger("limit", 1); // we only care about the first match
			queryWriter.EndObject(); // options
			queryWriter.EndObject(); // end root object

			CbObject queryObject = queryWriter.ToObject();
			DirectoryReference outputDir = ResolveDirectory(_parameters.OutputDir);
			string bucketId = $"{CreateCloudArtifactTask.SanitizeBucketValue(project)}.{CreateCloudArtifactTask.SanitizeBucketValue(type.ToString())}.{CreateCloudArtifactTask.SanitizeBucketValue(streamId.ToString())}.{CreateCloudArtifactTask.SanitizeBucketValue(platform)}".ToLowerInvariant();
			using HttpClient httpClient = BuildHttpClient(host, ns, bucketId, accessToken);

			List<CbObjectId> artifactIds = await SearchArtifactsAsync(httpClient, queryObject);

			if (artifactIds.Count == 0)
			{
				throw new AutomationException($"Unable to find any artifact matching given criteria in namespace {ns} and bucket {bucketId} criteria: {queryObject.ToJson()}");
			}

			if (!_parameters.AllowMultipleMatches && artifactIds.Count != 1)
			{
				throw new AutomationException("More then one matching artifact given criteria, set \"AllowMultipleMatches\" option if you want to use the newest matching artifact or refine the search");
			}
			
			CbObjectId artifact = artifactIds.First();
			Logger.LogInformation("Found artifact {ArtifactId}", artifact);

			BuildDownload.DownloadBuild(host, outputDir, ns, bucketId, artifact.ToString(), accessToken, allowMultipart: _parameters.AllowMultipart, assumeHttp2: String.Equals(httpVersion, "http2-only", StringComparison.OrdinalIgnoreCase), boostWorkers: _parameters.BoostWorkers);
		}

		static HttpClient BuildHttpClient(string host, string ns, string bucketId, string accessToken)
		{
			HttpClient httpClient = new HttpClient();
			string url = $"{host}/api/v2/builds/{ns}/{bucketId}/";
			try
			{
				httpClient.BaseAddress = new Uri(url);
			}
			catch (UriFormatException)
			{
				throw new AutomationException($"{url} is not a valid url. Make sure host, namespace and bucket have been specified");
			}

			CbObjectId sessionId = CbObjectId.NewObjectId();
			Logger.LogInformation("Using SessionId {SessionId} please include this in any error reports.", sessionId);
			string authScheme = "Bearer"; // TODO: for a proper implementation this should be configurable
			httpClient.DefaultRequestHeaders.Add("Authorization", $"{authScheme} {accessToken}");
			httpClient.DefaultRequestHeaders.Add("UE-Session", sessionId.ToString());
			httpClient.DefaultRequestHeaders.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);
			httpClient.Timeout = TimeSpan.FromMinutes(5.0); // bump timeout for each request as we sometimes have larger files
			return httpClient;
		}
		static async Task<List<CbObjectId>> SearchArtifactsAsync(HttpClient httpClient, CbObject queryObject)
		{
			using StringContent content = new StringContent(queryObject.ToJson());
			content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Json);

			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, new Uri($"search", UriKind.Relative));
			request.Content = content;
			request.Headers.Add("Accept", MediaTypeNames.Application.Json);
			using HttpResponseMessage result = await httpClient.SendAsync(request);
			result.EnsureSuccessStatusCode();

			SearchResult? searchResult = await result.Content.ReadFromJsonAsync<SearchResult>();
			return searchResult!.Results.Select(r => r.BuildId).ToList();
		}

		/// <inheritdoc/>
		public override void Write(XmlWriter writer)
			=> Write(writer, _parameters);

		/// <inheritdoc/>
		public override IEnumerable<string> FindConsumedTagNames()
			=> Enumerable.Empty<string>();

		/// <inheritdoc/>
		public override IEnumerable<string> FindProducedTagNames()
			=> Enumerable.Empty<string>();
	}

	class BuildSearchResult
	{
		[CbField("buildId")]
		public CbObjectId BuildId { get; set; }
	}

	class SearchResult
	{
		[CbField("results")] public List<BuildSearchResult> Results { get; set; } = new List<BuildSearchResult>();

		[CbField("partialResult")] public bool PartialResult { get; set; }

		public SearchResult()
		{
			Results = new List<BuildSearchResult>();
			PartialResult = false;
		}
	}

	static class BuildDownload
	{
		public static void DownloadBuild(string host, DirectoryReference targetDir, string namespaceId, string bucketId, string buildId, string accessToken, bool allowMultipart = false, bool assumeHttp2 = false, bool boostWorkers = false)
		{
			FileInfo zenExe = new FileInfo("Engine/Binaries/Win64/zen.exe");

			string http2Options = assumeHttp2 ? "--assume-http2" : "";
			string boostWorkersOption = boostWorkers ? "--boost-workers" : "";
			// pass the access token via environment variable to avoid showing it on the cli
			string cmdline =
				$"builds download --url {host} --namespace {namespaceId} --bucket {bucketId} --access-token-env UE-CloudDataCacheAccessToken --plain-progress --allow-multipart={allowMultipart} {http2Options} {boostWorkersOption} \"{targetDir}\" {buildId}";
			CommandUtils.RunAndLog(CommandUtils.CmdEnv, zenExe.FullName, cmdline, Options: CommandUtils.ERunOptions.Default,
				EnvVars: new Dictionary<string, string> { { "UE-CloudDataCacheAccessToken", accessToken } });
		}
	}
}
