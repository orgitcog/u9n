// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassExecutor.h"
#include "MassProcessingTypes.h"
#include "MassProcessor.h"
#include "MassCommandBuffer.h"
#include "MassExecutionContext.h"
#include "MassProcessingContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace UE::Mass::Executor
{

FORCEINLINE void ExecuteProcessors(FMassEntityManager& EntityManager, TArrayView<UMassProcessor* const> Processors, FMassExecutionContext& ExecutionContext)
{
	for (UMassProcessor* Proc : Processors)
	{
		if (LIKELY(Proc->IsActive()))
		{
			Proc->CallExecute(EntityManager, ExecutionContext);
		}
	}
}

void Run(FMassRuntimePipeline& RuntimePipeline, FProcessingContext& ProcessingContext)
{
	if (!ensure(ProcessingContext.GetDeltaSeconds() >= 0.f) 
		|| !ensure(RuntimePipeline.GetProcessors().Find(nullptr) == INDEX_NONE))
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassExecutor Run Pipeline")
	RunProcessorsView(RuntimePipeline.GetMutableProcessors(), ProcessingContext);
}

void RunSparse(FMassRuntimePipeline& RuntimePipeline, FProcessingContext& ProcessingContext, FMassArchetypeHandle Archetype, TConstArrayView<FMassEntityHandle> Entities)
{
	if (!ensure(RuntimePipeline.GetProcessors().Find(nullptr) == INDEX_NONE) 
		|| RuntimePipeline.Num() == 0 
		|| !ensureMsgf(Archetype.IsValid(), TEXT("The Archetype passed in to UE::Mass::Executor::RunSparse is invalid")))
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassExecutor RunSparseEntities");

	const FMassArchetypeEntityCollection EntityCollection(Archetype, Entities, FMassArchetypeEntityCollection::NoDuplicates);
	RunProcessorsView(RuntimePipeline.GetMutableProcessors(), ProcessingContext, MakeArrayView(&EntityCollection, 1));
}

void RunSparse(FMassRuntimePipeline& RuntimePipeline, FProcessingContext& ProcessingContext, const FMassArchetypeEntityCollection& EntityCollection)
{
	if (!ensure(RuntimePipeline.GetProcessors().Find(nullptr) == INDEX_NONE) 
		|| RuntimePipeline.Num() == 0
		|| !ensureMsgf(EntityCollection.GetArchetype().IsValid(), TEXT("The Archetype of EntityCollection passed in to UE::Mass::Executor::RunSparse is invalid")))
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassExecutor RunSparse");

	RunProcessorsView(RuntimePipeline.GetMutableProcessors(), ProcessingContext, MakeArrayView(&EntityCollection, 1));
}

void Run(UMassProcessor& Processor, FProcessingContext& ProcessingContext)
{
	if (!ensure(ProcessingContext.GetDeltaSeconds() >= 0.f))
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassExecutor Run")

	UMassProcessor* ProcPtr = &Processor;
	RunProcessorsView(MakeArrayView(&ProcPtr, 1), ProcessingContext);
}

void RunProcessorsView(TArrayView<UMassProcessor* const> Processors, FProcessingContext& ProcessingContext, TConstArrayView<FMassArchetypeEntityCollection> EntityCollections)
{
#if WITH_MASSENTITY_DEBUG
	if (Processors.Find(nullptr) != INDEX_NONE)
	{
		UE_LOG(LogMass, Error, TEXT("%s input Processors contains nullptr. Baling out."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}
#endif // WITH_MASSENTITY_DEBUG

	TRACE_CPUPROFILER_EVENT_SCOPE(MassExecutor_RunProcessorsView);

	FMassExecutionContext& ExecutionContext = ProcessingContext.GetExecutionContext();
	FMassEntityManager& EntityManager = *ProcessingContext.GetEntityManager();
	FMassEntityManager::FScopedProcessing ProcessingScope = EntityManager.NewProcessingScope();

	if (EntityCollections.Num() == 0)
	{
		ExecuteProcessors(EntityManager, Processors, ExecutionContext);
	}
	else
	{
		for (const FMassArchetypeEntityCollection& Collection : EntityCollections)
		{
			ExecutionContext.SetEntityCollection(Collection);
			ExecuteProcessors(EntityManager, Processors, ExecutionContext);
			ExecutionContext.ClearEntityCollection();
		}
	}
}

struct FMassExecutorDoneTask
{
	FMassExecutorDoneTask(FMassExecutionContext&& InExecutionContext, TFunction<void()> InOnDoneNotification, const FString& InDebugName, ENamedThreads::Type InDesiredThread)
		: ExecutionContext(InExecutionContext)
		, OnDoneNotification(InOnDoneNotification)
		, DebugName(InDebugName)
		, DesiredThread(InDesiredThread)
	{
	}
	static TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMassExecutorDoneTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Flush Deferred Commands Parallel");
		SCOPE_CYCLE_COUNTER(STAT_Mass_Total);

		FMassEntityManager& EntityManagerRef = ExecutionContext.GetEntityManagerChecked();

		if (&ExecutionContext.Defer() != &EntityManagerRef.Defer())
		{
			ExecutionContext.Defer().MoveAppend(EntityManagerRef.Defer());
		}

		UE_LOG(LogMass, Verbose, TEXT("MassExecutor %s tasks DONE"), *DebugName);
		ExecutionContext.SetFlushDeferredCommands(true);
		ExecutionContext.FlushDeferred();

		OnDoneNotification();
	}
private:
	FMassExecutionContext ExecutionContext;
	TFunction<void()> OnDoneNotification;
	FString DebugName;
	ENamedThreads::Type DesiredThread;
};

FGraphEventRef TriggerParallelTasks(UMassProcessor& Processor, FProcessingContext&& ProcessingContext, TFunction<void()> OnDoneNotification, ENamedThreads::Type CurrentThread)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MassExecutor_RunParallel);

	// We need to transfer ProcessingContext's ExecutionContext - otherwise ProcessingContext's destructor will attempt
	// flushing stored commands. 
	FMassExecutionContext ExecutionContext = MoveTemp(ProcessingContext).GetExecutionContext();

	FGraphEventRef CompletionEvent;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Dispatch Processors")
		CompletionEvent = Processor.DispatchProcessorTasks(ProcessingContext.GetEntityManager(), ExecutionContext, {});
	}

	if (CompletionEvent.IsValid())
	{
		const FGraphEventArray Prerequisites = { CompletionEvent };
		CompletionEvent = TGraphTask<FMassExecutorDoneTask>::CreateTask(&Prerequisites)
			.ConstructAndDispatchWhenReady(MoveTemp(ExecutionContext), OnDoneNotification, Processor.GetName(), CurrentThread);
	}

	return CompletionEvent;
}

//-----------------------------------------------------------------------------
// DEPRECATED
//-----------------------------------------------------------------------------
void RunProcessorsView(TArrayView<UMassProcessor* const> Processors, FProcessingContext& ProcessingContext, const FMassArchetypeEntityCollection* EntityCollection)
{
	if (EntityCollection)
	{
		RunProcessorsView(Processors, ProcessingContext, MakeArrayView(EntityCollection, 1));
	}
	else
	{
		RunProcessorsView(Processors, ProcessingContext);
	}
}

inline FGraphEventRef TriggerParallelTasks(UMassProcessor& Processor, FProcessingContext& ProcessingContext, TFunction<void()> OnDoneNotification
		, ENamedThreads::Type CurrentThread)
{
	FProcessingContext LocalContext = ProcessingContext;
	return TriggerParallelTasks(Processor, MoveTemp(ProcessingContext), OnDoneNotification, CurrentThread);
}

} // namespace UE::Mass::Executor
