// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "FastGeoWeakElement.h"
#include "FastGeoAsyncRenderStateJobQueue.h"
#include "FastGeoWorldSubsystem.generated.h"

class FFastGeoPrimitiveComponent;
class ULevelStreaming;
class ULevel;
class IWorldPartitionHLODObject;
enum class ELevelStreamingState : uint8;

UCLASS()
class FASTGEOSTREAMING_API UFastGeoWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

	UFastGeoWorldSubsystem();
	~UFastGeoWorldSubsystem();

public:
	//~Begin USubsystem interface
	virtual void Deinitialize() override;
	//~End USubsystem interface

	//~ Begin UWorldSubsystem interface.
	virtual void PostInitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem interface.

	//~ Begin UTickableWorldSubsystem interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End UTickableWorldSubsystem interface

	void AddToComponentsPendingRecreate(FFastGeoPrimitiveComponent* InComponentPendingRecreate);
	void RemoveFromComponentsPendingRecreate(FFastGeoPrimitiveComponent* InComponentPendingRecreate);
	void ProcessPendingRecreate();

	/**
	 * Start a job that will create render state asynchronously for a specified FastGeo container.
	 * @param FastGeo: FastGeo container.
	 */
	void PushAsyncCreateRenderStateJob(UFastGeoContainer* FastGeo);

	/**
	 * Start a job that will destroy render state asynchronously for a specified FastGeo container.
	 * @param FastGeo: FastGeo container.
	 */
	void PushAsyncDestroyRenderStateJob(UFastGeoContainer* FastGeo);

	/**
	 * Update progress of asynchronous render state creation and destruction.
	 * @param bWaitForCompletion : Whether to wait for pending asynchronous tasks to complete.
	 */
	void ProcessAsyncRenderStateJobs(bool bWaitForCompletion = false);

	/**
	 * Start a job that will create physics state asynchronously for a specified FastGeo container.
	 * @param FastGeo: FastGeo container.
	 */
	void PushAsyncCreatePhysicsStateJobs(UFastGeoContainer* FastGeo);

	/**
	 * Start a job that will destroy physics state asynchronously for a specified FastGeo container.
	 * @param FastGeo: FastGeo container.
	 */
	void PushAsyncDestroyPhysicsStateJobs(UFastGeoContainer* FastGeo);

	static bool IsEnableDebugView();

	bool IsWaitingForCompletion() const;

private:
	void OnUpdateLevelStreaming();
	void OnLevelStreamingStateChanged(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LevelIfLoaded, ELevelStreamingState PrevState, ELevelStreamingState NewState);
	void OnLevelStartedAddToWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* Level);
	void OnLevelStartedRemoveFromWorld(UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* Level);
	void OnLevelComponentsUpdated(UWorld* World, ULevel* Level);
	void OnLevelComponentsCleared(UWorld* World, ULevel* Level);
	void OnAddLevelToWorldExtension(ULevel* Level, const bool bWaitForCompletion, bool& bOutHasCompleted);
	void OnRemoveLevelFromWorldExtension(ULevel* Level, const bool bWaitForCompletion, bool& bOutHasCompleted);
#if DO_CHECK
	void OnLevelAddedToWorld(ULevel* Level, UWorld* World);
	void OnLevelRemovedFromWorld(ULevel* Level, UWorld* World);
	void CheckNoPendingTasks(ULevel* Level, UWorld* World);
#endif

	void ForEachHLODObjectInCell(const UWorldPartitionRuntimeCell*, TFunction<void(IWorldPartitionHLODObject*)>);

	void RequestAsyncRenderStateTasksBudget_Concurrent(float& OutAvailableTimeBudgetMS, int32& OutAvaiableComponentsBudget, int32& OutTimeEpoch);
	void CommitAsyncRenderStateTasksBudget_Concurrent(float InUsedTimeBudgetMS, int32& InUsedComponentsBudget, int32 TimeEpoch);

	FDelegateHandle Handle_OnLevelStreamingStateChanged;
	FDelegateHandle Handle_OnLevelBeginAddToWorld;
	FDelegateHandle Handle_OnLevelBeginRemoveFromWorld;
	FDelegateHandle Handle_OnForEachHLODObjectInCell;

	TArray<FWeakFastGeoComponent> ComponentsPendingRecreate;
	static bool bEnableDebugView;

	FRWLock Lock;
	int32 TimeEpoch = 0;
	float UsedAsyncRenderStateTasksTimeBudgetMS = 0;
	int32 UsedNumComponentsToProcessBudget = 0;
	bool bWaitingForCompletion = false;

	TUniquePtr<FFastGeoAsyncRenderStateJobQueue> AsyncRenderStateJobQueue;

	friend class UFastGeoContainer;
};
