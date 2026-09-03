// Copyright Epic Games, Inc. All Rights Reserved.

#include "FastGeoWorldSubsystem.h"
#include "FastGeoContainer.h"
#include "FastGeoPrimitiveComponent.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Streaming/LevelStreamingDelegates.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "WorldPartition/HLOD/HLODRuntimeSubsystem.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "GameFramework/ActorPrimitiveColorHandler.h"
#include "Engine/GameViewportClient.h"
#include "ProfilingDebugging/CsvProfiler.h"

#define LOCTEXT_NAMESPACE "FastGeoWorldSubsystem"

static FName NAME_FastGeoColorHandler(TEXT("FastGeo"));
bool UFastGeoWorldSubsystem::bEnableDebugView = false;

namespace FastGeo
{
	static float GAsyncRenderStateTaskTimeBudgetMS = 0.0f;
	static FAutoConsoleVariableRef CVarAsyncRenderStateTaskTimeBudgetMS(
		TEXT("FastGeo.AsyncRenderStateTask.TimeBudgetMS"),
		GAsyncRenderStateTaskTimeBudgetMS,
		TEXT("Maximum time budget in milliseconds for the async render state tasks (0 = no time limit)"),
		ECVF_Default);

	static int32 GAsyncRenderStateTaskMaxNumComponentsToProcess = 0;
	static FAutoConsoleVariableRef CVarAsyncRenderStateTaskMaxNumComponentsToProcess(
		TEXT("FastGeo.AsyncRenderStateTask.MaxNumComponentsToProcess"),
		GAsyncRenderStateTaskMaxNumComponentsToProcess,
		TEXT("Maximum number of components to process (0 = no component limit)"),
		ECVF_Default);
}

CSV_DEFINE_CATEGORY(FastGeo, true);

bool UFastGeoWorldSubsystem::IsEnableDebugView()
{
	return bEnableDebugView;
}

UFastGeoWorldSubsystem::UFastGeoWorldSubsystem()
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER

	auto UpdatePrimitivesColor = []()
	{
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (World && World->IsGameWorld())
				{
					for (ULevel* Level : World->GetLevels())
					{
						if (UFastGeoContainer* FastGeo = Level->GetAssetUserData<UFastGeoContainer>())
						{
							FastGeo->ForEachComponentCluster([](const FFastGeoComponentCluster& ComponentCluster)
							{
								ComponentCluster.ForEachComponent<FFastGeoPrimitiveComponent>([](const FFastGeoPrimitiveComponent& Component)
								{
									if (FPrimitiveSceneProxy* SceneProxy = Component.GetSceneProxy())
									{
										SceneProxy->SetPrimitiveColor_GameThread(Component.GetDebugColor());
									}
								});
							});
						}
					}
				}
			}
		}
	};

	if (HasAnyFlags(RF_ClassDefaultObject) && ExactCast<UFastGeoWorldSubsystem>(this))
	{
		FActorPrimitiveColorHandler::FPrimitiveColorHandler FastGeoColorHandler;
		FastGeoColorHandler.HandlerName = NAME_FastGeoColorHandler;
		FastGeoColorHandler.HandlerText = LOCTEXT("FastGeo", "FastGeo");
		FastGeoColorHandler.HandlerToolTipText = LOCTEXT("FastGeoColor_ToopTip", "Colorize FastGeo primitives. ISM [Orange], HLOD ISM [Red], SM [Cyan], HLOD SM [Blue], else White.");
		FastGeoColorHandler.bAvailalbleInEditor = false;
		FastGeoColorHandler.GetColorFunc = [](const UPrimitiveComponent* InPrimitiveComponent)
		{
			return FLinearColor::Red;
		};
		FastGeoColorHandler.ActivateFunc = [&UpdatePrimitivesColor]()
		{
			bEnableDebugView = true;
			UpdatePrimitivesColor();
		};
		FastGeoColorHandler.DeactivateFunc = [&UpdatePrimitivesColor]()
		{
			bEnableDebugView = false;
			UpdatePrimitivesColor();
		};
		FActorPrimitiveColorHandler::Get().RegisterPrimitiveColorHandler(FastGeoColorHandler);
	}
#endif
}

UFastGeoWorldSubsystem::~UFastGeoWorldSubsystem()
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	if (HasAnyFlags(RF_ClassDefaultObject) && ExactCast<UFastGeoWorldSubsystem>(this))
	{
		FActorPrimitiveColorHandler::Get().UnregisterPrimitiveColorHandler(NAME_FastGeoColorHandler);
	}
#endif
}

bool UFastGeoWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFastGeoWorldSubsystem::PostInitialize()
{
	Super::PostInitialize();

	UWorld* World = GetWorld();
	if (World->IsPartitionedWorld())
	{
		World->OnAllLevelsChanged().AddUObject(this, &UFastGeoWorldSubsystem::OnUpdateLevelStreaming);
		World->OnAddLevelToWorldExtension().AddUObject(this, &UFastGeoWorldSubsystem::OnAddLevelToWorldExtension);
		World->OnRemoveLevelFromWorldExtension().AddUObject(this, &UFastGeoWorldSubsystem::OnRemoveLevelFromWorldExtension);
		FWorldDelegates::LevelComponentsUpdated.AddUObject(this, &UFastGeoWorldSubsystem::OnLevelComponentsUpdated);
		FWorldDelegates::LevelComponentsCleared.AddUObject(this, &UFastGeoWorldSubsystem::OnLevelComponentsCleared);
#if DO_CHECK
		FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UFastGeoWorldSubsystem::OnLevelAddedToWorld);
		FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UFastGeoWorldSubsystem::OnLevelRemovedFromWorld);
#endif
		Handle_OnLevelStreamingStateChanged = FLevelStreamingDelegates::OnLevelStreamingStateChanged.AddUObject(this, &UFastGeoWorldSubsystem::OnLevelStreamingStateChanged);
		Handle_OnLevelBeginAddToWorld = FLevelStreamingDelegates::OnLevelBeginMakingVisible.AddUObject(this, &UFastGeoWorldSubsystem::OnLevelStartedAddToWorld);
		Handle_OnLevelBeginRemoveFromWorld = FLevelStreamingDelegates::OnLevelBeginMakingInvisible.AddUObject(this, &UFastGeoWorldSubsystem::OnLevelStartedRemoveFromWorld);

		Handle_OnForEachHLODObjectInCell = World->GetSubsystem<UWorldPartitionHLODRuntimeSubsystem>()->GetForEachHLODObjectInCellEvent().AddUObject(this, &UFastGeoWorldSubsystem::ForEachHLODObjectInCell);
	}
}

void UFastGeoWorldSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();
	World->OnAllLevelsChanged().RemoveAll(this);
	World->OnAddLevelToWorldExtension().RemoveAll(this);
	World->OnRemoveLevelFromWorldExtension().RemoveAll(this);
	FWorldDelegates::LevelComponentsUpdated.RemoveAll(this);
	FWorldDelegates::LevelComponentsCleared.RemoveAll(this);
#if DO_CHECK
	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);
#endif
	FLevelStreamingDelegates::OnLevelStreamingStateChanged.Remove(Handle_OnLevelStreamingStateChanged);
	FLevelStreamingDelegates::OnLevelBeginMakingVisible.Remove(Handle_OnLevelBeginAddToWorld);
	FLevelStreamingDelegates::OnLevelBeginMakingInvisible.Remove(Handle_OnLevelBeginRemoveFromWorld);
	Handle_OnLevelStreamingStateChanged.Reset();
	Handle_OnLevelBeginAddToWorld.Reset();
	Handle_OnLevelBeginRemoveFromWorld.Reset();

	World->GetSubsystem<UWorldPartitionHLODRuntimeSubsystem>()->GetForEachHLODObjectInCellEvent().Remove(Handle_OnForEachHLODObjectInCell);
	Handle_OnForEachHLODObjectInCell.Reset();

	Super::Deinitialize();
}

void UFastGeoWorldSubsystem::OnLevelStreamingStateChanged(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LevelIfLoaded, ELevelStreamingState PrevState, ELevelStreamingState NewState)
{
	if (World != GetWorld())
	{
		return;
	}

	if (LevelIfLoaded && ((NewState == ELevelStreamingState::LoadedNotVisible) || (NewState == ELevelStreamingState::LoadedVisible)))
	{
		if (UFastGeoContainer* FastGeo = LevelIfLoaded->GetAssetUserData<UFastGeoContainer>())
		{
			FastGeo->PrecachePSOs();
		}
	}
}

void UFastGeoWorldSubsystem::OnLevelStartedAddToWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* Level)
{
	if (World != GetWorld())
	{
		return;
	}

	if (UFastGeoContainer* FastGeo = Level->GetAssetUserData<UFastGeoContainer>())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoWorldSubsystem::OnLevelStartedAddToWorld);

		FastGeo->Register();
	}
}

void UFastGeoWorldSubsystem::OnLevelStartedRemoveFromWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* Level)
{
	if (World != GetWorld())
	{
		return;
	}

	if (UFastGeoContainer* FastGeo = Level->GetAssetUserData<UFastGeoContainer>())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoWorldSubsystem::OnLevelStartedRemoveFromWorld);

		FastGeo->Unregister();
	}
}

void UFastGeoWorldSubsystem::OnLevelComponentsUpdated(UWorld* World, ULevel* Level)
{
	if (World != GetWorld())
	{
		return;
	}

	if (UFastGeoContainer* FastGeo = Level->GetAssetUserData<UFastGeoContainer>())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoWorldSubsystem::OnLevelComponentsUpdated);

		FastGeo->Register();

		const bool bWaitForCompletion = true;
		FastGeo->Tick(bWaitForCompletion);
	}
}

void UFastGeoWorldSubsystem::OnLevelComponentsCleared(UWorld* World, ULevel* Level)
{
	if (World != GetWorld())
	{
		return;
	}

	if (UFastGeoContainer* FastGeo = Level->GetAssetUserData<UFastGeoContainer>())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoWorldSubsystem::OnLevelComponentsCleared);

		if (World->IsBeingCleanedUp())
		{
			FastGeo->Unregister();

			const bool bWaitForCompletion = true;
			FastGeo->Tick(bWaitForCompletion);
		}
		else
		{
			check(!FastGeo->IsRegistered());
			check(!FastGeo->HasAnyPendingTasks());
		}
	}
}

void UFastGeoWorldSubsystem::OnAddLevelToWorldExtension(ULevel* InLevel, const bool bInWaitForCompletion, bool& bOutHasCompleted)
{
	TGuardValue<bool> GuardValue(bWaitingForCompletion, bInWaitForCompletion);

	if (UFastGeoContainer* FastGeo = InLevel->GetAssetUserData<UFastGeoContainer>())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoWorldSubsystem::OnAddLevelToWorldExtension);

		if (FastGeo->HasAnyPendingCreateTasks())
		{
			FastGeo->Tick(bInWaitForCompletion);
		}

		if (FastGeo->HasAnyPendingCreateTasks())
		{
			bOutHasCompleted = false;
		}
	}
}

void UFastGeoWorldSubsystem::OnRemoveLevelFromWorldExtension(ULevel* InLevel, const bool bInWaitForCompletion, bool& bOutHasCompleted)
{
	TGuardValue<bool> GuardValue(bWaitingForCompletion, bInWaitForCompletion);

	if (UFastGeoContainer* FastGeo = InLevel->GetAssetUserData<UFastGeoContainer>())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoWorldSubsystem::OnRemoveLevelFromWorldExtension);

		if (FastGeo->HasAnyPendingDestroyTasks())
		{
			FastGeo->Tick(bInWaitForCompletion);
		}

		if (FastGeo->HasAnyPendingDestroyTasks())
		{
			bOutHasCompleted = false;
		}
	}
}

void UFastGeoWorldSubsystem::OnUpdateLevelStreaming()
{
	UWorld* World = GetWorld();
	if (World->GetShouldForceUnloadStreamingLevels())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoWorldSubsystem::OnUpdateLevelStreaming);

		const bool bWaitForCompletion = true;
		ProcessAsyncRenderStateJobs(bWaitForCompletion);
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			PhysScene->ProcessAsyncPhysicsStateJobs(bWaitForCompletion);
		}
	}
}

void UFastGeoWorldSubsystem::AddToComponentsPendingRecreate(FFastGeoPrimitiveComponent* InComponentPendingRecreate)
{
	ComponentsPendingRecreate.Add(InComponentPendingRecreate);
}

void UFastGeoWorldSubsystem::RemoveFromComponentsPendingRecreate(FFastGeoPrimitiveComponent* InComponentPendingRecreate)
{
	ComponentsPendingRecreate.Remove(InComponentPendingRecreate);
}

void UFastGeoWorldSubsystem::ProcessPendingRecreate()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoWorldSubsystem::ProcessPendingRecreate);

	CSV_CUSTOM_STAT(FastGeo, PendingRecreate, ComponentsPendingRecreate.Num(), ECsvCustomStatOp::Set);

	int32 NumPrimitiveDelayed = 0;
	for (FWeakFastGeoComponent WeakComponentPendingRecreate : ComponentsPendingRecreate)
	{
		if (FFastGeoPrimitiveComponent* ComponentPendingRecreate = WeakComponentPendingRecreate.Get<FFastGeoPrimitiveComponent>())
		{
#if CSV_PROFILER && !CSV_PROFILER_MINIMAL
			if (ComponentPendingRecreate->IsRenderStateDelayed())
			{
				++NumPrimitiveDelayed;
			}
#endif
			// Skip component if its container was unregistered before we process it
			if (ComponentPendingRecreate->GetOwnerContainer()->IsRegistered())
			{
				ComponentPendingRecreate->DestroyRenderState(nullptr);
				ComponentPendingRecreate->CreateRenderState(nullptr);
			}
		}
	}

	CSV_CUSTOM_STAT(FastGeo, PendingRecreateDelayed, NumPrimitiveDelayed, ECsvCustomStatOp::Set);

	ComponentsPendingRecreate.Reset();
}

void UFastGeoWorldSubsystem::ForEachHLODObjectInCell(const UWorldPartitionRuntimeCell* InCell, TFunction<void(IWorldPartitionHLODObject*)> InFunc)
{
	check(InCell);
	check(InCell->GetLevel());

	if (UFastGeoContainer* FastGeo = InCell->GetLevel()->GetAssetUserData<UFastGeoContainer>())
	{
		// Iterate over clusters in the container, and call InFunc on all HLOD objects
		FastGeo->ForEachComponentCluster<FFastGeoHLOD>([&InFunc](FFastGeoHLOD& HLOD)
		{
			InFunc(&HLOD);
		});
	}
}

#if DO_CHECK
void UFastGeoWorldSubsystem::CheckNoPendingTasks(ULevel* Level, UWorld* World)
{
	if (World != GetWorld())
	{
		return;
	}

	if (UFastGeoContainer* FastGeo = Level->GetAssetUserData<UFastGeoContainer>())
	{
		check(!FastGeo->HasAnyPendingTasks());
	}
}

void UFastGeoWorldSubsystem::OnLevelAddedToWorld(ULevel* Level, UWorld* World)
{
	check(Level);
	CheckNoPendingTasks(Level, World);
}

void UFastGeoWorldSubsystem::OnLevelRemovedFromWorld(ULevel* Level, UWorld* World)
{
	// Null means every sublevel is being removed
	if (!Level)
	{
		check(World);
		for (ULevel* SubLevel : World->GetLevels())
		{
			CheckNoPendingTasks(SubLevel, World);
		}
	}
	else
	{
		CheckNoPendingTasks(Level, World);
	}
}
#endif

bool UFastGeoWorldSubsystem::IsWaitingForCompletion() const
{
	return bWaitingForCompletion ||
			GetWorld()->GetIsInBlockTillLevelStreamingCompleted() ||
			GetWorld()->GetShouldForceUnloadStreamingLevels() ||
			GetWorld()->IsBeingCleanedUp();
}

void UFastGeoWorldSubsystem::Tick(float DeltaTime)
{
	FWriteScopeLock ScopeLock(Lock);

	++TimeEpoch;
	UsedAsyncRenderStateTasksTimeBudgetMS = 0;
	UsedNumComponentsToProcessBudget = 0;
}

TStatId UFastGeoWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFastGeoWorldSubsystem, STATGROUP_Tickables);
}

void UFastGeoWorldSubsystem::RequestAsyncRenderStateTasksBudget_Concurrent(float& OutAvailableTimeBudgetMS, int32& OutAvailableComponentsBudget, int32& OutTimeEpoch)
{
	FWriteScopeLock ScopeLock(Lock);

	bool bUnlimitedBudget = IsInGameThread() ? IsWaitingForCompletion() : false;

	if (bUnlimitedBudget || FastGeo::GAsyncRenderStateTaskTimeBudgetMS == 0)
	{
		OutAvailableTimeBudgetMS = FLT_MAX;
	}
	else
	{
		OutAvailableTimeBudgetMS = FMath::Max(FastGeo::GAsyncRenderStateTaskTimeBudgetMS - UsedAsyncRenderStateTasksTimeBudgetMS, 0);
	}

	if (bUnlimitedBudget || FastGeo::GAsyncRenderStateTaskMaxNumComponentsToProcess == 0)
	{
		OutAvailableComponentsBudget = INT32_MAX;
	}
	else
	{
		OutAvailableComponentsBudget = FMath::Max(FastGeo::GAsyncRenderStateTaskMaxNumComponentsToProcess - UsedNumComponentsToProcessBudget, 0);
	}

	OutTimeEpoch = TimeEpoch;
}

void UFastGeoWorldSubsystem::CommitAsyncRenderStateTasksBudget_Concurrent(float InUsedTimeBudgetMS, int32& InUsedComponentsBudget, int32 InTimeEpoch)
{
	FWriteScopeLock ScopeLock(Lock);

	if (InTimeEpoch == TimeEpoch)
	{
		if (FastGeo::GAsyncRenderStateTaskTimeBudgetMS != 0)
		{
			UsedAsyncRenderStateTasksTimeBudgetMS += InUsedTimeBudgetMS;
		}

		if (FastGeo::GAsyncRenderStateTaskMaxNumComponentsToProcess != 0)
		{
			UsedNumComponentsToProcessBudget += InUsedComponentsBudget;
		}
	}
}

void UFastGeoWorldSubsystem::PushAsyncCreateRenderStateJob(UFastGeoContainer* FastGeo)
{
	check(IsInGameThread());
	if (!AsyncRenderStateJobQueue.IsValid())
	{
		AsyncRenderStateJobQueue = MakeUnique<FFastGeoAsyncRenderStateJobQueue>(FastGeo->GetWorld()->Scene);
	}
	AsyncRenderStateJobQueue->AddJob({ FastGeo, FFastGeoAsyncRenderStateJobQueue::EJobType::CreateRenderState });
}

void UFastGeoWorldSubsystem::PushAsyncDestroyRenderStateJob(UFastGeoContainer* FastGeo)
{
	check(IsInGameThread());
	if (!AsyncRenderStateJobQueue.IsValid())
	{
		AsyncRenderStateJobQueue = MakeUnique<FFastGeoAsyncRenderStateJobQueue>(FastGeo->GetWorld()->Scene);
	}
	AsyncRenderStateJobQueue->AddJob({ FastGeo, FFastGeoAsyncRenderStateJobQueue::EJobType::DestroyRenderState });
}

void UFastGeoWorldSubsystem::ProcessAsyncRenderStateJobs(bool bWaitForCompletion)
{
	check(IsInGameThread());
	if (AsyncRenderStateJobQueue.IsValid())
	{
		AsyncRenderStateJobQueue->Tick(bWaitForCompletion);
		if (AsyncRenderStateJobQueue->IsCompleted())
		{
			AsyncRenderStateJobQueue.Reset();
		}
	}
}

void UFastGeoWorldSubsystem::PushAsyncCreatePhysicsStateJobs(UFastGeoContainer* FastGeo)
{
	check(FPhysScene::SupportsAsyncPhysicsStateCreation());
	FastGeo->OnCreatePhysicsStateBegin_GameThread();
}

void UFastGeoWorldSubsystem::PushAsyncDestroyPhysicsStateJobs(UFastGeoContainer* FastGeo)
{
	check(FPhysScene::SupportsAsyncPhysicsStateDestruction());
	FastGeo->OnDestroyPhysicsStateBegin_GameThread();
}

#undef LOCTEXT_NAMESPACE