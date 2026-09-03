// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Features/IModularFeatures.h"
#include "Async/Async.h"

struct FDistributedBuildTaskResult
{
	int32 ReturnCode;
	bool bCompleted;
};

struct FDistributedBuildStats
{
	uint32 MaxRemoteAgents = 0;
	uint32 MaxActiveAgentCores = 0;
};

struct FTaskCommandData
{	
	FString Command;
	FString WorkingDirectory;
	FString InputFileName;
	FString OutputFileName;
	FString ExtraCommandArgs;
	FString Description; // Optional string describing the task. Shows up in UBA trace files for each job.
	uint32 DispatcherPID = 0;
	TArray<FString> Dependencies;
	TArray<FString> AdditionalOutputFolders; // Optional additional folder(s) which task may write artifacts to
};

struct FDistributedBuildTask
{
	uint32 ID;
	FTaskCommandData CommandData;
	TPromise<FDistributedBuildTaskResult> Promise;

	FDistributedBuildTask(uint32 ID, const FTaskCommandData& CommandData, TPromise<FDistributedBuildTaskResult>&& Promise)
		: ID(ID)
		, CommandData(CommandData)
		, Promise(MoveTemp(Promise))
	{
	}

	/** Sets the promised task result to being incomplete, i.e. FDistributedBuildTaskResult::bCompleted=false. */
	void Cancel()
	{
		FDistributedBuildTaskResult Result;
		Result.ReturnCode = 0;
		Result.bCompleted = false;
		Promise.SetValue(Result);
	}

	/** Sets the promised task result to being completed with the specified return code. */
	void Finalize(int32 InReturnCode)
	{
		FDistributedBuildTaskResult Result;
		Result.ReturnCode = InReturnCode;
		Result.bCompleted = true;
		Promise.SetValue(Result);
	}
};

using FTask UE_DEPRECATED(5.6, "FTask from DistributedBUildControllerInterface.h has been renamed to FDistributedBuildTask in UE5.6") = FDistributedBuildTask;

struct FTaskResponse
{
	uint32 ID;
	int32 ReturnCode;
};

class IDistributedBuildController : public IModuleInterface, public IModularFeature
{
public:
	virtual bool SupportsDynamicReloading() override { return false; }

	// Returns true if this distributed controller also supports local workers alongside remote workers. By default false.
	virtual bool SupportsLocalWorkers() { return false; }
	
	virtual bool RequiresRelativePaths() { return false; }

	// Sets the maxmium number of local workers. Ignored if this controller does not support local workers.
	virtual void SetMaxLocalWorkers(int32 InMaxNumLocalWorkers) { /*dummy*/ }

	virtual void InitializeController() = 0;
	
	// Returns true if the controller may be used.
	virtual bool IsSupported() = 0;

	// Returns the name of the controller. Used for logging purposes.
	virtual const FString GetName() = 0;

	virtual FString RemapPath(const FString& SourcePath) const { return SourcePath; }

	virtual void Tick(float DeltaSeconds){}

	// Returns a new file path to be used for writing input data to.
	virtual FString CreateUniqueFilePath() = 0;

	// Returns the distributed build statistics since the last call and resets its internal values. Returns false if there are no statistics provided.
	virtual bool PollStats(FDistributedBuildStats& OutStats) { return false; }

	// Launches a task. Returns a future which can be waited on for the results.
	virtual TFuture<FDistributedBuildTaskResult> EnqueueTask(const FTaskCommandData& CommandData) = 0;
	
	static const FName& GetModularFeatureType()
	{
		static FName FeatureTypeName = FName(TEXT("DistributedBuildController"));
		return FeatureTypeName;
	}
};
