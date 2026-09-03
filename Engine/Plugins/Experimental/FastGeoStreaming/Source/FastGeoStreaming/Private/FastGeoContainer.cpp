// Copyright Epic Games, Inc. All Rights Reserved.

#include "FastGeoContainer.h"
#include "FastGeoComponentCluster.h"
#include "FastGeoHLOD.h"
#include "FastGeoComponent.h"
#include "FastGeoWorldSubsystem.h"
#include "FastGeoLog.h"
#include "AI/Navigation/NavigationElement.h"
#include "Async/ParallelFor.h"
#include "Engine/Level.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Misc/Timeout.h"
#include "NavigationSystem.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "PSOPrecache.h"
#include "Templates/Invoke.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartition.h"

namespace FastGeo
{
	static int32 GAsyncRenderStateTaskParallelWorkerCount = 1;
	static FAutoConsoleVariableRef CVarAsyncRenderStateTaskParallelWorkerCount(
		TEXT("FastGeo.AsyncRenderStateTask.ParallelWorkerCount"),
		GAsyncRenderStateTaskParallelWorkerCount,
		TEXT("Set the max number of workers to use when creating FastGeo render state. ")
		TEXT("Only taken into account if value is greater than 1."),
		ECVF_Default);

	class FAssetRemapArchive : public FArchiveProxy
	{
	public:
		FAssetRemapArchive(FArchive& InArchive, TArray<TObjectPtr<UObject>>& InUniqueAssetsArray)
			: FArchiveProxy(InArchive)
			, UniqueAssetsArray(InUniqueAssetsArray)
		{
			// For some unknown reason, copy constructor resets ArIsFilterEditorOnly flag copied from the input archive (see FArchiveState(const FArchiveState&))
			ArIsFilterEditorOnly = InArchive.ArIsFilterEditorOnly;
			for (int32 Index = 0, Num = UniqueAssetsArray.Num(); Index < Num; ++Index)
			{
				UniqueAssets.Add(UniqueAssetsArray[Index], Index);
			}
		}

		virtual FArchive& operator<<(UObject*& Obj) override
		{
			if (IsLoading())
			{
				int32 Index;
				*this << Index;
				Obj = UniqueAssetsArray.IsValidIndex(Index) ? UniqueAssetsArray[Index] : nullptr;
			}
			else if (IsSaving())
			{
				int32 Index = INDEX_NONE;
				if (int32* ExistingIndex = Obj ? UniqueAssets.Find(Obj) : nullptr)
				{
					Index = *ExistingIndex;
				}
				*this << Index;
			}
			return *this;
		}

		virtual FArchive& operator<<(FObjectPtr& Obj) override
		{
			UObject* ObjPtr = Obj.Get();
			FArchive& Result = operator<<(ObjPtr);
			Obj = ObjPtr;
			return Result;
		}

		virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override { unimplemented(); return *this; }
		virtual FArchive& operator<<(FSoftObjectPtr& AssetPtr) override { unimplemented(); return *this; }
		virtual FArchive& operator<<(FSoftObjectPath& AssetPtr) override { unimplemented(); return *this; }
		virtual FArchive& operator<<(FWeakObjectPtr& Value) override { unimplemented(); return *this; }

	private:
		TMap<UObject*, int32> UniqueAssets;
		TArray<TObjectPtr<UObject>>& UniqueAssetsArray;
	};

	class FAsyncTaskAssetReferenceManager : public FGCObject
	{
	public:
		TMap<FGuid, TArray<TObjectPtr<UObject>>> TasksAssets;

		// Made an on-demand singleton rather than a static global, to avoid issues with FGCObject initialization
		static FAsyncTaskAssetReferenceManager& Get()
		{
			static FAsyncTaskAssetReferenceManager Manager;
			return Manager;
		}

		void RegisterTaskAssets(const FGuid& TaskId, const TArray<TObjectPtr<UObject>>& Assets)
		{
			TasksAssets.Add(TaskId, Assets);
		}

		void UnregisterTask(const FGuid& TaskId)
		{
			TasksAssets.Remove(TaskId);
		}

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			for (auto& Pair : TasksAssets)
			{
				Collector.AddReferencedObjects(Pair.Value);
			}
		}

		virtual FString GetReferencerName() const override
		{
			return TEXT("FAsyncTaskAssetReferenceManager");
		}
	};

	struct FAsyncTaskWithAssetRefs : public TSharedFromThis<FAsyncTaskWithAssetRefs>
	{
		explicit FAsyncTaskWithAssetRefs(const TArray<TObjectPtr<UObject>>& InAssets)
			: TaskId(FGuid::NewGuid())
			, Assets(InAssets)
		{
			FAsyncTaskAssetReferenceManager::Get().RegisterTaskAssets(TaskId, Assets);
		}

		~FAsyncTaskWithAssetRefs()
		{
			// Ensure unregistration happens on the game thread
			UE::Tasks::Launch(TEXT("UnregisterFastGeoTask"), [MyTaskId = TaskId]()
			{
				FAsyncTaskAssetReferenceManager::Get().UnregisterTask(MyTaskId);
			}, LowLevelTasks::ETaskPriority::Normal, UE::Tasks::EExtendedTaskPriority::GameThreadNormalPri);
		}

		template<typename TaskBodyType>
		static UE::Tasks::TTask<TInvokeResult_T<TaskBodyType>> Launch(const TCHAR* TaskName, const TArray<TObjectPtr<UObject>>& AssetRefs, TaskBodyType&& Work, LowLevelTasks::ETaskPriority Priority)
		{
			TSharedRef<FAsyncTaskWithAssetRefs> TaskData = MakeShared<FAsyncTaskWithAssetRefs>(AssetRefs);

			return UE::Tasks::Launch(TaskName, [TaskData, Work = MoveTemp(Work)]() mutable
			{
				Work();
			}, Priority);
		}

	private:
		FGuid TaskId;
		TArray<TObjectPtr<UObject>> Assets;
	};
}

void UFastGeoContainer::Register()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::Register);
#if WITH_EDITOR
	if (IsRunningCookCommandlet())
	{
		return;
	}
#endif

	if (!bIsRegistered)
	{
		PendingCreate.Reset();

		check(GetWorld()->IsGameWorld());
#if WITH_EDITOR
		// In PIE we need to initialize dynamic properties as there's no serialization
		InitializeDynamicProperties(true);
#endif
		UWorldPartition* WorldPartition = FWorldPartitionHelpers::GetWorldPartition(this);
		const bool bApplyWorldTransform = WorldPartition->HasInstanceTransform();
		const FTransform& Transform = WorldPartition->GetInstanceTransform();
		ForEachComponentCluster([bApplyWorldTransform , &Transform, this](FFastGeoComponentCluster& ComponentCluster)
		{
			ComponentCluster.OnRegister();

			ComponentCluster.ForEachComponent<FFastGeoPrimitiveComponent>([bApplyWorldTransform, &Transform, this](FFastGeoPrimitiveComponent& Component)
			{
				if (bApplyWorldTransform)
				{
					Component.ApplyWorldTransform(Transform);
				}

				if (Component.ShouldCreateRenderState())
				{
					PendingCreate.RenderState.ComponentsToProcess.Add(&Component);
				}
			});
		});

		UFastGeoWorldSubsystem* WorldSubsystem = GetWorld()->GetSubsystem<UFastGeoWorldSubsystem>();
		if (!PendingCreate.RenderState.ComponentsToProcess.IsEmpty())
		{
			WorldSubsystem->PushAsyncCreateRenderStateJob(this);
		}

		WorldSubsystem->PushAsyncCreatePhysicsStateJobs(this);
		RegisterToNavigationSystem();

		bIsRegistered = true;
	}

	if (PendingCreate.HasAnyPendingState())
	{
		Tick();
	}
}

bool UFastGeoContainer::IsRegistered() const
{
	return bIsRegistered;
}

void UFastGeoContainer::Unregister()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::Unregister);

#if WITH_EDITOR
	if (IsRunningCookCommandlet())
	{
		return;
	}
#endif

	if (bIsRegistered)
	{
		PendingDestroy.Reset();

		ForEachComponentCluster([this](FFastGeoComponentCluster& ComponentCluster)
		{
			ComponentCluster.OnUnregister();

			ComponentCluster.ForEachComponent<FFastGeoPrimitiveComponent>([this](FFastGeoPrimitiveComponent& Component)
			{
				if (Component.IsRenderStateCreated())
				{
					PendingDestroy.RenderState.ComponentsToProcess.Add(&Component);
				}
			});
		});

		UnregisterFromNavigationSystem();
		
		UFastGeoWorldSubsystem* WorldSubsystem = GetWorld()->GetSubsystem<UFastGeoWorldSubsystem>();
		if (!PendingDestroy.RenderState.ComponentsToProcess.IsEmpty())
		{
			WorldSubsystem->PushAsyncDestroyRenderStateJob(this);
		}

		WorldSubsystem->PushAsyncDestroyPhysicsStateJobs(this);
		bIsRegistered = false;
	}

	if (PendingDestroy.HasAnyPendingState())
	{
		Tick();
	}
}

void UFastGeoContainer::Tick(bool bWaitForCompletion)
{
	UWorld* World = GetWorld();
	check(World);
	UFastGeoWorldSubsystem* WorldSubsystem = World->GetSubsystem<UFastGeoWorldSubsystem>();

	do
	{
		WorldSubsystem->ProcessAsyncRenderStateJobs(bWaitForCompletion);

		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			PhysScene->ProcessAsyncPhysicsStateJobs(bWaitForCompletion);
		}
	} while (bWaitForCompletion && HasAnyPendingTasks());
}

bool UFastGeoContainer::HasAnyPendingTasks() const
{
	return HasAnyPendingCreateTasks() || HasAnyPendingDestroyTasks();
}

bool UFastGeoContainer::HasAnyPendingCreateTasks() const
{
	// Physics state can have no PendingState but OnAsyncCreatePhysicsStateEnd_GameThread has not been called yet
	return (PhysicsStateCreation == EPhysicsStateCreation::Creating) || PendingCreate.HasAnyPendingState();
}

bool UFastGeoContainer::HasAnyPendingDestroyTasks() const
{
	// Physics state can have no PendingState but OnAsyncDestroyPhysicsStateEnd_GameThread has not been called yet
	return (PhysicsStateCreation == EPhysicsStateCreation::Destroying) || PendingDestroy.HasAnyPendingState();
}

FFastGeoComponentCluster* UFastGeoContainer::GetComponentCluster(uint32 InComponentClusterTypeID, int32 InComponentClusterIndex)
{
	if (FFastGeoHLOD::Type.IsSameTypeID(InComponentClusterTypeID))
	{
		return HLODs.IsValidIndex(InComponentClusterIndex) ? &HLODs[InComponentClusterIndex] : nullptr;
	}
	else if (FFastGeoComponentCluster::Type.IsSameTypeID(InComponentClusterTypeID))
	{
		return ComponentClusters.IsValidIndex(InComponentClusterIndex) ? &ComponentClusters[InComponentClusterIndex] : nullptr;
	}
	check(false);
	return nullptr;
}

ULevel* UFastGeoContainer::GetLevel() const
{
	return GetOuterULevel();
}

UWorld* UFastGeoContainer::GetWorld() const
{
	check(GetLevel());
	return GetLevel()->GetWorld();
}

void UFastGeoContainer::RegisterToNavigationSystem()
{
	UWorld* World = GetWorld();
	if (!FNavigationSystem::SupportsDynamicChanges(World))
	{
		return;
	}
	
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::RegisterToNavigationSystem);
	check(NavigationElementHandles.IsEmpty());

	ForEachComponentCluster([this, World](FFastGeoComponentCluster& ComponentCluster)
	{
		ComponentCluster.ForEachComponent<FFastGeoPrimitiveComponent>([this, World](FFastGeoPrimitiveComponent& Component)
		{
			if (Component.IsNavigationRelevant())
			{
				FNavigationElement Element(*this, reinterpret_cast<const uint64>(&Component));
				Element.SetDirtyAreaOnRegistration(!Component.ShouldSkipNavigationDirtyAreaOnAddOrRemove());
				Element.SetBounds(Component.GetNavigationBounds());
				Element.SetBodySetup(Component.GetBodySetup());
				Element.SetTransform(Component.GetTransform());
				Element.SetGeometryExportType(Component.HasCustomNavigableGeometry());
				Element.NavigationDataExportDelegate.BindWeakLambda(this, [&Component](const FNavigationElement& NavigationElement, FNavigationRelevantData& OutNavigationRelevantData)
				{
					Component.GetNavigationData(OutNavigationRelevantData);
				});
				Element.CustomGeometryExportDelegate.BindWeakLambda(this, [&Component](const FNavigationElement& NavigationElement, FNavigableGeometryExport& OutGeometry, bool& bOutShouldExportDefaultGeometry)
				{
					bOutShouldExportDefaultGeometry = Component.DoCustomNavigableGeometryExport(OutGeometry);
				});
				FNavigationElementHandle Handle = FNavigationSystem::AddNavigationElement(World, MoveTemp(Element));
				if (ensure(Handle.IsValid()))
				{
					NavigationElementHandles.Add(Handle);
				}
			}
		});
	});
}

void UFastGeoContainer::UnregisterFromNavigationSystem()
{
	if (!NavigationElementHandles.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::UnregisterFromNavigationSystem);

		for (FNavigationElementHandle& ElementHandle : NavigationElementHandles)
		{
			FNavigationSystem::RemoveNavigationElement(GetWorld(), ElementHandle);
		}

		NavigationElementHandles.Empty();
	}
}

void UFastGeoContainer::OnCreateRenderStateBegin_GameThread()
{
	check(PendingCreate.RenderState.NumToProcess == 0);
	check(PendingCreate.RenderState.TotalNumProcessed != PendingCreate.RenderState.ComponentsToProcess.Num());

	PendingCreate.bIsInBlockingWait = GetWorld()->GetSubsystem<UFastGeoWorldSubsystem>()->IsWaitingForCompletion();

	PendingCreate.RenderState.NumToProcess = PendingCreate.RenderState.ComponentsToProcess.Num() - PendingCreate.RenderState.TotalNumProcessed;
}

void UFastGeoContainer::OnDestroyRenderStateBegin_GameThread()
{
	check(PendingDestroy.RenderState.NumToProcess == 0);
	check(PendingDestroy.RenderState.TotalNumProcessed != PendingDestroy.RenderState.ComponentsToProcess.Num());

	PendingDestroy.bIsInBlockingWait = GetWorld()->GetSubsystem<UFastGeoWorldSubsystem>()->IsWaitingForCompletion();

	// TODO_FASTGEO: Evaluate if we need throttling of the async destruction task
	PendingDestroy.RenderState.NumToProcess = PendingDestroy.RenderState.ComponentsToProcess.Num();
}

void UFastGeoContainer::OnCreateRenderState_Concurrent()
{
	static const int32 MinNumElementsToProcessPerThread = 8;
	const int32 NumComponentsToProcess = PendingCreate.RenderState.NumToProcess;
	const int32 MaxNumThreads = PendingCreate.bIsInBlockingWait ? INT32_MAX : FastGeo::GAsyncRenderStateTaskParallelWorkerCount;
	const int32 NumThreads = FMath::Clamp(NumComponentsToProcess / MinNumElementsToProcessPerThread, 1, MaxNumThreads);
	const bool bIsParallelForAllowed = NumThreads > 1 && FApp::ShouldUseThreadingForPerformance();

	float AvailableTimeBudgetMS;
	int32 AvailableComponentsBudget;
	int32 TimeEpoch;

	UFastGeoWorldSubsystem* WorldSubsystem = GetWorld()->GetSubsystem<UFastGeoWorldSubsystem>();
	WorldSubsystem->RequestAsyncRenderStateTasksBudget_Concurrent(AvailableTimeBudgetMS, AvailableComponentsBudget, TimeEpoch);
	
	const int32 ComponentsBudget = FMath::Min(NumComponentsToProcess, AvailableComponentsBudget);
	const double TimeBudgetSeconds = AvailableTimeBudgetMS / 1000.0;
	UE::FTimeout Timeout = UE::FTimeout(TimeBudgetSeconds);

	TAtomic<int32> NextIndex = 0;
	TAtomic<int32> NumProcessed = 0;

	if (ComponentsBudget > 0 && !Timeout.IsExpired())
	{
		ParallelFor(NumThreads, [&](int32 Index)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::OnCreateRenderState_Concurrent);
			// Currently necessary for FSimpleStreamableAssetManager::FRegister (to be revisited)
			FTaskTagScope Scope(ETaskTag::EParallelGameThread);

			while(true)
			{
				// Time budget exceeded ?
				if (Timeout.IsExpired())
				{
					return; 
				}

				// All work completed ?
				int32 LocalIdx = NextIndex++;
				if (LocalIdx >= ComponentsBudget)
				{
					return; 
				}

				FFastGeoPrimitiveComponent* ComponentToProcess = PendingCreate.RenderState.ComponentsToProcess[PendingCreate.RenderState.TotalNumProcessed + LocalIdx];
				ComponentToProcess->CreateRenderState(/*Context = */ nullptr);

				NumProcessed++;
			}
		}, bIsParallelForAllowed ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);
	}

	// Adjust NumRenderStatesToProcess to the actual number of states we processed. 
	PendingCreate.RenderState.NumProcessed = NumProcessed.Load(EMemoryOrder::Relaxed);

	WorldSubsystem->CommitAsyncRenderStateTasksBudget_Concurrent(Timeout.GetElapsedSeconds() * 1000, PendingCreate.RenderState.NumProcessed, TimeEpoch);
}

void UFastGeoContainer::OnDestroyRenderState_Concurrent()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::OnDestroyRenderState_Concurrent);

	// TODO_FASTGEO: Evaluate if we need throttling of the async destruction task
	check(PendingDestroy.RenderState.NumToProcess == PendingDestroy.RenderState.ComponentsToProcess.Num());
	{
		FFastGeoPrimitiveComponent::FFastGeoDestroyRenderStateContext Context(GetWorld()->Scene);

		for (FFastGeoPrimitiveComponent* Component : PendingDestroy.RenderState.ComponentsToProcess)
		{
			Component->DestroyRenderState(&Context);
		}

		PendingDestroy.RenderState.NumProcessed = PendingDestroy.RenderState.NumToProcess;
	}
}

void UFastGeoContainer::OnCreateRenderStateEnd_GameThread()
{
	check(PendingCreate.RenderState.NumToProcess != 0);
	check(PendingCreate.RenderState.TotalNumProcessed != PendingCreate.RenderState.ComponentsToProcess.Num());

	PendingCreate.RenderState.TotalNumProcessed += PendingCreate.RenderState.NumProcessed;
	PendingCreate.RenderState.NumToProcess = 0;
	PendingCreate.RenderState.NumProcessed = 0;
	PendingCreate.bIsInBlockingWait = false;

	if (PendingCreate.RenderState.TotalNumProcessed < PendingCreate.RenderState.ComponentsToProcess.Num())
	{
		UFastGeoWorldSubsystem* WorldSubsystem = GetWorld()->GetSubsystem<UFastGeoWorldSubsystem>();
		WorldSubsystem->PushAsyncCreateRenderStateJob(this);
	}
}

void UFastGeoContainer::OnDestroyRenderStateEnd_GameThread()
{
	check(PendingDestroy.RenderState.NumToProcess != 0);
	check(PendingDestroy.RenderState.TotalNumProcessed != PendingDestroy.RenderState.ComponentsToProcess.Num());

	PendingDestroy.RenderState.TotalNumProcessed += PendingDestroy.RenderState.NumProcessed;
	PendingDestroy.RenderState.NumToProcess = 0;
	PendingDestroy.RenderState.NumProcessed = 0;	
	PendingDestroy.bIsInBlockingWait = false;

	if (PendingDestroy.RenderState.TotalNumProcessed < PendingDestroy.RenderState.ComponentsToProcess.Num())
	{
		UFastGeoWorldSubsystem* WorldSubsystem = GetWorld()->GetSubsystem<UFastGeoWorldSubsystem>();
		WorldSubsystem->PushAsyncDestroyRenderStateJob(this);
	}
}

void UFastGeoContainer::BeginDestroy()
{
	Super::BeginDestroy();
	DestroyFence.BeginFence();
}

bool UFastGeoContainer::IsReadyForFinishDestroy()
{
	bool bResult = Super::IsReadyForFinishDestroy() && DestroyFence.IsFenceComplete();
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	bResult = bResult && PrecachePSOsTask.IsCompleted();
#endif
	return bResult;
}

#if WITH_EDITOR
void UFastGeoContainer::AddComponentCluster(FFastGeoComponentCluster* ComponentCluster)
{
	FFastGeoComponentCluster* NewComponentCluster;
	if (ComponentCluster->IsA<FFastGeoHLOD>())
	{
		NewComponentCluster = &HLODs.Add_GetRef(*ComponentCluster->CastTo<FFastGeoHLOD>());
		NewComponentCluster->SetComponentClusterIndex(HLODs.Num() - 1);
	}
	else
	{
		NewComponentCluster = &ComponentClusters.Add_GetRef(*ComponentCluster);
		NewComponentCluster->SetComponentClusterIndex(ComponentClusters.Num() - 1);
	}
}

void UFastGeoContainer::CreateEditorProxyObjects()
{
	ForEachComponentCluster([this](FFastGeoComponentCluster& ComponentCluster)
	{
		ComponentCluster.ForEachComponent([this](FFastGeoComponent& Component)
		{
			if (UClass* EditorProxyClass = Component.GetEditorProxyClass())
			{
				UFastGeoComponentEditorProxy* ComponentEditorProxy = NewObject<UFastGeoComponentEditorProxy>(this, EditorProxyClass);
				ComponentEditorProxy->SetFastGeoComponent(&Component);
				Component.SetEditorProxy(ComponentEditorProxy);
				EditorProxyObjects.Add(ComponentEditorProxy);
			}
		});
	});
}

void UFastGeoContainer::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	ForEachComponentCluster([&ObjectSaveContext](FFastGeoComponentCluster& ComponentCluster)
	{
		ComponentCluster.PreSave(ObjectSaveContext);
	});
}

class FFastGeoGatherFastGeoContainerAssetRefsArchive : public FArchive
{
public:
	FFastGeoGatherFastGeoContainerAssetRefsArchive(UFastGeoContainer& Container)
	{
		KnownClasses.Add(URuntimeVirtualTexture::StaticClass());
		KnownClasses.Add(UStaticMesh::StaticClass());
		KnownClasses.Add(UMaterialInterface::StaticClass());
		KnownClasses.Add(UPhysicalMaterial::StaticClass());
		KnownClasses.Add(USkeletalMesh::StaticClass());

		SetIsPersistent(true);
		SetIsSaving(true);
		ArIgnoreOuterRef = true;
		ArIsObjectReferenceCollector = true;
		ArShouldSkipBulkData = true;
		Container.SerializeComponentClusters(*this);
	}

	virtual FArchive& operator<<(UObject*& Obj) override
	{
		if (Obj && !Obj->IsTemplate() && !Obj->HasAnyFlags(RF_Transient))
		{
			if (!UniqueAssets.Contains(Obj))
			{
				check(IsKnownClass(Obj->GetClass()));
				UniqueAssets.Add(Obj);
			}
		}
		return *this;
	}

	virtual FArchive& operator<<(FObjectPtr& Obj) override
	{
		UObject* ObjPtr = Obj.Get();
		FArchive& Result = operator<<(ObjPtr);
		Obj = ObjPtr;
		return Result;
	}

	const TSet<UObject*>& GetUniqueAssets() { return UniqueAssets; }

private:
	bool IsKnownClass(UClass* Class)
	{
		for (UClass* KnownClass : KnownClasses)
		{
			if (Class->IsChildOf(KnownClass))
			{
				return true;
			}
		}
		return false;
	}

	TSet<UClass*> KnownClasses;
	TSet<UObject*> UniqueAssets;
};

void UFastGeoContainer::CollectAssetReferences()
{
	Assets = FFastGeoGatherFastGeoContainerAssetRefsArchive(*this).GetUniqueAssets().Array();
}

void UFastGeoContainer::OnCreated()
{
	// Initialize component clusters & components dynamic properties
	InitializeDynamicProperties(GetLevel()->GetWorld()->IsGameWorld());

	// In editor, we need a UObject representation of the components for some operations
	CreateEditorProxyObjects();

	// Collect references in order to avoid garbage collection of objects that may now be unreferenced
	// The fast geo container will hold onto those objects if necessary.
	CollectAssetReferences();
}
#endif

void UFastGeoContainer::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	FastGeo::FAssetRemapArchive AssetRemapAr(Ar, Assets);
	SerializeComponentClusters(AssetRemapAr);

#if !WITH_EDITOR
	if (Ar.IsLoading())
	{
		// Once loaded, initialize component clusters & components dynamic properties
		InitializeDynamicProperties(true);
	}
#endif
}

void UFastGeoContainer::SerializeComponentClusters(FArchive& Ar)
{
	Ar << ComponentClusters;
	Ar << HLODs;
}

void UFastGeoContainer::PrecachePSOs()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::PrecachePSOs_GameThread);

#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	if (bPrecachedPSOs)
	{
		return;
	}
	bPrecachedPSOs = true;

	if (IsComponentPSOPrecachingEnabled())
	{
		TArray<FFastGeoPrimitiveComponent*> Components;
		ForEachComponentCluster([&Components](FFastGeoComponentCluster& ComponentCluster)
		{
			ComponentCluster.ForEachComponent<FFastGeoPrimitiveComponent>([&Components](FFastGeoPrimitiveComponent& Component)
			{
				// Mark component so that IsPSOPrecaching() return true even if component's PSO task has not started
				Component.MarkPrecachePSOsRequired();
				Components.Add(&Component);
			});
		});

		check(PrecachePSOsTask.IsCompleted());
		PrecachePSOsTask = FastGeo::FAsyncTaskWithAssetRefs::Launch(TEXT("UFastGeoContainer::PrecachePSOs_Task"), Assets, [this, Components = MoveTemp(Components)]()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::PrecachePSOs_Task);
			for (FFastGeoPrimitiveComponent* Component : Components)
			{
				Component->PrecachePSOs();
			}
		}, LowLevelTasks::ETaskPriority::BackgroundNormal);
	}
#endif
}

void UFastGeoContainer::InitializeDynamicProperties(bool bInitForPlay)
{
	ForEachComponentCluster([this](FFastGeoComponentCluster& ComponentCluster)
	{
		ComponentCluster.SetOwnerContainer(this);
		ComponentCluster.InitializeDynamicProperties();
	});

	if (bInitForPlay)
	{
		CollisionComponents.Reset();
		ForEachComponentCluster([this](FFastGeoComponentCluster& ComponentCluster)
		{
			ComponentCluster.ForEachComponent([this](FFastGeoComponent& Component)
			{
				if (Component.IsCollisionEnabled())
				{
					CollisionComponents.Add(&Component);
				}
			});
		});
	}
}

void UFastGeoContainer::OnCreatePhysicsStateBegin_GameThread()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::OnCreatePhysicsStateBegin_GameThread);

	PendingCreate.PhysicsState.ComponentsToProcess = &CollisionComponents;
	check(PendingCreate.PhysicsState.TotalNumProcessed == 0);

	UWorld* World = GetWorld();
	check(World);
	FPhysScene* PhysScene = World->GetPhysicsScene();
	check(PhysScene);
	PhysScene->PushAsyncCreatePhysicsStateJob(this);
}

void UFastGeoContainer::OnDestroyPhysicsStateBegin_GameThread()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::OnDestroyPhysicsStateBegin_GameThread);

	PendingDestroy.PhysicsState.ComponentsToProcess = &CollisionComponents;
	check(PendingDestroy.PhysicsState.TotalNumProcessed == 0);

	UWorld* World = GetWorld();
	check(World);
	FPhysScene* PhysScene = World->GetPhysicsScene();
	check(PhysScene);
	verify(PhysScene->PushAsyncDestroyPhysicsStateJob(this));
}

bool UFastGeoContainer::AllowsAsyncPhysicsStateCreation() const
{
	check(FPhysScene::SupportsAsyncPhysicsStateCreation());
	return true;
}

bool UFastGeoContainer::AllowsAsyncPhysicsStateDestruction() const
{
	check(FPhysScene::SupportsAsyncPhysicsStateDestruction());
	return true;
}

bool UFastGeoContainer::IsAsyncPhysicsStateCreated() const
{
	return PhysicsStateCreation == EPhysicsStateCreation::Created;
}

UObject* UFastGeoContainer::GetAsyncPhysicsStateObject() const
{
	return const_cast<UFastGeoContainer*>(this);
}

bool UFastGeoContainer::OnAsyncCreatePhysicsState(const UE::FTimeout& Timeout)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::OnAsyncCreatePhysicsState);
	check((PhysicsStateCreation == EPhysicsStateCreation::NotCreated) || 
		  ((PhysicsStateCreation == EPhysicsStateCreation::Creating) && (PendingCreate.PhysicsState.TotalNumProcessed > 0) && !PendingCreate.PhysicsState.IsCompleted()));
	PhysicsStateCreation = EPhysicsStateCreation::Creating;

	check(PendingCreate.PhysicsState.ComponentsToProcess);
	for (int Index = PendingCreate.PhysicsState.TotalNumProcessed; Index < PendingCreate.PhysicsState.ComponentsToProcess->Num(); ++Index)
	{
		FFastGeoComponent* Component = (*PendingCreate.PhysicsState.ComponentsToProcess)[Index];
		Component->OnAsyncCreatePhysicsState();
		++PendingCreate.PhysicsState.TotalNumProcessed;
		if (!PendingCreate.PhysicsState.IsCompleted() && Timeout.IsExpired())
		{
			return false;
		}
	}

	return true;
}

void UFastGeoContainer::OnAsyncCreatePhysicsStateEnd_GameThread()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::OnAsyncCreatePhysicsStateEnd_GameThread);
	check(PendingCreate.PhysicsState.ComponentsToProcess && PendingCreate.PhysicsState.IsCompleted());
	PendingCreate.PhysicsState.Reset();
	check(PhysicsStateCreation == EPhysicsStateCreation::Creating);
	PhysicsStateCreation = EPhysicsStateCreation::Created;
	
	// This loop could be removed if component's PhysicsStateCreation was removed
	for (FFastGeoComponent* Component : CollisionComponents)
	{
		Component->OnAsyncCreatePhysicsStateEnd_GameThread();
	}
}

void UFastGeoContainer::OnAsyncDestroyPhysicsStateBegin_GameThread()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::OnAsyncDestroyPhysicsStateBegin_GameThread);
	check(PhysicsStateCreation == EPhysicsStateCreation::Created);
	PhysicsStateCreation = EPhysicsStateCreation::Destroying;

	for (FFastGeoComponent* Component : CollisionComponents)
	{
		Component->OnAsyncDestroyPhysicsStateBegin_GameThread();
	}
}

bool UFastGeoContainer::OnAsyncDestroyPhysicsState(const UE::FTimeout& Timeout)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::OnAsyncDestroyPhysicsState);

	//for (FFastGeoComponent* Component : CollisionComponents)
	check(PendingDestroy.PhysicsState.ComponentsToProcess);
	for (int Index = PendingDestroy.PhysicsState.TotalNumProcessed; Index < PendingDestroy.PhysicsState.ComponentsToProcess->Num(); ++Index)
	{
		FFastGeoComponent* Component = (*PendingDestroy.PhysicsState.ComponentsToProcess)[Index];
		Component->OnAsyncDestroyPhysicsState();
		++PendingDestroy.PhysicsState.TotalNumProcessed;
		if (!PendingDestroy.PhysicsState.IsCompleted() && Timeout.IsExpired())
		{
			return false;
		}
	}
	return true;
}

void UFastGeoContainer::OnAsyncDestroyPhysicsStateEnd_GameThread()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UFastGeoContainer::OnAsyncDestroyPhysicsStateEnd_GameThread);
	check(PendingDestroy.PhysicsState.ComponentsToProcess && PendingDestroy.PhysicsState.IsCompleted());
	PendingDestroy.PhysicsState.Reset();
	check(PhysicsStateCreation == EPhysicsStateCreation::Destroying);
	PhysicsStateCreation = EPhysicsStateCreation::NotCreated;

	// This loop could be removed if component's PhysicsStateCreation was removed
	for (FFastGeoComponent* Component : CollisionComponents)
	{
		Component->OnAsyncDestroyPhysicsStateEnd_GameThread();
	}
}

void UFastGeoContainer::CollectBodySetupsWithPhysicsMeshesToCreate(TSet<UBodySetup*>& OutBodySetups) const
{
	for (FFastGeoComponent* Component : CollisionComponents)
	{
		if (UBodySetup* BodySetup = const_cast<FFastGeoComponent*>(Component)->GetBodySetup(); BodySetup && !BodySetup->bCreatedPhysicsMeshes)
		{
			OutBodySetups.Add(BodySetup);
		}
	}
}

IPhysicsBodyInstanceOwner* UFastGeoContainer::ResolvePhysicsBodyInstanceOwner(Chaos::FConstPhysicsObjectHandle PhysicsObject)
{
	if (PhysicsObject)
	{
		FLockedReadPhysicsObjectExternalInterface PhysicsObjectInterface = FPhysicsObjectExternalInterface::LockRead(PhysicsObject);
		FChaosUserDefinedEntity* UserDefinedEntity = PhysicsObjectInterface->GetUserDefinedEntity(PhysicsObject);
		return FFastGeoPhysicsBodyInstanceOwner::GetPhysicsBodyInstanceOwner(UserDefinedEntity);
	}
	return nullptr;
}
