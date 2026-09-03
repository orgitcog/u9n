// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Utilities;
using Google.Protobuf;
using HordeAgent.Services;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace HordeAgent.Leases.Handlers
{
	class JobHandler : LeaseHandler<ExecuteJobTask>
	{
		private readonly IOptionsMonitor<AgentSettings> _settings;
		
		public JobHandler(RpcLease lease, IOptionsMonitor<AgentSettings> settings)
			: base(lease)
		{
			_settings = settings;
		}

		/// <inheritdoc/>
		protected override async Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, ExecuteJobTask executeTask, Tracer tracer, ILogger localLogger, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = tracer.StartActiveSpan($"{nameof(JobHandler)}.{nameof(ExecuteAsync)}");
			span.SetAttribute("horde.job.id", executeTask.JobId);
			span.SetAttribute("horde.job.name", executeTask.JobName);
			span.SetAttribute("horde.job.batch_id", executeTask.BatchId);

			await using IServerLogger logger = session.HordeClient.CreateServerLogger(LogId.Parse(executeTask.LogId)).WithLocalLogger(localLogger);

			int exitCode = 0;
			Exception? exception = null;

			executeTask.JobOptions ??= new RpcJobOptions();

			List<string> arguments = new List<string>();
			arguments.Add("execute");
			arguments.Add("job");
			arguments.Add($"-AgentId={session.AgentId}");
			arguments.Add($"-SessionId={session.SessionId}");
			arguments.Add($"-LeaseId={leaseId}");
			arguments.Add($"-WorkingDir={session.WorkingDir}");
			arguments.Add($"-Task={Convert.ToBase64String(executeTask.ToByteArray())}");

			string driverName = String.IsNullOrEmpty(executeTask.JobOptions.Driver) ? "JobDriver" : executeTask.JobOptions.Driver;			
			FileReference driverAssembly = FileReference.Combine(new DirectoryReference(AppContext.BaseDirectory), driverName, $"{driverName}.dll");			
			span.SetAttribute("horde.job.driver_name", driverName);

			Dictionary<string, string> environment = ManagedProcess.GetCurrentEnvVars();

			try
			{
				environment[HordeHttpClient.HordeUrlEnvVarName] = session.HordeClient.ServerUrl.ToString();
				environment[HordeHttpClient.HordeTokenEnvVarName] = executeTask.Token;
				environment["UE_LOG_JSON_TO_STDOUT"] = "1";
				environment["UE_HORDE_OTEL_SETTINGS"] = OpenTelemetrySettingsExtensions.Serialize(_settings.CurrentValue.OpenTelemetry, true);

				exitCode = await RunDotNetProcessAsync(driverAssembly, arguments, environment, AgentApp.IsSelfContained, logger, cancellationToken);
				logger.LogInformation("Driver finished with exit code {ExitCode}", exitCode);				
			}
			catch (OperationCanceledException ex)
			{
				logger.LogError(ex, "Lease was cancelled ({Reason})", CancellationReason);
				exception = ex;
			}
			catch (Exception ex)
			{
				logger.LogError(ex, "Unhandled exception: {Message}", ex.Message);
				exception = ex;
			}

			if (exception != null || exitCode != 0)
			{
				try
				{
					logger.LogInformation("Driver initiating job batch cleanup");
					// Run job batch cleanup
					environment["UE_HORDE_JOB_BATCH_CLEANUP"] = "1";
					int cleanupCode = await RunDotNetProcessAsync(driverAssembly, arguments, environment, AgentApp.IsSelfContained, logger, CancellationToken.None);
					if (cleanupCode != 0)
					{
						logger.LogInformation("Driver job batch cleanup finished with exit code {ExitCode}", cleanupCode);
					}
				}
				catch (Exception ex)
				{
					logger.LogError(ex, "Driver job batch cleanup error ({Message})", ex.Message);
				}

				if (exception != null)
				{
					throw exception;
				}
			}

			return (exitCode == 0) ? LeaseResult.Success : LeaseResult.Failed;
		}
	}

	class JobHandlerFactory : LeaseHandlerFactory<ExecuteJobTask>
	{
		private readonly IOptionsMonitor<AgentSettings> _settings;
		
		public JobHandlerFactory(IOptionsMonitor<AgentSettings> settings)
		{
			_settings = settings;
		}
		
		public override LeaseHandler<ExecuteJobTask> CreateHandler(RpcLease lease)
			=> new JobHandler(lease, _settings);
	}
}

