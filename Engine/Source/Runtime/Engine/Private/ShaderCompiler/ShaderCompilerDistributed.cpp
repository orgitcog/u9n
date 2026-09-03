// Copyright Epic Games, Inc. All Rights Reserved.

#include "DistributedBuildControllerInterface.h"
#include "Async/Future.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/StringBuilder.h"
#include "ShaderCompiler.h"
#include "ShaderCompilerJobTypes.h"

namespace DistributedShaderCompilerVariables
{
	//TODO: Remove the XGE doublet
	int32 MinBatchSize = 50;
	FAutoConsoleVariableRef CVarXGEShaderCompileMinBatchSize(
        TEXT("r.XGEShaderCompile.MinBatchSize"),
        MinBatchSize,
        TEXT("This CVar is deprecated, please use r.ShaderCompiler.DistributedMinBatchSize"),
        ECVF_Default);

	FAutoConsoleVariableRef CVarDistributedMinBatchSize(
		TEXT("r.ShaderCompiler.DistributedMinBatchSize"),
		MinBatchSize,
		TEXT("Minimum number of shaders to compile with a distributed controller.\n")
		TEXT("Smaller number of shaders will compile locally."),
		ECVF_Default);

	static int32 GDistributedJobDescriptionLevel = 1;
	static FAutoConsoleVariableRef CVarDistributedJobDescriptionLevel(
		TEXT("r.ShaderCompiler.DistributedJobDescriptionLevel"),
		GDistributedJobDescriptionLevel,
		TEXT("Sets the level of descriptive details for each distributed job batch. The following modes are supported:\n")
		TEXT(" Mode 0: Disabled.\n")
		TEXT(" Mode 1: Basic information of the first 20 compile jobs per batch (Default).\n")
		TEXT(" Mode 2: Additional information of the shader format per compile job.\n")
		TEXT("This will show up in the UBA trace files. By default 0.")
	);

}

extern bool GShaderCompilerDumpWorkerDiagnostics;

bool FShaderCompileDistributedThreadRunnable_Interface::IsSupported()
{
	//TODO Handle Generic response
	return true;
}

class FDistributedShaderCompilerTask
{
public:
	TFuture<FDistributedBuildTaskResult> Future;
	TArray<FShaderCommonCompileJobPtr> ShaderJobs;
	FString InputFilePath;
	FString OutputFilePath;

	FDistributedShaderCompilerTask(TFuture<FDistributedBuildTaskResult>&& Future,TArray<FShaderCommonCompileJobPtr>&& ShaderJobs, FString&& InputFilePath, FString&& OutputFilePath)
		: Future(MoveTemp(Future))
		, ShaderJobs(MoveTemp(ShaderJobs))
		, InputFilePath(MoveTemp(InputFilePath))
		, OutputFilePath(MoveTemp(OutputFilePath))
	{}
};

/** Initialization constructor. */
FShaderCompileDistributedThreadRunnable_Interface::FShaderCompileDistributedThreadRunnable_Interface(class FShaderCompilingManager* InManager, IDistributedBuildController& InController)
	: FShaderCompileThreadRunnableBase(InManager)
	, NumDispatchedJobs(0)
	, bIsHung(false)
	, CachedController(InController)
{
}

FShaderCompileDistributedThreadRunnable_Interface::~FShaderCompileDistributedThreadRunnable_Interface()
{
}

static FString BuildCompactTaskDescriptionForJob(const FShaderCommonCompileJob& Job)
{
	FString JobDescription;
	if (const FShaderCompileJob* SingleJob = Job.GetSingleShaderJob())
	{
		JobDescription = SingleJob->Input.DebugGroupName;
		if (DistributedShaderCompilerVariables::GDistributedJobDescriptionLevel >= 2)
		{
			JobDescription += FString::Printf(TEXT("(%s)"), *SingleJob->Input.ShaderFormat.ToString());
		}
	}
	else if (const FShaderPipelineCompileJob* PipelineJob = Job.GetShaderPipelineJob())
	{
		JobDescription = TEXT("Stages:");
		for (int32 StageJobIndex = 0; StageJobIndex < PipelineJob->StageJobs.Num(); ++StageJobIndex)
		{
			if (StageJobIndex > 0)
			{
				JobDescription += TEXT(",");
			}
			JobDescription += BuildCompactTaskDescriptionForJob(*PipelineJob->StageJobs[StageJobIndex]);
		}
	}
	return JobDescription;
}

// Builds a compact description of the shader compile task that will show up in UBA trace files for instance.
// It shall contain a brief summary of the shaders being compiled to diagnose issues with overly long remote jobs.
static FString BuildCompactTaskDescription(const TArray<FShaderCommonCompileJobPtr>& JobsToSerialize)
{
	FString Description;

	if (!JobsToSerialize.IsEmpty())
	{
		constexpr int32 MaxNumJobsInDescription = 20;
		const int32 NumJobsInDescription = JobsToSerialize.Num() > MaxNumJobsInDescription ? MaxNumJobsInDescription - 1 : JobsToSerialize.Num();
		for (int32 JobIndex = 0; JobIndex < NumJobsInDescription; ++JobIndex)
		{
			if (JobIndex > 0)
			{
				Description += TEXT("\n");
			}
			Description += BuildCompactTaskDescriptionForJob(*JobsToSerialize[JobIndex]);
		}
		if (JobsToSerialize.Num() > NumJobsInDescription)
		{
			Description += FString::Printf(TEXT("\n%d more shaders ...\n"), JobsToSerialize.Num() - NumJobsInDescription);
		}
	}

	return Description;
}

static bool IsCharValidForFilename(TCHAR Ch)
{
	const TCHAR* ValidSpecialChars = TEXT("_-+()[]");
	return TChar<TCHAR>::IsAlnum(Ch) || FCString::Strchr(ValidSpecialChars, Ch) != nullptr;
}

static void ConvertDebugNameToFilename(TStringBuilder<1024>& StringBuilder, const FString& Name)
{
	for (TCHAR Ch : Name)
	{
		StringBuilder << (IsCharValidForFilename(Ch) ? Ch : TEXT('-'));
	}
}

static void BuildDescriptiveTaskFilename(TStringBuilder<1024>& StringBuilder, const TArray<FShaderCommonCompileJobPtr>& JobsToSerialize, int32 BasePathLen, int32 PathLimit)
{
	StringBuilder << TEXT(".");

	if (JobsToSerialize.Num() == 1)
	{
		// Decorate filename with single job description
		if (const FShaderCompileJob* FirstSingleJob = JobsToSerialize[0]->GetSingleShaderJob())
		{
			const int32 BaseFilenameLen = BasePathLen + StringBuilder.Len();
			const int32 TotalFilenameLen = BaseFilenameLen + FirstSingleJob->Input.DebugGroupName.Len();
			if (TotalFilenameLen > PathLimit)
			{
				const int32 RemainingLen = PathLimit - BaseFilenameLen;

				// If even a shortened task filename with just 5 characters were to exceed the path limit,
				// don't bother trying to construct a descriptive task name, just fall back to default.
				constexpr int32 kMinFilenameSuffixLen = 5;
				if (RemainingLen >= kMinFilenameSuffixLen)
				{
					const int32 RemainingLenHalfPart = (RemainingLen - 3) / 2;
					FString ShortenedDebugGroupName = FString::Printf(TEXT("%s---%s"), *FirstSingleJob->Input.DebugGroupName.Left(RemainingLenHalfPart), *FirstSingleJob->Input.DebugGroupName.Right(RemainingLenHalfPart));
					ConvertDebugNameToFilename(StringBuilder, ShortenedDebugGroupName);
					return;
				}
			}
			else
			{
				ConvertDebugNameToFilename(StringBuilder, FirstSingleJob->Input.DebugGroupName);
				return;
			}
		}
	}

	// Decorate filename with number of jobs
	StringBuilder << TEXT("j-") << JobsToSerialize.Num();
}

void FShaderCompileDistributedThreadRunnable_Interface::DispatchShaderCompileJobsBatch(TArray<FShaderCommonCompileJobPtr>& JobsToSerialize)
{
	// Generate unique filename for shader compiler I/O files
	FString BaseFilePath = CachedController.CreateUniqueFilePath();

	constexpr int32 kMaxFilePathSuffixLenWithNullChar = 5; // for ".out\0"
	const int32 PathLimit = FPlatformMisc::GetExternalAppMaxPathLength() - kMaxFilePathSuffixLenWithNullChar;
	if (BaseFilePath.Len() > PathLimit)
	{
		UE_LOG(LogShaderCompilers, Error, TEXT("Distributed job batch exceeded path limit: Length=%d, Limit=%d, Path=%s"), BaseFilePath.Len(), PathLimit, *BaseFilePath);
	}

	TStringBuilder<1024> BaseFilePathBuilder;
	BuildDescriptiveTaskFilename(BaseFilePathBuilder, JobsToSerialize, BaseFilePath.Len(), PathLimit);
	BaseFilePath += BaseFilePathBuilder.ToView();

	// Ensure the constructed input and output filenames don't exceed platform path limit
	if (BaseFilePath.Len() > PathLimit)
	{
		BaseFilePath.LeftChopInline(BaseFilePath.Len() - PathLimit);
	}

	FString InputFilePath = BaseFilePath + TEXT(".in");
	FString OutputFilePath = BaseFilePath + TEXT(".out");

	// Set up remote task
	const FString WorkingDirectory = FPaths::GetPath(InputFilePath);

	// Serialize the jobs to the input file
	GShaderCompilerStats->RegisterJobBatch(JobsToSerialize.Num(), FShaderCompilerStats::EExecutionType::Distributed);
	FArchive* InputFileAr = IFileManager::Get().CreateFileWriter(*InputFilePath, FILEWRITE_EvenIfReadOnly | FILEWRITE_NoFail);
	FShaderCompileUtilities::DoWriteTasks(JobsToSerialize, *InputFileAr, &CachedController, CachedController.RequiresRelativePaths());
	delete InputFileAr;

	// Kick off the job
	NumDispatchedJobs += JobsToSerialize.Num();

	FTaskCommandData TaskCommandData;
	TaskCommandData.Command = Manager->ShaderCompileWorkerName;
	TaskCommandData.WorkingDirectory = WorkingDirectory;
	TaskCommandData.DispatcherPID = Manager->ProcessId;
	TaskCommandData.InputFileName = InputFilePath;
	TaskCommandData.OutputFileName = OutputFilePath;
	TStringBuilder<64> SubprocessCommandLine;
	FCommandLine::BuildSubprocessCommandLine(ECommandLineArgumentFlags::ProgramContext, false /*bOnlyInherited*/, SubprocessCommandLine);
	TaskCommandData.ExtraCommandArgs = FString::Printf(TEXT("%s%s"), *SubprocessCommandLine, GIsBuildMachine ? TEXT(" -buildmachine") : TEXT(""));
	TaskCommandData.Dependencies = GetDependencyFilesForJobs(JobsToSerialize);

	// Register any debug info paths that may be written to as additional output folders
	// Without this remote tasks can incorrectly report that the debug info paths do not exist
	for (const FShaderCommonCompileJobPtr& Job : JobsToSerialize)
	{
		Job->ForEachSingleShaderJob([&TaskCommandData](FShaderCompileJob& SingleJob)
			{
				if (SingleJob.Input.DumpDebugInfoEnabled())
				{
					TaskCommandData.AdditionalOutputFolders.Add(SingleJob.Input.DumpDebugInfoPath);
				};
			});
	}

	if (DistributedShaderCompilerVariables::GDistributedJobDescriptionLevel > 0)
	{
		TaskCommandData.Description = BuildCompactTaskDescription(JobsToSerialize);
	}
	
	DispatchedTasks.Add(
		new FDistributedShaderCompilerTask(
			CachedController.EnqueueTask(TaskCommandData),
			MoveTemp(JobsToSerialize),
			MoveTemp(InputFilePath),
			MoveTemp(OutputFilePath)
		)
	);

	FDistributedBuildStats Stats;
	if (CachedController.PollStats(Stats))
	{
		GShaderCompilerStats->RegisterDistributedBuildStats(Stats);
	}
}

TArray<FString> FShaderCompileDistributedThreadRunnable_Interface::GetDependencyFilesForJobs(
	TArray<FShaderCommonCompileJobPtr>& Jobs)
{
	TArray<FString> Dependencies;
	TBitArray<> ShaderPlatformMask;
	ShaderPlatformMask.Init(false, EShaderPlatform::SP_NumPlatforms);
	for (const FShaderCommonCompileJobPtr& Job : Jobs)
	{
		EShaderPlatform ShaderPlatform = EShaderPlatform::SP_PCD3D_SM5;
		const FShaderCompileJob* ShaderJob = Job->GetSingleShaderJob();
		if (ShaderJob)
		{
			ShaderPlatform = ShaderJob->Input.Target.GetPlatform();
			// Add the source shader file and its dependencies.
			AddShaderSourceFileEntry(Dependencies, ShaderJob->Input.VirtualSourceFilePath, ShaderPlatform);
		}
		else
		{
			const FShaderPipelineCompileJob* PipelineJob = Job->GetShaderPipelineJob();
			if (PipelineJob)
			{
				for (const TRefCountPtr<FShaderCompileJob>& CommonCompileJob : PipelineJob->StageJobs)
				{
					if (const FShaderCompileJob* SingleShaderJob = CommonCompileJob->GetSingleShaderJob())
					{
						ShaderPlatform = SingleShaderJob->Input.Target.GetPlatform();
						// Add the source shader file and its dependencies.
						AddShaderSourceFileEntry(Dependencies, SingleShaderJob->Input.VirtualSourceFilePath, ShaderPlatform);
					}
				}
			}
			else
			{
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Unknown shader compilation job type."));
			}
		}
		// Add base dependencies for the platform only once.
		if (!(ShaderPlatformMask[(int)ShaderPlatform]))
		{
			ShaderPlatformMask[(int)ShaderPlatform] = true;
			TArray<FString>& ShaderPlatformCacheEntry = PlatformShaderInputFilesCache.FindOrAdd(ShaderPlatform);
			if (!ShaderPlatformCacheEntry.Num())
			{
				GetAllVirtualShaderSourcePaths(ShaderPlatformCacheEntry, ShaderPlatform);
			}
			if (Dependencies.Num())
			{
				for (const FString& Filename : ShaderPlatformCacheEntry)
				{
					Dependencies.AddUnique(Filename);
				}
			}
			else
			{
				Dependencies = ShaderPlatformCacheEntry;
			}
		}
	}

	return Dependencies;
}

static void LogShaderCompileWorkerDistributedDiagnostics(const TSparseArray<FDistributedShaderCompilerTask*>& InDispatchedTasks)
{
	UE_LOG(LogShaderCompilers, Display, TEXT("======= ShaderCompileWorker-Distributed Diagnostics ======="));

	FString JobDiagnostics;

	int32 TaskIndex = 0;
	for (auto Iter = InDispatchedTasks.CreateConstIterator(); Iter; ++Iter)
	{
		FDistributedShaderCompilerTask* Task = *Iter;
		checkf(Task != nullptr, TEXT("Task entries in the sparse array of dispatched distributed shader jobs (FDistributedShaderCompilerTask) must not be null"));
		if (!Task->ShaderJobs.IsEmpty())
		{
			JobDiagnostics.Empty();

			for (int32 JobIndex = 0; JobIndex < Task->ShaderJobs.Num(); ++JobIndex)
			{
				Task->ShaderJobs[JobIndex]->AppendDiagnostics(JobDiagnostics, JobIndex, Task->ShaderJobs.Num(), TEXT("  "));
			}

			UE_LOG(
				LogShaderCompilers, Display, TEXT("Task [%d/%d]:\n%s"),
				TaskIndex + 1, InDispatchedTasks.Num(), *JobDiagnostics
			);
		}
		++TaskIndex;
	}
}

int32 FShaderCompileDistributedThreadRunnable_Interface::CompilingLoop()
{
	TArray<FShaderCommonCompileJobPtr> PendingJobs;
	//if (LIKELY(!bIsHung))	// stop accepting jobs if we're hung - TODO: re-enable this after lockup detection logic is proved reliable and/or we have job resubmission in place
	{
		FScopeLock Lock(&Manager->CompileQueueSection);
		for (int32 PriorityIndex = MaxPriorityIndex; PriorityIndex >= MinPriorityIndex; --PriorityIndex)
		{
			// Grab as many jobs from the job queue as we can, unless there is no local shader compiling thread to pick up smaller batches
			const EShaderCompileJobPriority Priority = (EShaderCompileJobPriority)PriorityIndex;
			const int32 MinBatchSize = (Priority == EShaderCompileJobPriority::Low || Manager->IsExclusiveDistributedCompilingEnabled()) ? 1 : DistributedShaderCompilerVariables::MinBatchSize;
			const int32 NumJobs = Manager->AllJobs.GetPendingJobs(EShaderCompilerWorkerType::Distributed, Priority, MinBatchSize, INT32_MAX, PendingJobs);
			if (NumJobs > 0)
			{
				UE_LOG(LogShaderCompilers, Verbose, TEXT("Started %d 'Distributed' shader compile jobs with '%s' priority"),
					NumJobs,
					ShaderCompileJobPriorityToString((EShaderCompileJobPriority)PriorityIndex));
			}
			if (PendingJobs.Num() >= DistributedShaderCompilerVariables::MinBatchSize)
			{
				break;
			}
		}
	}

	if (PendingJobs.Num() > 0)
	{
		// Increase the batch size when more jobs are queued/in flight.

		// Build farm is much more prone to pool oversubscription, so make sure the jobs are submitted in batches of at least MinBatchSize
		int MinJobsPerBatch = GIsBuildMachine ? DistributedShaderCompilerVariables::MinBatchSize : 1;

		// Just to provide typical numbers: the number of total jobs is usually in tens of thousands at most, oftentimes in low thousands. Thus JobsPerBatch when calculated as a log2 rarely reaches the value of 16,
		// and that seems to be a sweet spot: lowering it does not result in faster completion, while increasing the number of jobs per batch slows it down.
		const uint32 JobsPerBatch = FMath::Max(MinJobsPerBatch, FMath::FloorToInt(FMath::LogX(2.f, PendingJobs.Num() + NumDispatchedJobs)));
		UE_LOG(LogShaderCompilers, Log, TEXT("Current jobs: %d, Batch size: %d, Num Already Dispatched: %d"), PendingJobs.Num(), JobsPerBatch, NumDispatchedJobs);


		struct FJobBatch
		{
			TArray<FShaderCommonCompileJobPtr> Jobs;
			TSet<const FShaderType*> UniquePointers;

			bool operator == (const FJobBatch& B) const
			{
				return Jobs == B.Jobs;
			}
		};


		// Different batches.
		TArray<FJobBatch> JobBatches;


		for (int32 i = 0; i < PendingJobs.Num(); i++)
		{
			if (PendingJobs[i]->Priority > EShaderCompileJobPriority::High)
			{
				// Submit single job immediately if it has a higher priority than the default
				TArray<FShaderCommonCompileJobPtr> SingleJobArray{ PendingJobs[i] };
				DispatchShaderCompileJobsBatch(SingleJobArray);
				continue;
			}

			// Avoid to have multiple of permutation of same global shader in same batch, to avoid pending on long shader compilation
			// of batches that tries to compile permutation of a global shader type that is giving a hard time to the shader compiler.
			const FShaderType* OptionalUniqueShaderType = nullptr;
			if (FShaderCompileJob* ShaderCompileJob = PendingJobs[i]->GetSingleShaderJob())
			{
				if (ShaderCompileJob->Key.ShaderType->GetGlobalShaderType())
				{
					OptionalUniqueShaderType = ShaderCompileJob->Key.ShaderType;
				}
			}

			// Find a batch this compile job can be packed with.
			FJobBatch* SelectedJobBatch = nullptr;
			{
				if (JobBatches.Num() == 0)
				{
					SelectedJobBatch = &JobBatches[JobBatches.Emplace()];
				}
				else if (OptionalUniqueShaderType)
				{
					for (FJobBatch& PendingJobBatch : JobBatches)
					{
						if (!PendingJobBatch.UniquePointers.Contains(OptionalUniqueShaderType))
						{
							SelectedJobBatch = &PendingJobBatch;
							break;
						}
					}

					if (!SelectedJobBatch)
					{
						SelectedJobBatch = &JobBatches[JobBatches.Emplace()];
					}
				}
				else
				{
					SelectedJobBatch = &JobBatches[0];
				}
			}

			// Assign compile job to job batch.
			{
				SelectedJobBatch->Jobs.Add(PendingJobs[i]);
				if (OptionalUniqueShaderType)
				{
					SelectedJobBatch->UniquePointers.Add(OptionalUniqueShaderType);
				}
			}

			// Kick off compile job batch.
			if (SelectedJobBatch->Jobs.Num() == JobsPerBatch)
			{
				DispatchShaderCompileJobsBatch(SelectedJobBatch->Jobs);
				JobBatches.RemoveSingleSwap(*SelectedJobBatch);
			}
		}

		// Kick off remaining compile job batches.
		for (FJobBatch& PendingJobBatch : JobBatches)
		{
			DispatchShaderCompileJobsBatch(PendingJobBatch.Jobs);
		}
	}

	TMemoryHasher<FXxHash64Builder, FXxHash64> WorkerStateHasher;
	bool bHasAnyJobs = false;

	for (auto Iter = DispatchedTasks.CreateIterator(); Iter; ++Iter)
	{
		bool bOutputFileReadFailed = true;

		FDistributedShaderCompilerTask* Task = *Iter;

		bHasAnyJobs = bHasAnyJobs || !Task->ShaderJobs.IsEmpty();

		// Add jobs input hashes to the current state hash
		bool bIsTaskReady = Task->Future.IsReady();
		WorkerStateHasher << bIsTaskReady;
		for (const FShaderCommonCompileJobPtr& Job : Task->ShaderJobs)
		{
			WorkerStateHasher << Job->InputHash;
		}

		if (!bIsTaskReady)
		{
			continue;
		}

		FDistributedBuildTaskResult Result = Task->Future.Get();
		NumDispatchedJobs -= Task->ShaderJobs.Num();

		if (Result.ReturnCode != 0)
		{
			UE_LOG(LogShaderCompilers, Error, TEXT("Shader compiler returned a non-zero error code (%d)."), Result.ReturnCode);
		}

		if (Result.bCompleted)
		{
			// Check the output file exists. If it does, attempt to open it and serialize in the completed jobs.
			bool bCompileJobsSucceeded = false;

			if (IFileManager::Get().FileExists(*Task->OutputFilePath))
			{
				if (TUniquePtr<FArchive> OutputFileAr = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*Task->OutputFilePath, FILEREAD_Silent)))
				{
					bOutputFileReadFailed = false;
					FShaderCompileWorkerDiagnostics WorkerDiagnostics;
					if (FShaderCompileUtilities::DoReadTaskResults(Task->ShaderJobs, *OutputFileAr, GShaderCompilerDumpWorkerDiagnostics ? &WorkerDiagnostics : nullptr) == FSCWErrorCode::Success)
					{
						bCompileJobsSucceeded = true;

						if (GShaderCompilerDumpWorkerDiagnostics)
						{
							FString BatchLabel = FPaths::GetCleanFilename(Task->InputFilePath);
							constexpr uint32 UnavailableWorkerId = 0;
							GShaderCompilerStats->RegisterWorkerDiagnostics(WorkerDiagnostics, MoveTemp(BatchLabel), Task->ShaderJobs.Num(), UnavailableWorkerId);
						}
					}
				}
			}

			if (!bCompileJobsSucceeded)
			{
				// Reading result from distributed job failed, so recompile shaders in current job batch locally
				UE_LOG(LogShaderCompilers, Display, TEXT("Rescheduling shader compilation to run locally after distributed job failed: %s"), *Task->OutputFilePath);

				FString JobDiagnostics;
				for (int32 JobIndex = 0; JobIndex < Task->ShaderJobs.Num(); ++JobIndex)
				{
					const FShaderCommonCompileJobPtr& Job = Task->ShaderJobs[JobIndex];

					// Rescheduling jobs after distributed readback failed should be rare, so display all job details with default verbosity
					JobDiagnostics.Empty();
					Job->AppendDiagnostics(JobDiagnostics, JobIndex, Task->ShaderJobs.Num());
					UE_LOG(LogShaderCompilers, Display, TEXT("Executing %s"), *JobDiagnostics);

					FShaderCompileUtilities::ExecuteShaderCompileJob(*Job);
				}
			}

			// Enter the critical section so we can access the input and output queues
			{
				FScopeLock Lock(&Manager->CompileQueueSection);
				for (const auto& Job : Task->ShaderJobs)
				{
					Manager->ProcessFinishedJob(Job, EShaderCompileJobStatus::CompleteDistributedExecution);
				}
			}
		}
		else
		{
			// The compile job was canceled. Return the jobs to the manager's compile queue.
			UE_LOG(LogShaderCompilers, Display, TEXT("Distributed build task did not complete; returning %d jobs to the compile queue"), Task->ShaderJobs.Num());
			FScopeLock Lock(&Manager->CompileQueueSection);
			Manager->AllJobs.SubmitJobs(Task->ShaderJobs);
		}

		// Delete input and output files, if they exist.
		while (!IFileManager::Get().Delete(*Task->InputFilePath, false, true, true))
		{
			FPlatformProcess::Sleep(0.01f);
		}

		if (!bOutputFileReadFailed)
		{
			while (!IFileManager::Get().Delete(*Task->OutputFilePath, false, true, true))
			{
				FPlatformProcess::Sleep(0.01f);
			}
		}

		Iter.RemoveCurrent();
		delete Task;
	}

	// Yield for a short while to stop this thread continuously polling the disk.
	FPlatformProcess::Sleep(0.01f);

	// Check if shader jobs have not changed in too long
	if (!WorkerStateHeartbeat(bHasAnyJobs ? WorkerStateHasher.Finalize().Hash : 0))
	{
		LogShaderCompileWorkerDistributedDiagnostics(DispatchedTasks);
	}

	// Return true if there is more work to be done.
	return Manager->AllJobs.GetNumOutstandingJobs() > 0;
}

const TCHAR* FShaderCompileDistributedThreadRunnable_Interface::GetThreadName() const
{
	return TEXT("ShaderCompilingThread-Distributed");
}
