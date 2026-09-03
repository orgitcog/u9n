// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/HLOD/HLODStats.h"
#include "WorldPartition/HLOD/HLODBuilder.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODObject.h"

#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#endif // WITH_EDITOR

#include "HLODActor.generated.h"

class UHLODLayer;
class UWorldPartitionHLODSourceActors;

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogHLODHash, Log, All);

UCLASS(NotPlaceable, MinimalAPI, HideCategories=(Rendering, Replication, Collision, Physics, Navigation, Networking, Input, Actor, LevelInstance, Cooking))
class AWorldPartitionHLOD : public AActor, public IWorldPartitionHLODObject
{
	GENERATED_UCLASS_BODY()

	friend class FHLODActorDesc;
	typedef TMap<FName, int64> FStats;

public:
	inline uint32 GetLODLevel() const { return LODLevel; }

	// ~Begin AActor interface
	virtual bool IsHLODRelevant() const override { return true; }
	// ~End AActor interface

	// ~Begin IWorldPartitionHLODObject interface
	virtual UObject* GetUObject() const override { return const_cast<AWorldPartitionHLOD*>(this); }
	ENGINE_API virtual ULevel* GetHLODLevel() const override;
	ENGINE_API virtual FString GetHLODNameOrLabel() const override;
	virtual bool DoesRequireWarmup() const override { return bRequireWarmup; }
	ENGINE_API virtual TSet<UObject*> GetAssetsToWarmup() const override;
	ENGINE_API virtual void SetVisibility(bool bIsVisible) override;
	ENGINE_API virtual const FGuid& GetSourceCellGuid() const override;
	ENGINE_API virtual bool IsStandalone() const override;
	ENGINE_API virtual const FGuid& GetStandaloneHLODGuid() const override;
	// ~End IWorldPartitionHLODObject interface

#if WITH_EDITOR
	ENGINE_API void SetHLODComponents(const TArray<UActorComponent*>& InHLODComponents);

	ENGINE_API void SetSourceActors(UWorldPartitionHLODSourceActors* InSourceActors);
	ENGINE_API UWorldPartitionHLODSourceActors* GetSourceActors();
	ENGINE_API const UWorldPartitionHLODSourceActors* GetSourceActors() const;

	ENGINE_API void SetInputStats(const FHLODBuildInputStats& InInputStats);
	ENGINE_API const FHLODBuildInputStats& GetInputStats() const;

	void SetRequireWarmup(bool InRequireWarmup) { bRequireWarmup = InRequireWarmup; }
	ENGINE_API void SetIsStandalone(bool bInIsStandalone);

	ENGINE_API void SetSourceCellGuid(const FGuid& InSourceCellGuid);
	inline void SetLODLevel(uint32 InLODLevel) { LODLevel = InLODLevel; }

	ENGINE_API const FBox& GetHLODBounds() const;
	ENGINE_API void SetHLODBounds(const FBox& InBounds);

	double GetMinVisibleDistance() const { return MinVisibleDistance; }
	void SetMinVisibleDistance(double InMinVisibleDistance) { MinVisibleDistance = InMinVisibleDistance; }

	ENGINE_API void BuildHLOD(bool bForceBuild = false);
	ENGINE_API uint32 GetHLODHash() const;

	ENGINE_API int64 GetStat(FName InStatName) const;
	void SetStat(FName InStatName, int64 InStatValue) { HLODStats.Add(InStatName, InStatValue); }
	void ResetStats() { HLODStats.Reset(); }

private:
	const FStats& GetStats() const { return HLODStats; }

#endif // WITH_EDITOR

protected:
	//~ Begin UObject Interface.
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API virtual bool IsEditorOnly() const override;
	ENGINE_API virtual bool NeedsLoadForServer() const override;
	ENGINE_API virtual void PostLoad() override;
#if WITH_EDITOR
	ENGINE_API virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	ENGINE_API virtual void RerunConstructionScripts() override;
	virtual bool CanEditChange(const FProperty* InProperty) const override { return false; }
	virtual bool CanEditChangeComponent(const UActorComponent* Component, const FProperty* InProperty) const override { return false; }
#endif
	//~ End UObject Interface.

	//~ Begin AActor Interface.
	ENGINE_API virtual void PreRegisterAllComponents() override;
	ENGINE_API virtual void BeginPlay() override;
	ENGINE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool SupportsIncrementalPreRegisterComponents() const override
	{
		return false;
	}
	virtual bool SupportsIncrementalPreUnregisterComponents() const override
	{
		return false;
	}
	virtual bool IsComponentRelevantForNavigation(UActorComponent* Component) const override
	{
		return false;
	}
	virtual bool IsRuntimeOnly() const override { return true; }
#if WITH_EDITOR
	ENGINE_API virtual TUniquePtr<class FWorldPartitionActorDesc> CreateClassActorDesc() const override;
	ENGINE_API virtual void GetStreamingBounds(FBox& OutRuntimeBounds, FBox& OutEditorBounds) const override;

	virtual bool ShouldImport(FStringView ActorPropString, bool IsMovingLevel) override { return false; }
	virtual bool IsLockLocation() const override { return true; }
	virtual bool IsUserManaged() const override { return false; }
#endif
	//~ End AActor Interface.

private:
#if WITH_EDITOR
	ENGINE_API void OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UWorldPartitionHLODSourceActors> SourceActors;

	UPROPERTY()
	FHLODBuildInputStats InputStats;

	UPROPERTY()
	FBox HLODBounds;

	UPROPERTY()
	double MinVisibleDistance;

	UPROPERTY()
	uint32 HLODHash;

	UPROPERTY()
	TMap<FName, int64> HLODStats;
#endif

	UPROPERTY()
	uint32 LODLevel;

	UPROPERTY()
	bool bRequireWarmup;

	UPROPERTY()
	FGuid SourceCellGuid;

	UPROPERTY()
	FGuid StandaloneHLODGuid;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TSoftObjectPtr<UWorldPartitionRuntimeCell> SourceCell_DEPRECATED;

	UPROPERTY()
	FName SourceCellName_DEPRECATED;

	UPROPERTY()
	TArray<FWorldPartitionRuntimeCellObjectMapping> HLODSubActors_DEPRECATED;

	UPROPERTY()
	TObjectPtr<const UHLODLayer> SubActorsHLODLayer_DEPRECATED;
#endif
};

DEFINE_ACTORDESC_TYPE(AWorldPartitionHLOD, FHLODActorDesc);
