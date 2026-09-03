// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaJobProcessor.h"

#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreMisc.h"
#include "UbaControllerModule.h"
#include "UbaHordeAgentManager.h"
#include "UbaHordeConfig.h"
#include "UbaStringConversion.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"

#include "Misc/Paths.h"
#include "Misc/ScopeRWLock.h"

namespace UbaJobProcessorOptions
{
	static float SleepTimeBetweenActions = 0.01f;
	static FAutoConsoleVariableRef CVarSleepTimeBetweenActions(
        TEXT("r.UbaController.SleepTimeBetweenActions"),
        SleepTimeBetweenActions,
        TEXT("How much time the job processor thread should sleep between actions .\n"));

	static float MaxTimeWithoutTasks = 100.0f;
	static FAutoConsoleVariableRef CVarMaxTimeWithoutTasks(
        TEXT("r.UbaController.MaxTimeWithoutTasks"),
        MaxTimeWithoutTasks,
        TEXT("Time to wait (in seconds) before stop processing attempts if we don't have any pending task.\n"));

	static float HeartBeatInterval = 180.0f;
	static FAutoConsoleVariableRef CVarHeartBeatInterval(
		TEXT("r.UbaController.HeartBeatInterval"),
		HeartBeatInterval,
		TEXT("Time between heart beat log messages"));

	static bool bAutoLaunchVisualizer = false;
	static FAutoConsoleVariableRef CVarAutoLaunchVisualizer(
		TEXT("r.UbaController.AutoLaunchVisualizer"),
		bAutoLaunchVisualizer,
		TEXT("If true, UBA visualizer will be launched automatically\n"));

	static bool bAllowProcessReuse = true;
	static FAutoConsoleVariableRef CVarAllowProcessReuse(
		TEXT("r.UbaController.AllowProcessReuse"),
		bAllowProcessReuse,
		TEXT("If true, remote process is allowed to fetch new processes from the queue (this requires the remote processes to have UbaRequestNextProcess implemented)\n"));

	static bool bDetailedTrace = false;
	static FAutoConsoleVariableRef CVarDetailedTrace(
		TEXT("r.UbaController.DetailedTrace"),
		bDetailedTrace,
		TEXT("If true, a UBA will output detailed trace\n"));

	enum EUbaLogVerbosity
	{
		UbaLogVerbosity_Default = 0, // foward erros and warnings only
		UbaLogVerbosity_High, // also forward infos
		UbaLogVerbosity_Max // forward all UBA logs to UE_LOG
	};

	static int32 UbaLogVerbosity = UbaLogVerbosity_Default;
	static FAutoConsoleVariableRef CVarShowUbaLog(
		TEXT("r.UbaController.LogVerbosity"),
		UbaLogVerbosity,
		TEXT("Specifies how much of UBA logs is forwarded to UE logs..\n")
		TEXT("0 - Default, only forward errrors and warnings.\n")
		TEXT("1 - Also forward regular information about UBA sessions.\n")
		TEXT("2 - Forward all UBA logs."));

	static int32 SaveUbaTraceSnapshotInterval = 0;
	static FAutoConsoleVariableRef CVarSaveUbaTraceSnapshotInterval(
		TEXT("r.UbaController.SaveTraceSnapshotInterval"),
		SaveUbaTraceSnapshotInterval,
		TEXT("Specifies the interval (in seconds) in which a snapshot of the current state of the UBA trace will be saved to file.\n")
		TEXT("A value of 0 disables the periodic snapshots and only saves the UBA trace at the end of each server session. By default 0.\n"));

	static bool bProcessLogEnabled = false;
	static FAutoConsoleVariableRef CVarProcessLogEnabled(
		TEXT("r.UbaController.ProcessLogEnabled"),
		bProcessLogEnabled,
		TEXT("If true, each detoured process will write a log file. Note this is only useful if UBA is compiled in debug\n"));

	FString ReplaceEnvironmentVariablesInPath(const FString& ExtraFilePartialPath) // Duplicated code with FAST build.. put it somewhere else?
	{
		FString ParsedPath;

		// Fast build cannot read environmental variables easily
		// Is better to resolve them here
		if (ExtraFilePartialPath.Contains(TEXT("%")))
		{
			TArray<FString> PathSections;
			ExtraFilePartialPath.ParseIntoArray(PathSections, TEXT("/"));

			for (FString& Section : PathSections)
			{
				if (Section.Contains(TEXT("%")))
				{
					Section.RemoveFromStart(TEXT("%"));
					Section.RemoveFromEnd(TEXT("%"));
					Section = FPlatformMisc::GetEnvironmentVariable(*Section);
				}
			}

			for (FString& Section : PathSections)
			{
				ParsedPath /= Section;
			}

			FPaths::NormalizeDirectoryName(ParsedPath);
		}

		if (ParsedPath.IsEmpty())
		{
			ParsedPath = ExtraFilePartialPath;
		}

		return ParsedPath;
	}
}

FUbaJobProcessor::FUbaJobProcessor(
	FUbaControllerModule& InControllerModule) :

	Thread(nullptr),
	ControllerModule(InControllerModule),
	bForceStop(false),
	LastTimeCheckedForTasks(0),
	bIsWorkDone(false),
	LogWriter([]() {}, []() {}, [](uba::LogEntryType type, const uba::tchar* str, uba::u32 /*strlen*/)
	{
			switch (type)
			{
			case uba::LogEntryType_Error:
				UE_LOG(LogUbaController, Error, TEXT("%s"), UBASTRING_TO_TCHAR(str));
				break;
			case uba::LogEntryType_Warning:
				UE_LOG(LogUbaController, Warning, TEXT("%s"), UBASTRING_TO_TCHAR(str));
				break;
			case uba::LogEntryType_Info:
				if (UbaJobProcessorOptions::UbaLogVerbosity >= UbaJobProcessorOptions::UbaLogVerbosity_High)
				{
					UE_LOG(LogUbaController, Display, TEXT("%s"), UBASTRING_TO_TCHAR(str));
				}
				break;
			default:
				if (UbaJobProcessorOptions::UbaLogVerbosity >= UbaJobProcessorOptions::UbaLogVerbosity_Max)
				{
					UE_LOG(LogUbaController, Display, TEXT("%s"), UBASTRING_TO_TCHAR(str));
				}
				break;
			}
	})
{
	Uba_SetCustomAssertHandler([](const uba::tchar* text)
		{
			checkf(false, TEXT("%s"), UBASTRING_TO_TCHAR(text));
		});

	UpdateMaxLocalParallelJobs();
}

FUbaJobProcessor::~FUbaJobProcessor()
{
	delete Thread;
}

void FUbaJobProcessor::UpdateMaxLocalParallelJobs()
{
	// Limit number of parallel jobs by UBA/Horde configuration
	MaxLocalParallelJobs = FUbaHordeConfig::Get().MaxParallelActions;
	if (MaxLocalParallelJobs == -1)
	{
		MaxLocalParallelJobs = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
	}

	// Also apply limits from shader compiling manager, i.e. [DevOptions.Shaders]:NumUnusedShaderCompilingThreads etc.
	const int32 MaxNumLocalWorkers = ControllerModule.GetMaxNumLocalWorkers();
	if (MaxNumLocalWorkers != -1)
	{
		MaxLocalParallelJobs = FMath::Min(MaxLocalParallelJobs, MaxNumLocalWorkers);
	}
}

void FUbaJobProcessor::CalculateKnownInputs()
{
	// TODO: This is ShaderCompileWorker specific and this code is designed to handle all kinds of distributed workload.
	// Instead this information should be provided from the outside


	if (KnownInputsCount) // In order to improve startup we provide some of the input we know will be loaded by ShaderCompileWorker.exe
	{
		return;
	}

	auto AddKnownInput = [&](const FString& file)
		{
			#if PLATFORM_WINDOWS
			auto& fileData = file.GetCharArray();
			const uba::tchar* fileName = fileData.GetData();
			size_t fileNameLen = fileData.Num();
			#else
			FStringToUbaStringConversion conv(*file);
			const uba::tchar* fileName = conv.Get();
			size_t fileNameLen = strlen(fileName) + 1;
			#endif
			auto num = KnownInputsBuffer.Num();
			KnownInputsBuffer.SetNum(num + fileNameLen);
			memcpy(KnownInputsBuffer.GetData() + num, fileName, fileNameLen * sizeof(uba::tchar));
			++KnownInputsCount;
		};

	// Get the binaries
	TArray<FString> KnownFileNames;
	FString BinDir = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory());

	#if PLATFORM_WINDOWS
	AddKnownInput(*FPaths::Combine(BinDir, TEXT("ShaderCompileWorker.exe")));
	#else
	AddKnownInput(*FPaths::Combine(BinDir, TEXT("ShaderCompileWorker")));
	#endif

	IFileManager::Get().FindFilesRecursive(KnownFileNames, *BinDir, TEXT("ShaderCompileWorker-*.*"), true, false);
	for (const FString& file : KnownFileNames)
	{
		if (file.EndsWith(FPlatformProcess::GetModuleExtension()))
		{
			AddKnownInput(file);
		}
	}

	// Get the compiler dependencies for all platforms)
	ITargetPlatformManagerModule* TargetPlatformManager = GetTargetPlatformManager();
	for (ITargetPlatform* TargetPlatform : GetTargetPlatformManager()->GetTargetPlatforms())
	{
		KnownFileNames.Empty();
		TargetPlatform->GetShaderCompilerDependencies(KnownFileNames);

		for (const FString& ExtraFilePartialPath : KnownFileNames)
		{
			if (!ExtraFilePartialPath.Contains(TEXT("*"))) // Seems like there are some *.x paths in there.. TODO: Do a find files
			{
				AddKnownInput(UbaJobProcessorOptions::ReplaceEnvironmentVariablesInPath(ExtraFilePartialPath));
			}
		}
	}

	// Get all the config files
	for (const FString& ConfigDir : FPaths::GetExtensionDirs(FPaths::EngineDir(), TEXT("Config")))
	{
		KnownFileNames.Empty();
		IFileManager::Get().FindFilesRecursive(KnownFileNames, *ConfigDir, TEXT("*.ini"), true, false);
		for (const FString& file : KnownFileNames)
		{
			AddKnownInput(file);
		}
	}

	KnownInputsBuffer.Add(0);
}

void FUbaJobProcessor::RunTaskWithUba(FDistributedBuildTask* Task)
{
	FTaskCommandData& Data = Task->CommandData;
	SessionServer_RegisterNewFile(UbaSessionServer, TCHAR_TO_UBASTRING(*Data.InputFileName));

	for (const FString& AdditionalOutputFolder : Data.AdditionalOutputFolders)
	{
		SessionServer_RegisterNewDirectory(UbaSessionServer, TCHAR_TO_UBASTRING(*AdditionalOutputFolder));
	}

	FString InputFileName = FPaths::GetCleanFilename(Data.InputFileName);
	FString OutputFileName = FPaths::GetCleanFilename(Data.OutputFileName);
	FString Parameters = FString::Printf(TEXT("\"%s/\" %d 0 \"%s\" \"%s\" %s "), *Data.WorkingDirectory, Data.DispatcherPID, *InputFileName, *OutputFileName, *Data.ExtraCommandArgs);
	FString AppDir = FPaths::GetPath(Data.Command);
	
	struct ExitedInfo
	{
		FUbaJobProcessor* Processor;
		FString InputFile;
		FString OutputFile;
		FDistributedBuildTask* Task;
	};

	auto Info = new ExitedInfo;
	Info->Processor = this;
	Info->InputFile = Data.InputFileName;
	Info->OutputFile = Data.OutputFileName;
	Info->Task = Task;

	static auto ExitedFunc = [](void* UserData, const uba::ProcessHandle& Process)
		{
			for (uint32 LogLineIndex = 0; const uba::tchar* LogLine = ProcessHandle_GetLogLine(&Process, LogLineIndex); ++LogLineIndex)
			{
				UE_LOG(LogUbaController, Display, TEXT("%s"), UBASTRING_TO_TCHAR(LogLine));
			}

			if (ExitedInfo* Info = static_cast<ExitedInfo*>(UserData)) // It can be null if custom message has already handled all of them
			{
				IFileManager::Get().Delete(*Info->InputFile);
				SessionServer_RegisterDeleteFile(Info->Processor->UbaSessionServer, TCHAR_TO_UBASTRING(*Info->InputFile));
				Info->Processor->HandleUbaJobFinished(Info->Task);

				StorageServer_DeleteFile(Info->Processor->UbaStorageServer, TCHAR_TO_UBASTRING(*Info->InputFile));
				StorageServer_DeleteFile(Info->Processor->UbaStorageServer, TCHAR_TO_UBASTRING(*Info->OutputFile));

				delete Info;
			}
		};

	FStringToUbaStringConversion UbaInputFileNameStr(*InputFileName);

	uba::Config* Config = Config_Create();
	uba::ConfigTable* RootTable = Config_RootTable(*Config);
	ConfigTable_AddValueString(*RootTable, TC("Application"), FStringToUbaStringConversion(*Data.Command).Get());
	ConfigTable_AddValueString(*RootTable, TC("Arguments"), FStringToUbaStringConversion(*Parameters).Get());
	ConfigTable_AddValueString(*RootTable, TC("Description"), UbaInputFileNameStr.Get());
	ConfigTable_AddValueString(*RootTable, TC("WorkingDir"), FStringToUbaStringConversion(*AppDir).Get());
	ConfigTable_AddValueString(*RootTable, TC("Breadcrumbs"), FStringToUbaStringConversion(*Data.Description).Get());
	ConfigTable_AddValueBool(*RootTable,   TC("WriteOutputFilesOnFail"), true);

	if (UbaJobProcessorOptions::bProcessLogEnabled)
	{
		ConfigTable_AddValueString(*RootTable, TC("LogFile"), UbaInputFileNameStr.Get());
	}

	uba::ProcessStartInfo* StartInfo = ProcessStartInfo_Create3(*Config);
	ProcessStartInfo_SetExitedCallback(*StartInfo, ExitedFunc, Info);

	Scheduler_EnqueueProcess(UbaScheduler, *StartInfo, 1.0f, KnownInputsBuffer.GetData(), KnownInputsBuffer.Num()*sizeof(uba::tchar), KnownInputsCount);

	ProcessStartInfo_Destroy(StartInfo);
	Config_Destroy(Config);
}

FString GetUbaBinariesPath();

void FUbaJobProcessor::StartUba()
{
	FString TraceName = FString::Printf(TEXT("UbaController_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	UE_LOG(LogUbaController, Display, TEXT("Starting up UBA/Horde connection for session %s"), *TraceName);

	checkf(UbaServer == nullptr, TEXT("FUbaJobProcessor::StartUba() was called twice before FUbaJobProcessor::ShutDownUba()"));

	FString RootDir;
	int32 FolderIndex = 0;
	while (true)
	{
		RootDir = FString::Printf(TEXT("%s/%s/%u"), *FUbaControllerModule::GetTempDir(), TEXT("UbaControllerStorageDir"), FolderIndex);
		if (Uba_GetExclusiveAccess(TCHAR_TO_UBASTRING(*RootDir)))
			break;
		++FolderIndex;
	}
	IFileManager::Get().MakeDirectory(*RootDir, true);

	if (!ControllerModule.GetDebugInfoPath().IsEmpty())
	{
		static uint32 UbaSessionCounter;
		TraceOutputFilename = ControllerModule.GetDebugInfoPath() / FString::Printf(TEXT("UbaController.MultiprocessId-%u.Session-%u.uba"), UE::GetMultiprocessId(), UbaSessionCounter++);
	}

	uba::Config* Config = Config_Create();
	{
		uba::ConfigTable* RootTable = Config_RootTable(*Config);
		ConfigTable_AddValueString(*RootTable,     TC("RootDir"), TCHAR_TO_UBASTRING(*RootDir));

		uba::ConfigTable* StorageTable = Config_AddTable(*Config, TC("Storage"));
		ConfigTable_AddValueU64(*StorageTable,     TC("CasCapacityBytes"), 32llu * 1024 * 1024 * 1024);

		uba::ConfigTable* SessionTable = Config_AddTable(*Config, TC("Session"));
		ConfigTable_AddValueBool(*SessionTable,    TC("LaunchVisualizer"), UbaJobProcessorOptions::bAutoLaunchVisualizer);
		ConfigTable_AddValueBool(*SessionTable,    TC("AllowMemoryMaps"), false); // Skip using memory maps
		ConfigTable_AddValueBool(*SessionTable,    TC("RemoteLogEnabled"), UbaJobProcessorOptions::bProcessLogEnabled);
		ConfigTable_AddValueBool(*SessionTable,    TC("TraceEnabled"), true);
		ConfigTable_AddValueString(*SessionTable,  TC("TraceOutputFile"), TCHAR_TO_UBASTRING(*TraceOutputFilename));
		ConfigTable_AddValueBool(*SessionTable,    TC("DetailedTrace"), UbaJobProcessorOptions::bDetailedTrace);
		ConfigTable_AddValueString(*SessionTable,  TC("TraceName"), TCHAR_TO_UBASTRING(*TraceName));
		// ConfigTable_AddValueBool(*SessionTable,    TC("RemoteTraceEnabled"), true); Enable this to have the remotes send back the uba trace to the host (ends up in log folder)

		uba::ConfigTable* SchedulerTable = Config_AddTable(*Config, TC("Scheduler"));
		ConfigTable_AddValueU32(*SchedulerTable,   TC("MaxLocalProcessors"), MaxLocalParallelJobs);
		ConfigTable_AddValueBool(*SchedulerTable,  TC("EnableProcessReuse"), UbaJobProcessorOptions::bAllowProcessReuse);
	}


	UbaServer = NetworkServer_Create(LogWriter);
	UbaStorageServer = StorageServer_Create2(*UbaServer, *Config, LogWriter);
	UbaSessionServer = SessionServer_Create2(*UbaStorageServer, *UbaServer, *Config, LogWriter);
	UbaScheduler = Scheduler_Create2(*UbaSessionServer, *Config);

	Config_Destroy(Config);

	// Config used by clients that connect
	{
		uba::Config* ClientConfig = Config_Create();
		uba::ConfigTable* StorageTable = Config_AddTable(*ClientConfig, TC("Storage"));
		ConfigTable_AddValueBool(*StorageTable,  TC("ResendCas"), true); // Since we call StorageServer_DeleteFile there is a tiny risk we might delete a cas file that is needed in the future
		NetworkServer_SetClientsConfig(UbaServer, *ClientConfig);
		Config_Destroy(ClientConfig);
	}

	CalculateKnownInputs();
	UpdateMaxLocalParallelJobs();

	Scheduler_Start(UbaScheduler);

	if (FolderIndex == 0)
	{
		NetworkServer_StartListen(UbaServer, uba::DefaultPort, nullptr); // Start listen so any helper on the LAN can join in
	}

	// Only request Horde agents if Horde is enabled for UBA
	if (FUbaHordeConfig::Get().bIsProviderEnabled)
	{
		SessionServer_UpdateStatus(UbaSessionServer, 0, 1, TC("Horde"), uba::LogEntryType_Info, nullptr);
		SessionServer_UpdateStatus(UbaSessionServer, 0, 6, TC("Starting"), uba::LogEntryType_Info, nullptr);
		HordeAgentManager = MakeUnique<FUbaHordeAgentManager>(ControllerModule.GetWorkingDirectory(), GetUbaBinariesPath());

		HordeAgentManager->SetAddClientCallback([](void* UserData, const uba::tchar* Ip, uint16 Port, const uba::tchar* Crypto16Characters)
			{
				return NetworkServer_AddClient(reinterpret_cast<uba::NetworkServer*>(UserData), Ip, Port, Crypto16Characters);
			}, UbaServer);

		HordeAgentManager->SetUpdateStatusCallback([](void* UserData, const TCHAR* Status)
			{
				SessionServer_UpdateStatus(static_cast<uba::SessionServer*>(UserData), 0, 6, TCHAR_TO_UBASTRING(Status), uba::LogEntryType_Info, nullptr);
			}, UbaSessionServer);
	}

	UE_LOG(LogUbaController, Display, TEXT("Created UBA storage server: RootDir=%s"), *RootDir);
}

void FUbaJobProcessor::ShutDownUba()
{
	UE_LOG(LogUbaController, Display, TEXT("Shutting down UBA/Horde connection"));

	HordeAgentManager = nullptr;

	if (UbaSessionServer == nullptr)
	{
		return;
	}

	NetworkServer_Stop(UbaServer);

	Scheduler_Destroy(UbaScheduler);
	SessionServer_Destroy(UbaSessionServer);
	StorageServer_Destroy(UbaStorageServer);
	NetworkServer_Destroy(UbaServer);

	UbaScheduler = nullptr;
	UbaSessionServer = nullptr;
	UbaStorageServer = nullptr;
	UbaServer = nullptr;
}

uint32 FUbaJobProcessor::Run()
{
	bIsWorkDone = false;
	
	const double CurrentTime = FPlatformTime::Seconds();
	double LastTimeSinceHadJobs = CurrentTime;
	double LastHeartBeat = CurrentTime;
	double LastTraceSnapshot = CurrentTime;

	while (!bForceStop)
	{
		const double ElapsedSeconds = (FPlatformTime::Seconds() - LastTimeSinceHadJobs);

		uint32 NumQueuedJobs = 0;
		uint32 NumActiveLocalJobs = 0;
		uint32 NumActiveRemoteJobs = 0;
		uint32 NumFinishedJobs = 0;
		bool bNewTasks = !ControllerModule.PendingRequestedCompilationTasks.IsEmpty();
		// never empty if we have new tasks
		bool bIsEmpty = !bNewTasks;

		const double HeartBeatElapsedSeconds = (FPlatformTime::Seconds() - LastHeartBeat);

		if (UbaScheduler)
		{
			Scheduler_GetStats(UbaScheduler, NumQueuedJobs, NumActiveLocalJobs, NumActiveRemoteJobs, NumFinishedJobs);
			bIsEmpty &= Scheduler_IsEmpty(UbaScheduler);
		}
		const uint32 NumActiveJobs = NumActiveLocalJobs + NumActiveRemoteJobs;

		// We don't want to hog up Horde resources.
		if (UbaScheduler && bIsEmpty && ElapsedSeconds > UbaJobProcessorOptions::MaxTimeWithoutTasks)
		{
			// If we're optimizing job starting, we only want to shutdown UBA if all the processes have terminated
			ShutDownUba();
		}

		// Check if we have new tasks to process
		if (!bIsEmpty)
		{
			if (!UbaScheduler)
			{
				// We have new tasks. Start processing again
				StartUba();
			}

			LastTimeSinceHadJobs = FPlatformTime::Seconds();
		}

		if (UbaScheduler)
		{
			if (bNewTasks)
			{
				while (true)
				{
					FDistributedBuildTask* Task = nullptr;
					if (!ControllerModule.PendingRequestedCompilationTasks.Dequeue(Task) || !Task)
					{
						break;
					}
					RunTaskWithUba(Task);
				}
			}

			const int32 MaxAvailableLocalCores = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
			const int32 LocalCoresToNotUse = 1 + (int32)NumActiveRemoteJobs / 30; // Use one core per 30 remote ones
			const int32 MaxLocalCoresToUse = FMath::Clamp(MaxAvailableLocalCores - LocalCoresToNotUse, 0, MaxLocalParallelJobs);
			const int32 MaxRemoteCoresToRequest = FMath::Max(0, (int32)(NumQueuedJobs + NumActiveJobs) - MaxLocalCoresToUse);

			Scheduler_SetMaxLocalProcessors(UbaScheduler, MaxLocalCoresToUse);

			if (HordeAgentManager)
			{
				HordeAgentManager->SetTargetCoreCount(MaxRemoteCoresToRequest);
			}
			
			// TODO: Not sure this is a good idea in a cooking scenario where number of queued processes are going up and down
			SessionServer_SetMaxRemoteProcessCount(UbaSessionServer, MaxRemoteCoresToRequest);

			UpdateStats();

			if (HeartBeatElapsedSeconds > UbaJobProcessorOptions::HeartBeatInterval)
			{
				// only print heartbeat log messages if currently executing tasks
				UE_LOG(LogUbaController, Display, TEXT("Task Status -- Queued: %u -- Active: %u local, %u remote -- Completed: %u"), NumQueuedJobs, NumActiveLocalJobs, NumActiveRemoteJobs, NumFinishedJobs);
				LastHeartBeat = FPlatformTime::Seconds();
			}

			// Save snapshot of current trace if periodic saves are enabled
			if (UbaJobProcessorOptions::SaveUbaTraceSnapshotInterval > 0 && UbaSessionServer)
			{
				const int32 SnapshotIntervalElapsedSeconds = (int32)(FPlatformTime::Seconds() - LastTraceSnapshot);
				if (SnapshotIntervalElapsedSeconds >= UbaJobProcessorOptions::SaveUbaTraceSnapshotInterval)
				{
					SaveSnapshotOfTrace();
					LastTraceSnapshot = FPlatformTime::Seconds();
				}
			}
		}

		FPlatformProcess::Sleep(UbaJobProcessorOptions::SleepTimeBetweenActions);
	}

	ShutDownUba();

	bIsWorkDone = true;
	return 0;
}

void FUbaJobProcessor::Stop()
{
	bForceStop = true;
};

void FUbaJobProcessor::StartThread()
{
	Thread = FRunnableThread::Create(this, TEXT("UbaJobProcessor"), 0, TPri_SlightlyBelowNormal, FPlatformAffinity::GetPoolThreadMask());
}

bool FUbaJobProcessor::ProcessOutputFile(FDistributedBuildTask* CompileTask)
{
	//TODO: This method is mostly taken from the other Distribution controllers
	// As we get an explicit callback when the process ends, we should be able to simplify this to just check if the file exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileManager& FileManager = IFileManager::Get();

	constexpr uint64 VersionAndFileSizeSize = sizeof(uint32) + sizeof(uint64);	
	if (ensure(CompileTask) && 
		PlatformFile.FileExists(*CompileTask->CommandData.OutputFileName) &&
		FileManager.FileSize(*CompileTask->CommandData.OutputFileName) > VersionAndFileSizeSize)
	{
		const TUniquePtr<FArchive> OutputFilePtr(FileManager.CreateFileReader(*CompileTask->CommandData.OutputFileName, FILEREAD_Silent));
		if (ensure(OutputFilePtr))
		{
			FArchive& OutputFile = *OutputFilePtr;
			int32 OutputVersion;
			OutputFile << OutputVersion; // NOTE (SB): Do not care right now about the version.
			int64 FileSize = 0;
			OutputFile << FileSize;

			// NOTE (SB): Check if we received the full file yet.
			if (ensure(OutputFile.TotalSize() >= FileSize))
			{
				FTaskResponse TaskCompleted;
				TaskCompleted.ID = CompileTask->ID;
				TaskCompleted.ReturnCode = 0;
						
				ControllerModule.ReportJobProcessed(TaskCompleted, CompileTask);
			}
			else
			{
				UE_LOG(LogUbaController, Error, TEXT("Output file size is not correct [%s] | Expected Size [%lld] : => Actual Size : [%lld]"), *CompileTask->CommandData.OutputFileName, OutputFile.TotalSize(), FileSize);
				return false;
			}
		}
		else
		{
			UE_LOG(LogUbaController, Error, TEXT("Failed open for read Output File [%s]"), *CompileTask->CommandData.OutputFileName);
			return false;
		}
	}
	else
	{
		// Error: Output file was expected but is missing or invalid. Print some diagnostics about disk space and the folder content.
		if (CompileTask != nullptr)
		{
			TStringBuilder<1024> DiagnosticsInfo;

			// Append diagnostics about output directory
			const FString OutputFileDirectory = FPaths::GetPath(CompileTask->CommandData.OutputFileName);
			if (FPaths::DirectoryExists(OutputFileDirectory))
			{
				int32 NumFilesInDirectory = 0;
				IFileManager::Get().IterateDirectory(
					*OutputFileDirectory,
					[&NumFilesInDirectory](const TCHAR* InName, bool bIsDirectory) -> bool
					{
						if (!bIsDirectory)
						{
							++NumFilesInDirectory;
						}
						return true;
					}
				);
				DiagnosticsInfo.Appendf(TEXT("\n - Directory \"%s\" exists and contains %d file(s)"), *OutputFileDirectory, NumFilesInDirectory);
			}
			else
			{
				DiagnosticsInfo.Appendf(TEXT("\n - Directory \"%s\" does not exist"), *OutputFileDirectory);
			}

			// Append diagnostics about disk space
			uint64 TotalNumberOfBytes = 0;
			uint64 NumberOfFreeBytes = 0;
			if (FPlatformMisc::GetDiskTotalAndFreeSpace(OutputFileDirectory, TotalNumberOfBytes, NumberOfFreeBytes))
			{
				DiagnosticsInfo.Appendf(TEXT("\n - Disk space: %lld MiB, free %lld MiB"), (long long int)TotalNumberOfBytes >> 20, (long long int)NumberOfFreeBytes >> 20);
			}

			UE_LOG(LogUbaController, Display, TEXT("Distributed job output file [%s] is invalid or does not exist%s"), *CompileTask->CommandData.OutputFileName, *DiagnosticsInfo);
		}
		else
		{
			UE_LOG(LogUbaController, Error, TEXT("Invalid CompileTask, cannot retrieve name"));
		}
		return false;
	}

	return true;
}

void FUbaJobProcessor::HandleUbaJobFinished(FDistributedBuildTask* CompileTask)
{
	const bool bWasSuccessful = ProcessOutputFile(CompileTask);
	if (!bWasSuccessful)
	{
		// Save current snapshot of UBA trace in case this failure crashes the cook later on
		SaveSnapshotOfTrace();

		// If it failed running locally, lets try Run it locally but outside Uba
		// Signaling a jobs as complete when it wasn't done, will cause a rerun on local worker as fallback
		// because there is not output file for this job

		FTaskResponse TaskCompleted;
		TaskCompleted.ID = CompileTask->ID;
		TaskCompleted.ReturnCode = 0;
		ControllerModule.ReportJobProcessed(TaskCompleted, CompileTask);
	}
}

bool FUbaJobProcessor::HasJobsInFlight() const
{
	if (!UbaScheduler)
	{
		return false;
	}

	return !Scheduler_IsEmpty(UbaScheduler);
}

bool FUbaJobProcessor::PollStats(FDistributedBuildStats& OutStats)
{
	// Return current stats and reset internal data
	FScopeLock StatsLockGuard(&StatsLock);
	OutStats = Stats;
	Stats = FDistributedBuildStats();
	return true;
}

void FUbaJobProcessor::UpdateStats()
{
	if (HordeAgentManager)
	{
		FScopeLock StatsLockGuard(&StatsLock);

		// Update maximum
		Stats.MaxRemoteAgents = FMath::Max(Stats.MaxRemoteAgents, (uint32)HordeAgentManager->GetAgentCount());
		Stats.MaxActiveAgentCores = FMath::Max(Stats.MaxActiveAgentCores, HordeAgentManager->GetActiveCoreCount());
	}
}

void FUbaJobProcessor::SaveSnapshotOfTrace()
{
	if (!TraceOutputFilename.IsEmpty())
	{
		UE_LOG(LogUbaController, Log, TEXT("Save snapshot of UBA trace: %s"), *TraceOutputFilename);
		SessionServer_SaveSnapshotOfTrace(UbaSessionServer);
	}
}
