// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncRegisterLevelContext.h"
#include "Async/ParallelFor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "SceneInterface.h"
#include "Algo/Transform.h"
#include "Engine/CoreSettings.h"
#include "Components/PrimitiveComponent.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#endif

namespace LevelStreaming::AsyncRegisterLevelContext
{
	static bool bEnabled = false;
	static FAutoConsoleVariableRef CVarEnabled(
		TEXT("LevelStreaming.AsyncRegisterLevelContext.Enabled"),
		bEnabled,
		TEXT("Whether to allow level streaming to add primitives to the scene asynchronously while components are being incrementally registered."));

	static int32 PrimitiveBatchSize = 16;
	static FAutoConsoleVariableRef CVarPrimitiveBatchSize(
		TEXT("LevelStreaming.AsyncRegisterLevelContext.PrimitiveBatchSize"),
		PrimitiveBatchSize,
		TEXT("The number of primitives before starting adding them to the scene during the incremental component registration.\n")
		TEXT("Used when LevelStreaming.AsyncRegisterLevelContext.Enabled is true."));
}

// ------------------------------------
// FAsyncRegisterLevelContext

TUniquePtr<FAsyncRegisterLevelContext> FAsyncRegisterLevelContext::CreateInstance(ULevel* InLevel)
{
	if (LevelStreaming::AsyncRegisterLevelContext::bEnabled)
	{
		bool bSingleThreaded = !FApp::ShouldUseThreadingForPerformance();
#if WITH_EDITOR
		// This is required for async static mesh compilation in case a scene proxy is not async aware.
		// A stall until the compilation is finished might occur, and this is only supported on the game thread for now.
		bSingleThreaded |= FStaticMeshCompilingManager::Get().IsAsyncStaticMeshCompilationEnabled();
#endif
		if (!bSingleThreaded)
		{
			return MakeUnique<FAsyncRegisterLevelContext>(InLevel);
		}
	}
	return nullptr;
}

FAsyncRegisterLevelContext::FAsyncRegisterLevelContext(ULevel* InLevel)
	: Level(InLevel)
	, AsyncAddPrimitiveQueue(*this)
	, SendRenderDynamicDataPrimitivesQueue(*this)
	, bCanLaunchNewTasks(true)
	, bIncrementalRegisterComponentsDone(false)
{
}

void FAsyncRegisterLevelContext::AddPrimitive(UPrimitiveComponent* InComponent)
{
	check(!bIncrementalRegisterComponentsDone);
	AsyncAddPrimitiveQueue.AddPrimitive(InComponent);
	Tick();
}

void FAsyncRegisterLevelContext::AddSendRenderDynamicData(UPrimitiveComponent* InComponent)
{
	check(!bIncrementalRegisterComponentsDone);
	SendRenderDynamicDataPrimitivesQueue.AddSendRenderDynamicData(InComponent);
	Tick();
}

void FAsyncRegisterLevelContext::SetIncrementalRegisterComponentsDone(bool bValue)
{
	bIncrementalRegisterComponentsDone = bValue;
	if (bIncrementalRegisterComponentsDone)
	{
		Flush();
	}
}

void FAsyncRegisterLevelContext::SetCanLaunchNewTasks(bool bValue)
{
	bCanLaunchNewTasks = bValue;
}

bool FAsyncRegisterLevelContext::HasRemainingWork() const
{
	return AsyncAddPrimitiveQueue.HasRemainingWork() || SendRenderDynamicDataPrimitivesQueue.HasRemainingWork();
}

bool FAsyncRegisterLevelContext::IsRunningAsync() const
{
	return AsyncAddPrimitiveQueue.IsRunningAsync() || SendRenderDynamicDataPrimitivesQueue.IsRunningAsync();
}

void FAsyncRegisterLevelContext::WaitForAsyncTasks()
{
	AsyncAddPrimitiveQueue.WaitForAsyncTask();
	SendRenderDynamicDataPrimitivesQueue.WaitForAsyncTask();
}

void FAsyncRegisterLevelContext::Flush()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAsyncRegisterLevelContext::Flush);
	AsyncAddPrimitiveQueue.Flush();
	SendRenderDynamicDataPrimitivesQueue.Flush();
}

bool FAsyncRegisterLevelContext::Tick()
{
	check(!bIncrementalRegisterComponentsDone || AsyncAddPrimitiveQueue.NextBatch.IsEmpty());
	check(!bIncrementalRegisterComponentsDone || SendRenderDynamicDataPrimitivesQueue.NextBatch.IsEmpty());

	TRACE_CPUPROFILER_EVENT_SCOPE(FAsyncRegisterLevelContext::Tick);
	if (!AsyncAddPrimitiveQueue.Tick())
	{
		return false;
	}
	return SendRenderDynamicDataPrimitivesQueue.Tick();
}

// ------------------------------------
// FAsyncAddPrimitiveQueue

FAsyncAddPrimitiveQueue::FAsyncAddPrimitiveQueue(FAsyncRegisterLevelContext& InContext)
	: Context(InContext)
{
}

FAsyncAddPrimitiveQueue::~FAsyncAddPrimitiveQueue()
{
	check(!HasRemainingWork());
}

bool FAsyncAddPrimitiveQueue::HasRemainingWork() const
{
	return !AsyncTask.IsCompleted() || !AddPrimitivesArray.IsEmpty() || !NextBatch.IsEmpty();
}

bool FAsyncAddPrimitiveQueue::IsRunningAsync() const
{
	return AsyncTask.IsValid() && !AsyncTask.IsCompleted();
}

void FAsyncAddPrimitiveQueue::WaitForAsyncTask()
{
	if (AsyncTask.IsValid())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_AddPrimitivesTask_Wait);
		AsyncTask.Wait();
		AsyncTask.Reset();
	}
}

bool FAsyncAddPrimitiveQueue::Tick()
{
	if (IsRunningAsync())
	{
		if (Context.bIncrementalRegisterComponentsDone)
		{
			if (AddPrimitivesArray.Num())
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_AddPrimitivesTask_Execute_One_GameThread);
				TWeakObjectPtr<UPrimitiveComponent> Component;
				while (!AddPrimitivesArray.IsEmpty() && !Component.IsValid())
				{
					FPrimitiveBatch& Batch = AddPrimitivesArray[0];
					while (!Batch.IsEmpty() && !Component.IsValid())
					{
						Component = Batch[0];
						Batch.RemoveAtSwap(0, EAllowShrinking::No);
					}
					if (Batch.IsEmpty())
					{
						AddPrimitivesArray.RemoveAt(0, EAllowShrinking::No);
					}
				};

				if (Component.IsValid())
				{
					FSceneInterface* Scene = Context.Level->GetWorld()->Scene;
					const bool bAppCanEverRender = FApp::CanEverRender();
					FAddPrimitivesTask::Execute(Component.Get(), Scene, bAppCanEverRender);
					return false;
				}
			}
			
			// Nothing else to do, wait for task to finish
			WaitForAsyncTask();
		}
		return false;
	}
	AsyncTask.Reset();

	if (AddPrimitivesArray.Num())
	{
		// Don't launch any new task when context was marked as waiting for running task
		if (Context.bCanLaunchNewTasks)
		{
			FSceneInterface* Scene = Context.Level->GetWorld()->Scene;
			const bool bStartAsyncTask = !Context.bIncrementalRegisterComponentsDone;
			if (bStartAsyncTask)
			{
				FPrimitiveBatch Batch = MoveTemp(AddPrimitivesArray[0]);
				AddPrimitivesArray.RemoveAt(0, EAllowShrinking::No);
				TArray<FPrimitiveBatch> Batches;
				Batches.Emplace(MoveTemp(Batch));
				AsyncTask.Launch(Batches, Scene);
			}
			else
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_AddPrimitivesTask_Execute_GameThread);
				const int32 MaxCount = GLevelStreamingAddPrimitiveGranularity ? GLevelStreamingAddPrimitiveGranularity : MAX_int32;
				int32 TotalMoved = 0;
				TArray<FPrimitiveBatch> Batches;
				while (!AddPrimitivesArray.IsEmpty() && (TotalMoved < MaxCount))
				{
					if (TotalMoved + AddPrimitivesArray[0].Num() <= MaxCount)
					{
						TotalMoved += AddPrimitivesArray[0].Num();
						Batches.Emplace(MoveTemp(AddPrimitivesArray[0]));
						AddPrimitivesArray.RemoveAt(0, EAllowShrinking::No);
					}
					else
					{
						int32 Count = MaxCount - TotalMoved;
						Batches.Emplace_GetRef().Append(AddPrimitivesArray[0].GetData(), Count);
						AddPrimitivesArray[0].RemoveAt(0, Count, EAllowShrinking::No);
						TotalMoved += Count;
					}
				};
				FAddPrimitivesTask::Execute(Batches, Scene);
			}
		}
		// Return false so that caller can test time limit
		return false;
	}

	return !HasRemainingWork();
}

void FAsyncAddPrimitiveQueue::AddPrimitive(UPrimitiveComponent* InComponent)
{
	checkSlow(!NextBatch.Contains(InComponent));
	NextBatch.Add(InComponent);
	if (NextBatch.Num() >= LevelStreaming::AsyncRegisterLevelContext::PrimitiveBatchSize)
	{
		Flush();
	}
}

void FAsyncAddPrimitiveQueue::Flush()
{
	if (!NextBatch.IsEmpty())
	{
		Push(NextBatch);
		check(NextBatch.IsEmpty());
	}
}

void FAsyncAddPrimitiveQueue::Push(FPrimitiveBatch& InAddPrimitives)
{
	check(!InAddPrimitives.IsEmpty());
	AddPrimitivesArray.Emplace(MoveTemp(InAddPrimitives));
}

// ------------------------------------
// FAsyncAddPrimitiveQueue::FAddPrimitivesTask

void FAsyncAddPrimitiveQueue::FAddPrimitivesTask::Reset()
{
	check(IsCompleted());
	Task = UE::Tasks::TTask<void>();
	Batches.Reset();
}

void FAsyncAddPrimitiveQueue::FAddPrimitivesTask::Launch(TArray<FPrimitiveBatch>& InBatches, FSceneInterface* InScene)
{
	check(IsCompleted());
	Batches = MoveTemp(InBatches);
	Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [InScene, this]()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_AddPrimitivesTask_Execute_Async);
		FAddPrimitivesTask::Execute(Batches, InScene);
	});
}

void FAsyncAddPrimitiveQueue::FAddPrimitivesTask::Execute(UPrimitiveComponent* Component, FSceneInterface* InScene, bool bAppCanEverRender)
{
	// AActor::PostRegisterAllComponents (called by AActor::IncrementalRegisterComponents) can trigger code 
	// that either unregisters or re-registers components. If unregistered, skip this component.
	// If re-registered, FRegisterComponentContext is not passed, so SceneProxy can be created.
	if (Component && Component->IsRegistered())
	{
		if (Component->IsRenderStateCreated() || !bAppCanEverRender)
		{
			// Skip if SceneProxy is already created
			if (Component->SceneProxy == nullptr)
			{
				InScene->AddPrimitive(Component);
			}
		}
		else // Fallback for some edge case where the component renderstate are missing
		{
			Component->CreateRenderState_Concurrent(nullptr);
		}
	}
}

void FAsyncAddPrimitiveQueue::FAddPrimitivesTask::Execute(const TArray<FPrimitiveBatch>& InBatches, FSceneInterface* InScene)
{
	check(InScene);

	const bool bAppCanEverRender = FApp::CanEverRender();

	int32 Num = 0;
	for (const FPrimitiveBatch& Batch : InBatches)
	{
		Num += Batch.Num();
	}

	auto GetComponent = [&InBatches](int32 Index) -> UPrimitiveComponent*
	{
		int32 MaxIndex = 0;
		for (const FPrimitiveBatch& Batch : InBatches)
		{
			if (Index < Batch.Num())
			{
				return Batch[Index].Get();
			}
			Index -= Batch.Num();
		}
		return nullptr;
	};

	ParallelFor(Num, [&](int32 Index)
	{
		FTaskTagScope Scope(ETaskTag::EParallelGameThread);
		UPrimitiveComponent* Component = GetComponent(Index);
		FAddPrimitivesTask::Execute(Component, InScene, bAppCanEverRender);
	});
}

void FAsyncAddPrimitiveQueue::FAddPrimitivesTask::Wait()
{
	if (Task.IsValid())
	{
		Task.Wait();
	}
	check(IsCompleted());
}

bool FAsyncAddPrimitiveQueue::FAddPrimitivesTask::IsCompleted() const
{
	return !Task.IsValid() || Task.IsCompleted();
}

bool FAsyncAddPrimitiveQueue::FAddPrimitivesTask::IsValid() const
{
	return Task.IsValid();
}

// ------------------------------------
// FSendRenderDynamicDataPrimitivesQueue

FSendRenderDynamicDataPrimitivesQueue::FSendRenderDynamicDataPrimitivesQueue(FAsyncRegisterLevelContext& InContext)
	: Context(InContext)
{}

FSendRenderDynamicDataPrimitivesQueue::~FSendRenderDynamicDataPrimitivesQueue()
{
	check(!HasRemainingWork());
}

void FSendRenderDynamicDataPrimitivesQueue::AddSendRenderDynamicData(UPrimitiveComponent* InComponent)
{
	checkSlow(!NextBatch.Contains(InComponent));
	NextBatch.Add(InComponent);
	if (NextBatch.Num() >= LevelStreaming::AsyncRegisterLevelContext::PrimitiveBatchSize)
	{
		Flush();
	}
}

void FSendRenderDynamicDataPrimitivesQueue::Flush()
{
	if (!NextBatch.IsEmpty())
	{
		Push(NextBatch);
		check(NextBatch.IsEmpty());
	}
}

void FSendRenderDynamicDataPrimitivesQueue::Push(FPrimitiveBatch& InSendRenderDynamicDataPrimitives)
{
	check(!InSendRenderDynamicDataPrimitives.IsEmpty());
	SendRenderDynamicDataPrimitivesArray.Emplace(MoveTemp(InSendRenderDynamicDataPrimitives));
}

bool FSendRenderDynamicDataPrimitivesQueue::Tick()
{
	if (SendRenderDynamicDataPrimitivesArray.Num())
	{
		FPrimitiveBatch SendRenderDynamicDataPrimitives = MoveTemp(SendRenderDynamicDataPrimitivesArray[0]);
		SendRenderDynamicDataPrimitivesArray.RemoveAt(0, EAllowShrinking::No);

		for (TWeakObjectPtr<UPrimitiveComponent>& Primitive : SendRenderDynamicDataPrimitives)
		{
			// With incremetal updates the component can be registered, added to send render data queue, then destroyed by an actor chain so we must test it's still valid before sending render data.
			if (Primitive.IsValid())
			{
				Primitive->SendRenderDynamicData_Concurrent();
			}
		}
		// Return false so that caller can test time limit
		return false;
	}

	return !HasRemainingWork();
}

bool FSendRenderDynamicDataPrimitivesQueue::HasRemainingWork() const
{
	return !SendRenderDynamicDataPrimitivesArray.IsEmpty() || !NextBatch.IsEmpty();
}