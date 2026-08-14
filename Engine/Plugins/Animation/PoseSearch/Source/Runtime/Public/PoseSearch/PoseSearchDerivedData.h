// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "PoseSearch/PoseSearchDerivedDataKey.h"
#include "TickableEditorObject.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectSaveContext.h"

class UAnimSequenceBase;
class UPoseSearchDatabase;

namespace UE::PoseSearch
{
	struct FPoseSearchDatabaseAsyncCacheTask;
	class FPoseSearchDatabaseAsyncCacheTasks;

	enum class ERequestAsyncBuildFlag
	{
		NewRequest = 1 << 0,			// generates new key and kick off a task to get updated data (it'll cancel for eventual previous Database request, unless WaitPreviousRequest)
		ContinueRequest = 1 << 1,		// make sure there's associated data to the Database (doesn't have to be up to date)
		WaitForCompletion = 1 << 2,		// wait the termination of the NewRequest or ContinueRequest
	};
	ENUM_CLASS_FLAGS(ERequestAsyncBuildFlag);

	enum class EAsyncBuildIndexResult
	{
		InProgress,						// indexing in progress
		Success,						// the index has been built and the Database updated correctly
		Failed							// indexing failed
	};

	class FAsyncPoseSearchDatabasesManagement : public FTickableGameObject, public FTickableCookObject, public FGCObject
	{
	public:
		static POSESEARCH_API EAsyncBuildIndexResult RequestAsyncBuildIndex(const UPoseSearchDatabase* Database, ERequestAsyncBuildFlag Flag);

	private:
		POSESEARCH_API FAsyncPoseSearchDatabasesManagement();
		POSESEARCH_API ~FAsyncPoseSearchDatabasesManagement();

		static POSESEARCH_API FAsyncPoseSearchDatabasesManagement& Get();
		static POSESEARCH_API EAsyncBuildIndexResult RequestAsyncBuildIndexInternal(const UPoseSearchDatabase* Database, ERequestAsyncBuildFlag Flag);

		POSESEARCH_API void OnObjectModified(UObject* Object);
		POSESEARCH_API void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& TransactionObjectEvent);
		POSESEARCH_API void OnPackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent);
		POSESEARCH_API void OnPreObjectPropertyChanged(UObject* InObject, const FEditPropertyChain& InPropertyChain);
		POSESEARCH_API void OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent);

		POSESEARCH_API void Shutdown();
		POSESEARCH_API void StartQueuedTasks(int32 MaxActiveTasks);
		POSESEARCH_API void PreModified(UObject* Object);
		POSESEARCH_API void PostModified(UObject* Object);
		POSESEARCH_API void ClearPreCancelled();

		// Begin FTickableGameObject
		POSESEARCH_API virtual void Tick(float DeltaTime) override;
		virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
		POSESEARCH_API virtual TStatId GetStatId() const override;
		virtual bool IsTickableWhenPaused() const override { return true; }
		virtual bool IsTickableInEditor() const override { return true; }
		// End FTickableGameObject

		// Begin FTickableCookObject
		POSESEARCH_API virtual void TickCook(float DeltaTime, bool bCookCompete) override;
		// End FTickableCookObject

		// Begin FGCObject
		POSESEARCH_API void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FAsyncPoseSearchDatabaseManagement"); }
		// End FGCObject
		
		POSESEARCH_API void CollectDatabasesToSynchronize(UObject* Object);
		POSESEARCH_API void SynchronizeDatabases();
		
		// map of all those databases UAnimSequenceBase(s) that were containing or are containing UAnimNotifyState_PoseSearchBranchIn that requires synchronization
		typedef TMap<TWeakObjectPtr<UPoseSearchDatabase>, TArray<TWeakObjectPtr<UAnimSequenceBase>>> TDatabasesToSynchronize;
		typedef TPair<TWeakObjectPtr<UPoseSearchDatabase>, TArray<TWeakObjectPtr<UAnimSequenceBase>>> TDatabasesToSynchronizePair;
		TDatabasesToSynchronize DatabasesToSynchronize;

		FPoseSearchDatabaseAsyncCacheTasks& Tasks;
		FDelegateHandle OnObjectModifiedHandle;
		FDelegateHandle OnObjectTransactedHandle;
		FDelegateHandle OnPackageReloadedHandle;
		FDelegateHandle OnPreObjectPropertyChangedHandle;
		FDelegateHandle OnObjectPropertyChangedHandle;

		// Experimental, this feature might be removed without warning, not for production use
		FPartialKeyHashes PartialKeyHashes;

		static POSESEARCH_API FCriticalSection Mutex;
	};
} // namespace UE::PoseSearch

#endif // WITH_EDITOR
