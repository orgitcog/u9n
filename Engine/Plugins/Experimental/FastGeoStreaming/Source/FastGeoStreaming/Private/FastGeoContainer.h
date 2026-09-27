// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetUserData.h"
#include "Physics/Experimental/AsyncPhysicsStateProcessorInterface.h"
#include "PhysicsEngine/PhysicsBodyInstanceOwnerInterface.h"
#include "PhysicsEngine/BodyInstance.h"
#include "AI/Navigation/NavigationElement.h"
#include "FastGeoComponentCluster.h"
#include "FastGeoHLOD.h"
#include "UObject/ObjectPtr.h"

#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#endif // WITH_EDITOR

#include "FastGeoContainer.generated.h"

class UWorld;
class ULevel;
class URuntimeVirtualTexture;
class UStaticMesh;
class UMaterialInterface;
class FFastGeoComponent;
class FChaosUserDefinedEntity;

struct FFastGeoComponentClusterPendingState
{
	struct FRenderState
	{
		FRenderState()
		{
			Reset();
		}

		void Reset()
		{
			ComponentsToProcess.Reset();
			NumToProcess = 0;
			NumProcessed = 0;
			TotalNumProcessed = 0;
		}

		bool IsCompleted() const
		{
			return TotalNumProcessed >= ComponentsToProcess.Num();
		}

		TArray<FFastGeoPrimitiveComponent*> ComponentsToProcess;

		int32 NumToProcess;
		int32 NumProcessed;
		int32 TotalNumProcessed;
	} RenderState;

	struct FPhysicsState
	{
		FPhysicsState()
		{
			Reset();
		}

		void Reset()
		{
			ComponentsToProcess = nullptr;
			TotalNumProcessed = 0;
		}

		bool IsCompleted() const
		{
			return !ComponentsToProcess || (TotalNumProcessed >= ComponentsToProcess->Num());
		}

		TArray<FFastGeoComponent*>* ComponentsToProcess;
		std::atomic<int32> TotalNumProcessed;
	} PhysicsState;

	bool bIsInBlockingWait;
	
	FFastGeoComponentClusterPendingState()
	{
		Reset();
	}

	void Reset()
	{
		RenderState.Reset();
		PhysicsState.Reset();
		bIsInBlockingWait = false;
	}

	bool HasAnyPendingState() const
	{ 
		return !RenderState.IsCompleted() || !PhysicsState.IsCompleted();
	}
};

UCLASS(Within = Level)
class FASTGEOSTREAMING_API UFastGeoContainer : public UAssetUserData, public IAsyncPhysicsStateProcessor, public IPhysicsBodyInstanceOwnerResolver
{
	GENERATED_BODY()

public:
	void Register();
	void Unregister();
	bool IsRegistered() const;
	void Tick(bool bWaitForCompletion = false);
	bool HasAnyPendingTasks() const;
	bool HasAnyPendingCreateTasks() const;
	bool HasAnyPendingDestroyTasks() const;
	void PrecachePSOs();
	UWorld* GetWorld() const;
	ULevel* GetLevel() const;
	FFastGeoComponentCluster* GetComponentCluster(uint32 InComponentClusterTypeID, int32 InComponentClusterIndex);

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#endif
	//~ End UObject Interface

	//~Begin IPhysicsBodyInstanceOwnerResolver
	virtual IPhysicsBodyInstanceOwner* ResolvePhysicsBodyInstanceOwner(Chaos::FConstPhysicsObjectHandle PhysicsObject) override;
	//~End IPhysicsBodyInstanceOwnerResolver

#if WITH_EDITOR
	void OnCreated();
	void AddComponentCluster(FFastGeoComponentCluster* ComponentCluster);
#endif

	void InitializeDynamicProperties(bool bInitForPlay = false);

	void OnCreateRenderStateBegin_GameThread();
	void OnDestroyRenderStateBegin_GameThread();
	void OnCreateRenderStateEnd_GameThread();
	void OnDestroyRenderStateEnd_GameThread();
	void OnCreateRenderState_Concurrent();
	void OnDestroyRenderState_Concurrent();

	void OnCreatePhysicsStateBegin_GameThread();
	void OnDestroyPhysicsStateBegin_GameThread();

	//~ Begin IAsyncPhysicsStateProcessor
	virtual bool AllowsAsyncPhysicsStateCreation() const override final;
	virtual bool AllowsAsyncPhysicsStateDestruction() const override final;
	virtual bool IsAsyncPhysicsStateCreated() const override final;
	virtual UObject* GetAsyncPhysicsStateObject() const override final;
	virtual bool OnAsyncCreatePhysicsState(const UE::FTimeout& Timeout) override;
	virtual void OnAsyncCreatePhysicsStateEnd_GameThread() override;
	virtual void OnAsyncDestroyPhysicsStateBegin_GameThread() override;
	virtual bool OnAsyncDestroyPhysicsState(const UE::FTimeout& Timeout) override;
	virtual void OnAsyncDestroyPhysicsStateEnd_GameThread() override;
	virtual void CollectBodySetupsWithPhysicsMeshesToCreate(TSet<UBodySetup*>& OutBodySetups) const override;
	//~ End IAsyncPhysicsStateProcessor

	template <typename TComponentCluster = const FFastGeoComponentCluster, typename TFunc>
	void ForEachComponentCluster(TFunc&& InFunc) const
	{
		ForEachComponentCluster<const UFastGeoContainer, const TComponentCluster, TFunc>(this, Forward<TFunc>(InFunc));
	}

	template <typename TComponentCluster = FFastGeoComponentCluster, typename TFunc>
	void ForEachComponentCluster(TFunc&& InFunc)
	{
		ForEachComponentCluster<UFastGeoContainer, TComponentCluster, TFunc>(this, Forward<TFunc>(InFunc));
	}

	template <typename TComponentCluster = const FFastGeoComponentCluster, typename TFunc>
	bool ForEachComponentClusterBreakable(TFunc&& InFunc) const
	{
		return ForEachComponentClusterBreakable<const UFastGeoContainer, const TComponentCluster, TFunc>(this, Forward<TFunc>(InFunc));
	}

	template <typename TComponentCluster = FFastGeoComponentCluster, typename TFunc>
	bool ForEachComponentClusterBreakable(TFunc&& InFunc)
	{
		return ForEachComponentClusterBreakable<UFastGeoContainer, TComponentCluster, TFunc>(this, Forward<TFunc>(InFunc));
	}

protected:

	FFastGeoComponentClusterPendingState PendingCreate;
	FFastGeoComponentClusterPendingState PendingDestroy;

private:

#if WITH_EDITOR
	void CreateEditorProxyObjects();
	void CollectAssetReferences();
#endif
	void SerializeComponentClusters(FArchive& Ar);
	void RegisterToNavigationSystem();
	void UnregisterFromNavigationSystem();

	template <typename ThisType, typename TComponentCluster, typename TFunc>
	static void ForEachComponentCluster(ThisType* Self, TFunc&& InFunc)
	{
		auto ForEachArray = [&InFunc](auto& InArray)
		{
			typedef typename TDecay<decltype(InArray)>::Type ArrayType;
			using TArrayComponentCluster = typename ArrayType::ElementType;

			if constexpr (std::is_same<TComponentCluster, FFastGeoComponentCluster>::value)
			{
				for (auto& ComponentCluster : InArray)
				{
					InFunc(ComponentCluster);
				}
			}
			else if (TArrayComponentCluster::Type.IsA(TComponentCluster::Type))
			{
				for (auto& ComponentCluster : InArray)
				{
					checkSlow(ComponentCluster.template IsA<TComponentCluster>());
					Forward<TFunc>(InFunc)(*StaticCast<TComponentCluster*>(&ComponentCluster));
				}
			}
		};

		ForEachArray(Self->ComponentClusters);
		ForEachArray(Self->HLODs);
	}

	template <typename ThisType, typename TComponentCluster, typename TFunc>
	static bool ForEachComponentClusterBreakable(ThisType* Self, TFunc&& InFunc)
	{
		auto ForEachArray = [&InFunc](auto& InArray) -> bool
		{
			typedef typename TDecay<decltype(InArray)>::Type ArrayType;
			using TArrayComponentCluster = typename ArrayType::ElementType;

			if constexpr (std::is_same<TComponentCluster, FFastGeoComponentCluster>::value)
			{
				for (auto& ComponentCluster : InArray)
				{
					if (!InFunc(ComponentCluster))
					{
						return false;
					}
				}
			}
			else if (TArrayComponentCluster::Type.IsA(TComponentCluster::Type))
			{
				for (auto& ComponentCluster : InArray)
				{
					checkSlow(ComponentCluster.template IsA<TComponentCluster>());
					if (!Forward<TFunc>(InFunc)(*StaticCast<TComponentCluster*>(&ComponentCluster)))
					{
						return false;
					}
				}
			}

			return true;
		};

		return ForEachArray(Self->ComponentClusters) &&
			ForEachArray(Self->HLODs);
	}

	enum EPhysicsStateCreation
	{
		NotCreated,
		Creating,
		Created,
		Destroying
	};
	EPhysicsStateCreation PhysicsStateCreation = EPhysicsStateCreation::NotCreated;

	// Persistent data
	TArray<FFastGeoComponentCluster> ComponentClusters;
	TArray<FFastGeoHLOD> HLODs;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> Assets;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TSet<TObjectPtr<UObject>> EditorProxyObjects;
#endif

#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	bool bPrecachedPSOs = false;
	UE::Tasks::TTask<void> PrecachePSOsTask;
#endif

	// Transient data
	TArray<FFastGeoComponent*> CollisionComponents;
	TArray<FNavigationElementHandle> NavigationElementHandles;

	bool bIsRegistered = false;

	FRenderCommandFence DestroyFence;

	friend class FFastGeoGatherFastGeoContainerAssetRefsArchive;
};