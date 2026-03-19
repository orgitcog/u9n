// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "Grid/PCGGridDescriptor.h"
#include "UObject/WeakObjectPtr.h"

#include "PCGRuntimeGenScheduler.generated.h"

class APCGPartitionActor;
class APCGWorldActor;
class FPCGActorAndComponentMapping;
class FPCGGenSourceManager;
class IPCGGenSourceBase;
class ULevel;
class ULevelStreaming;
class UPCGComponent;
class UPCGSubsystem;
class UWorld;
enum class ELevelStreamingState : uint8;
enum class ELevelStreamingTargetState : uint8;

/** Used to inform what virtual textures to prime and on what grids they need to be present. */
USTRUCT(BlueprintType)
struct FPCGVirtualTexturePrimingInfo
{
	GENERATED_BODY()

	/** Virtual texture asset to be primed. */
	UPROPERTY(EditAnywhere, Category = "")
	TSoftObjectPtr<class URuntimeVirtualTexture> VirtualTexture;

	/** Largest grid on which this virtual texture is sampled. */
	UPROPERTY(EditAnywhere, Category = "")
	EPCGHiGenGrid Grid = EPCGHiGenGrid::Grid32;

	/** Desired world size (cm) of a texel in the primed virtual texture. Determines what mip level will be primed. */
	UPROPERTY(EditAnywhere, Category = "", meta = (ClampMin=0.1))
	float WorldTexelSize = 100.0f;
};

/**
 * The Runtime Generation Scheduler system handles the scheduling of PCG Components marked as GenerateAtRuntime.
 * It searches the level for Partitioned and Non-Partitioned components in range of the currently active
 * UPCGGenSources in the level, and schedules them efficiently based on their UPCGSchedulingPolicy, creating 
 * APCGPartitionActors as necessary to support hierarchical generation.
 *
 * APCGPartitionActors can be created/destroyed on demand or provided by a dynamically growing pool of actors. If
 * enabled, the pool will double in capacity anytime the number of available PAs reaches zero.
 * 
 * Components and PartitionActors created by the Runtime Generation Scheduler should be managed exclusively by the
 * runtime gen scheduling system.
 */
class FPCGRuntimeGenScheduler
{
	friend class UPCGSubsystem;

public:
	FPCGRuntimeGenScheduler(UWorld* InWorld, FPCGActorAndComponentMapping* InActorAndComponentMapping);
	~FPCGRuntimeGenScheduler();

	FPCGRuntimeGenScheduler(const FPCGRuntimeGenScheduler&) = delete;
	FPCGRuntimeGenScheduler(FPCGRuntimeGenScheduler&& other) = delete;
	FPCGRuntimeGenScheduler& operator=(const FPCGRuntimeGenScheduler& other) = delete;
	FPCGRuntimeGenScheduler& operator=(FPCGRuntimeGenScheduler&& other) = delete; 

	void Tick(APCGWorldActor* InPCGWorldActor, double InEndTime);

	void OnOriginalComponentRegistered(UPCGComponent* InOriginalComponent);
	void OnOriginalComponentUnregistered(UPCGComponent* InOriginalComponent);

	/** Destroy all runtime gen partition actors (both generated and pooled). Executed in next tick. */
	void FlushAllGeneratedActors() { bActorFlushRequested = true; }

protected:
	// Grid size, grid coords, original component, local componnent
	struct FGridGenerationKey
	{
		FGridGenerationKey(uint32 InGridSize, const FIntVector& InGridCoords, UPCGComponent* InOriginalComponent)
			: FGridGenerationKey(InGridSize, InGridCoords, InOriginalComponent, nullptr)
		{
		}

		FGridGenerationKey(uint32 InGridSize, const FIntVector& InGridCoords, UPCGComponent* InOriginalComponent, UPCGComponent* InLocalComponent)
			: GridSize(InGridSize)
			, GridCoords(InGridCoords)
			, OriginalComponent(InOriginalComponent)
			, CachedLocalComponent(InLocalComponent)
		{
			bUse2DGrid = InOriginalComponent ? InOriginalComponent->Use2DGrid() : true;
		}

		bool operator==(const FGridGenerationKey& Other) const
		{
			return GridSize == Other.GridSize
				&& GridCoords == Other.GridCoords
				&& bUse2DGrid == Other.bUse2DGrid
				&& OriginalComponent == Other.OriginalComponent;
		}

		bool IsValid() const { return !!OriginalComponent.ResolveObjectPtr(); }

		bool Use2DGrid() const { return bUse2DGrid; }
		uint32 GetGridSize() const { return GridSize; }
		FIntVector GetGridCoords() const { return GridCoords; }
		UPCGComponent* GetOriginalComponent() const { return OriginalComponent.ResolveObjectPtr(); }

		UPCGComponent* GetCachedLocalComponent() const { return CachedLocalComponent.Get(); }
		void SetCachedLocalComponent(UPCGComponent* InComponent) { CachedLocalComponent = InComponent; }

		FPCGGridDescriptor GetGridDescriptor() const;

		bool bUse2DGrid = false;
		uint32 GridSize = 0;
		FIntVector GridCoords = FIntVector(0);
		TObjectKey<UPCGComponent> OriginalComponent;

		// Optional/opportunistic cached local component if one has been created.
		TWeakObjectPtr<UPCGComponent> CachedLocalComponent;

		friend uint32 GetTypeHash(const FGridGenerationKey& In)
		{
			return HashCombine(GetTypeHash(In.GridCoords), GetTypeHash(In.GridSize), GetTypeHash(In.bUse2DGrid), GetTypeHash(In.OriginalComponent));
		}
	};

	/** Returns true if the scheduler should tick this frame. */
	bool ShouldTick();

	// Add constructor, get moves for the sets
	struct FTickQueueComponentsForGenerationInputs
	{
		const TSet<IPCGGenSourceBase*>* GenSources;
		const APCGWorldActor* PCGWorldActor;
		TSet<UPCGComponent*> AllPartitionedComponents;
		TSet<UPCGComponent*> AllNonPartitionedComponents;
		TSet<FGridGenerationKey> GeneratedComponents;
	};

	/** Queue nearby components for generation. */
	void TickQueueComponentsForGeneration(
		const FTickQueueComponentsForGenerationInputs& Inputs,
		TMap<FGridGenerationKey, double>& OutComponentsToGenerate);

	/** Perform immediate cleanup on components that become out of range. */
	void TickCleanup(const TSet<IPCGGenSourceBase*>& GenSources, const APCGWorldActor* InPCGWorldActor, double InEndTime);

	/** Schedule generation on components in priority order. */
	void TickScheduleGeneration(TMap<FGridGenerationKey, double>& ComponentsToGenerate);

	/** Request any required virtual textures to be primed within the necessary generation radius. */
	void TickRequestVirtualTexturePriming(const TSet<IPCGGenSourceBase*>& InGenSources);

	/** Detects changes in RuntimeGen CVars to keep the PA pool in a valid state. */
	void TickCVars(const APCGWorldActor* InPCGWorldActor);

	/** Cleanup all local components in the GeneratedComponents set. */
	void CleanupLocalComponents(const APCGWorldActor* InPCGWorldActor);

	/** Cleanup a component and remove it from the GeneratedComponents set. */
	void CleanupComponent(const FGridGenerationKey& GenerationKey, UPCGComponent* GeneratedComponent);

	/** Remove components from the GeneratedComponents set that have been marked for delayed refresh. Fully cleanup any that would be leaked otherwise. */
	void CleanupDelayedRefreshComponents();

	/** Refresh a generated component. bRemovePartitionActors will also perform a full cleanup of PAs and local components. */
	void RefreshComponent(UPCGComponent* InComponent, bool bRemovePartitionActors = false);
	
	/** Grabs an empty RuntimeGen PA from the PartitionActorPool and initializes it at the given GridSize and GridCoords. If no PAs are available in the pool,
	* the pool capacity will double and new PAs will be created.
	*/
	APCGPartitionActor* GetPartitionActorFromPool(const FPCGGridDescriptor& GridDescriptor, const FIntVector& GridCoords);

	/** Adds Count new RuntimeGen PAs to the Runtime PA pool. */
	void AddPartitionActorPoolCount(int32 Count);

	/** Destroy all pooled partition actors and rebuild with the NewPoolSize. */
	void ResetPartitionActorPoolToSize(uint32 NewPoolSize);

	/** Add UObject references for GC */
	void AddReferencedObjects(FReferenceCollector& Collector);

private:
	/** Called on world streaming events. */
	void OnLevelStreamingStateChanged(UWorld* InWorld, const ULevelStreaming* InLevelStreaming, ULevel* InLevelIfLoaded, ELevelStreamingState InPreviousState, ELevelStreamingState InNewState);

	/** Cleanup a local compnent from the Partition actor. */
	void CleanupLocalComponent(APCGPartitionActor* PartitionActor, UPCGComponent* LocalComponent);

	/** Cleanup the remaining comopnents that are not part of the GeneratedComponents */
	void CleanupRemainingComponents(UPCGComponent* InOriginalComponent);

	/** Tracks the generated components managed by the RuntimeGenScheduler. For local components, this generation key will hold the original component.
	* For non-partitioned components, the generation key should have unbounded grid size and (0, 0, 0) grid coordinates.
	*/
	TSet<FGridGenerationKey> GeneratedComponents;

	/** Tracks the components which should be removed from the GeneratedComponents set on the next tick. This helps us defer removal in case we get multiple
	* refreshes in a single tick. For example, a shallow refresh followed by a deep refresh would require the generated components to persist, otherwise we
	* will leak Partition Actors.
	*/
	TSet<FGridGenerationKey> GeneratedComponentsToRemove;

	// Local to member functions but hoisted for efficiency.
	TSet<IPCGGenSourceBase*> GenSources;
	TMap<FGridGenerationKey, double> ComponentsToGenerate;

	/** Pool of RuntimeGen PartitionActors used for hierarchical generation. */
	TArray<TObjectPtr<APCGPartitionActor>> PartitionActorPool;

	/** PartitionActorPoolSize represents the current maximum capacity of the PartitionActorPool. */
	int32 PartitionActorPoolSize = 0;

	FPCGActorAndComponentMapping* ActorAndComponentMapping = nullptr;
	FPCGGenSourceManager* GenSourceManager = nullptr;
	UPCGSubsystem* Subsystem = nullptr;
	UWorld* World = nullptr;

	bool bPoolingWasEnabledLastFrame = true;
	uint32 BasePoolSizeLastFrame = 0;

	/** Requests to flush all actors are deferred so they can be handled at a known time during tick. */
	bool bActorFlushRequested = false;

	/** Track the existence of runtime gen components to avoid unnecessary computation when there is no work to do. */
	bool bAnyRuntimeGenComponentsExist = false;
	bool bAnyRuntimeGenComponentsExistDirty = false;

	/** Setting up a PA calls APCGPartitionActor::AddGraphInstance which later calls RefreshComponent, which can create
	* an infinite refresh loop. To break this loop we write the OC pointer to this variable, and if Refresh gets called for
	* this OC we early out. Basically we don't respond to refresh calls for a component we are midway through setting up.
	*/
	const UPCGComponent* OriginalComponentBeingGenerated = nullptr;

	int32 FramesUntilGeneration = 0;

	// Local to TickQueueComponentsForGeneration, cached here for efficiency.
	using FStreamingCompleteQueryKey = TPair<FVector, float>;
	TMap<FStreamingCompleteQueryKey, bool> CachedStreamingQueryResults;
};
