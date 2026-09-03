// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeGen/PCGRuntimeGenScheduler.h"
#include "RuntimeGen/SchedulingPolicies/PCGSchedulingPolicyBase.h"

#include "PCGActorAndComponentMapping.h"
#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Grid/PCGGridDescriptor.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "RuntimeGen/GenSources/PCGGenSourceBase.h"
#include "RuntimeGen/GenSources/PCGGenSourceComponent.h"
#include "RuntimeGen/PCGGenSourceManager.h"

#include "DrawDebugHelpers.h"
#include "EngineDefines.h" // For UE_ENABLE_DEBUG_DRAWING
#include "Async/ParallelFor.h"
#include "Engine/LevelStreaming.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "Streaming/LevelStreamingDelegates.h"
#include "UObject/UObjectGlobals.h"
#include "VT/RuntimeVirtualTexture.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#if WITH_EDITOR
#include "EditorViewportClient.h"
#endif

namespace PCGRuntimeGenSchedulerConstants
{
	const FString PooledPartitionActorName = TEXT("PCGRuntimePartitionGridActor_POOLED");
	static constexpr float MinWorldVirtualTextureTexelSize = 0.1;
}

namespace PCGRuntimeGenSchedulerHelpers
{
	static TAutoConsoleVariable<bool> CVarRuntimeGenerationEnable(
		TEXT("pcg.RuntimeGeneration.Enable"),
		true,
		TEXT("Enable the RuntimeGeneration system."));

	static TAutoConsoleVariable<int32> CVarNumGeneratingComponentsAtSameTime(
		TEXT("pcg.RuntimeGeneration.NumGeneratingComponents"),
		16,
		TEXT("Defines the maximum number of runtime components that can generate at the same time."));

	static TAutoConsoleVariable<bool> CVarRuntimeGenerationEnableDebugging(
		TEXT("pcg.RuntimeGeneration.EnableDebugging"),
		false,
		TEXT("Enable verbose debug logging for the RuntimeGeneration system."));

	static TAutoConsoleVariable<bool> CVarRuntimeGenerationEnablePooling(
		TEXT("pcg.RuntimeGeneration.EnablePooling"),
		true,
		TEXT("Enable PartitionActor pooling for the RuntimeGeneration system."));

	static TAutoConsoleVariable<int32> CVarRuntimeGenerationBasePoolSize(
		TEXT("pcg.RuntimeGeneration.BasePoolSize"),
		100,
		TEXT("Defines the base PartitionActor pool size for the RuntimeGeneration system. Cannot be less than 1."));

	static FAutoConsoleCommand CommandFlushActorPool(
		TEXT("pcg.RuntimeGeneration.FlushActorPool"),
		TEXT("Flushes all pooled actors and regenerates all components."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
			{
				PCGSubsystem->GetRuntimeGenScheduler()->FlushAllGeneratedActors();
			}
		}));

	static TAutoConsoleVariable<bool> CVarHideActorsFromOutliner(
		TEXT("pcg.RuntimeGeneration.HideActorsFromOutliner"),
		true,
		TEXT("Hides partition actors from Scene Outliner."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
		{
			if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
			{
				PCGSubsystem->GetRuntimeGenScheduler()->FlushAllGeneratedActors();
			}
		}));

	static TAutoConsoleVariable<bool> CVarEnableWorldStreamingQueries(
		TEXT("pcg.RuntimeGeneration.EnableWorldStreamingQueries"),
		true,
		TEXT("Checks that the world is streamed in before triggering generation of local (partitioned) components."));

	static TAutoConsoleVariable<int32> CVarFramesBeforeFirstGenerate(
		TEXT("pcg.RuntimeGeneration.FramesBeforeFirstGenerate"),
		0,
		TEXT("Waits this many engine ticks before allowing runtime gen to schedule generation."));

	static TAutoConsoleVariable<bool> CVarEnableVirtualTexturePriming(
		TEXT("pcg.VirtualTexturePriming.Enable"),
		true,
		TEXT("Enable priming of virtual textures for PCG Components which request it."));

	static TAutoConsoleVariable<bool> CVarDebugDrawTexturePrimingBounds(
		TEXT("pcg.VirtualTexturePriming.DebugDrawTexturePrimingBounds"),
		false,
		TEXT("Draws debug boxes to indicate regions where PCG is requesting virtual texture priming."));
}

FPCGGridDescriptor FPCGRuntimeGenScheduler::FGridGenerationKey::GetGridDescriptor() const
{
	return FPCGGridDescriptor().SetGridSize(GetGridSize()).SetIsRuntime(true).SetIs2DGrid(Use2DGrid());
}

FPCGRuntimeGenScheduler::FPCGRuntimeGenScheduler(UWorld* InWorld, FPCGActorAndComponentMapping* InActorAndComponentMapping)
{
	check(InWorld && InActorAndComponentMapping);

	World = InWorld;
	Subsystem = UPCGSubsystem::GetInstance(World);
	ActorAndComponentMapping = InActorAndComponentMapping;
	GenSourceManager = new FPCGGenSourceManager(InWorld);
	bPoolingWasEnabledLastFrame = PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread();
	BasePoolSizeLastFrame = PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationBasePoolSize.GetValueOnAnyThread();
	FramesUntilGeneration = PCGRuntimeGenSchedulerHelpers::CVarFramesBeforeFirstGenerate.GetValueOnGameThread();

	FLevelStreamingDelegates::OnLevelStreamingStateChanged.AddRaw(this, &FPCGRuntimeGenScheduler::OnLevelStreamingStateChanged);
}

FPCGRuntimeGenScheduler::~FPCGRuntimeGenScheduler()
{
	delete GenSourceManager;
	GenSourceManager = nullptr;

	FLevelStreamingDelegates::OnLevelStreamingStateChanged.RemoveAll(this);
}

void FPCGRuntimeGenScheduler::Tick(APCGWorldActor* InPCGWorldActor, double InEndTime)
{
	check(InPCGWorldActor && GenSourceManager);

	// 0. Preamble - check if we should be active in this world and do lazy initialization.

	if (!ShouldTick())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::Tick);

	TickCVars(InPCGWorldActor);

	GenSources.Empty(GenSources.Num());

	if (bAnyRuntimeGenComponentsExist)
	{
		GenSourceManager->Tick();
		GenSources = GenSourceManager->GetAllGenSources(InPCGWorldActor);

		for (IPCGGenSourceBase* GenSource : GenSources)
		{
			if (ensure(GenSource))
			{
				GenSource->Tick();
			}
		}
	}

	if (!GenSources.IsEmpty() && PCGRuntimeGenSchedulerHelpers::CVarEnableVirtualTexturePriming.GetValueOnGameThread())
	{
		// @todo_pcg: To support VT priming outside of RuntimeGen, this should probably move outside of the RGS tick, and be ticked directly by the PCGSubsystem.
		// However, that would require also moving the GenSourceManager out of the RGS, 
		TickRequestVirtualTexturePriming(GenSources);
	}

	// Initialize RuntimeGen PA pool if necessary. If PoolSize is 0, then we have not initialized the pool yet.
	if (!GenSources.IsEmpty() || !GeneratedComponents.IsEmpty())
	{
		if (PartitionActorPoolSize == 0 && PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread())
		{
			AddPartitionActorPoolCount(PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationBasePoolSize.GetValueOnAnyThread());
		}
	}

	// Allow virtual texture priming to tick even when generation has not begun. This helps alleviate issues where we
	// generate before the virtual textures have finished streaming in, which is particularly problematic on load.
	if (FramesUntilGeneration > 0)
	{
		--FramesUntilGeneration;
		return;
	}

	CleanupDelayedRefreshComponents();

	// 1. Queue nearby components for generation.
	
	// Mapping of component + coordinates to priorities - needed to compute max priority over all gen sources.
	ComponentsToGenerate.Empty(128);

	if (!GenSources.IsEmpty())
	{
		FTickQueueComponentsForGenerationInputs Inputs;
		Inputs.GenSources = &GenSources;
		Inputs.PCGWorldActor = InPCGWorldActor;
		Inputs.AllPartitionedComponents = ActorAndComponentMapping->GetAllRegisteredPartitionedComponents();
		Inputs.AllNonPartitionedComponents = ActorAndComponentMapping->GetAllRegisteredNonPartitionedComponents();
		Inputs.GeneratedComponents = GeneratedComponents;

		TickQueueComponentsForGeneration(Inputs, ComponentsToGenerate);
	}

	// 2. Schedule cleanup on components that become out of range.

	if (!GeneratedComponents.IsEmpty())
	{
		TickCleanup(GenSources, InPCGWorldActor, InEndTime);
	}

	// 3. Schedule generation on components in priority order.
	if (!ComponentsToGenerate.IsEmpty())
	{
		// Sort components by priority (will be generated in descending order).
		ComponentsToGenerate.ValueSort([](double PrioA, double PrioB)->bool { return PrioA > PrioB; });

		// Only apply time budget to cleanup currently. Currently too easy to introduce latency issues so don't hold back generation new components
		// (and we already have CVarNumGeneratingComponentsAtSameTime to throttle new generations).
		TickScheduleGeneration(ComponentsToGenerate);
	}
}

bool FPCGRuntimeGenScheduler::ShouldTick()
{
	check(World && ActorAndComponentMapping);

	if (!PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnable.GetValueOnAnyThread())
	{
		return false;
	}

	// Disable tick of editor scheduling if in runtime or PIE.
	if (PCGHelpers::IsRuntimeOrPIE() && !World->IsGameWorld())
	{
		return false;
	}

#if WITH_EDITOR
	// If we're in an editor world, stop updating preview if the editor window/viewport is not active (follows
	// same behaviour as other things).
	if (!World->IsGameWorld())
	{
		FViewport* Viewport = GEditor ? GEditor->GetActiveViewport() : nullptr;
		FEditorViewportClient* ViewportClient = Viewport ? static_cast<FEditorViewportClient*>(Viewport->GetClient()) : nullptr;

		if (!ViewportClient || !ViewportClient->IsVisible())
		{
			return false;
		}
	}
#endif

	if (bAnyRuntimeGenComponentsExistDirty)
	{
		const bool bDidAnyRuntimeGenComponentsExist = bAnyRuntimeGenComponentsExist;
		bAnyRuntimeGenComponentsExist = ActorAndComponentMapping->AnyRuntimeGenComponentsExist();
		bAnyRuntimeGenComponentsExistDirty = false;

		if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
		{
			if (bDidAnyRuntimeGenComponentsExist != bAnyRuntimeGenComponentsExist)
			{
				if (bAnyRuntimeGenComponentsExist)
				{
					UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] THERE ARE NOW RUNTIME COMPONENTS IN THE LEVEL. SCHEDULER WILL BEGIN TICKING."));
				}
				else
				{
					UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] THERE ARE NO MORE RUNTIME COMPONENTS. SCHEDULER WILL ONLY TICK TO CLEANUP."));
				}
			}
		}
	}

	// We can stop ticking if there are no runtime gen components alive and there are no generated components that need cleaning up.
	if (!bAnyRuntimeGenComponentsExist && GeneratedComponents.IsEmpty() && GeneratedComponentsToRemove.IsEmpty())
	{
		return false;
	}

	return true;
}

void FPCGRuntimeGenScheduler::TickQueueComponentsForGeneration(
	const FTickQueueComponentsForGenerationInputs& Inputs,
	TMap<FGridGenerationKey, double>& OutComponentsToGenerate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::TickQueueComponentsForGeneration);

	check(Inputs.PCGWorldActor && Inputs.GenSources);

	// TODO: Thought - it would be possible to maintain a global maximum generation distance across all components
	// perhaps in the actor&comp mapping system, and then do a spatial query to get the components here.

	auto AddComponentToGenerate = [&OutComponentsToGenerate](FGridGenerationKey& InKey, const IPCGGenSourceBase* InGenSource, const UPCGSchedulingPolicyBase* InPolicy, const FBox& InComponentBounds, bool bInUse2DGrid)
	{
		const double PolicyPriority = InPolicy ? InPolicy->CalculatePriority(InGenSource, InComponentBounds, bInUse2DGrid) : 0.0;
		double Priority = FMath::Clamp(PolicyPriority, 0.0, 1.0);
		if (PolicyPriority != Priority)
		{
			UE_LOG(LogPCG, Warning, TEXT("Priority from runtime generation policy (%lf) outside [0.0, 1.0] range, clamped."), PolicyPriority);
		}

		// Generate largest grid to smallest (and unbounded is larger than any grid).
		const uint32 GridSize = InKey.GetGridSize();
		Priority += GridSize;

		double* ExistingPriority = OutComponentsToGenerate.Find(InKey);
		if (!ExistingPriority)
		{
			OutComponentsToGenerate.Add(InKey, Priority);
		}
		else if (Priority > *ExistingPriority)
		{
			// If this generation source prioritizes this grid cell higher, then bump the priority.
			*ExistingPriority = Priority;
		}
	};

	// Prepare streaming queries up front.
	TArray<FWorldPartitionStreamingQuerySource> StreamingQuerySources;
	StreamingQuerySources.Reserve(1);
	
	bool bCheckStreaming = PCGRuntimeGenSchedulerHelpers::CVarEnableWorldStreamingQueries.GetValueOnGameThread();
	const UWorldPartitionSubsystem* WorldPartitionSubsystem = bCheckStreaming ? UWorld::GetSubsystem<UWorldPartitionSubsystem>(Inputs.PCGWorldActor->GetWorld()) : nullptr;

	if (WorldPartitionSubsystem)
	{
		FWorldPartitionStreamingQuerySource& QuerySource = StreamingQuerySources.Emplace_GetRef();
		QuerySource.bSpatialQuery = true;
		QuerySource.bUseGridLoadingRange = false;
		QuerySource.bDataLayersOnly = false;
	}
	else
	{
		bCheckStreaming = false;
	}

	auto IsWorldStreamingComplete = [WorldPartitionSubsystem, &StreamingQuerySources, &CachedStreamingQueryResults=CachedStreamingQueryResults](const FVector& InLocation, float InGridSize)
	{
		FStreamingCompleteQueryKey Key{ InLocation, InGridSize };
		if (bool* FoundResult = CachedStreamingQueryResults.Find(Key))
		{
			return *FoundResult;
		}

		TRACE_CPUPROFILER_EVENT_SCOPE(IsWorldStreamingComplete);

		StreamingQuerySources[0].Radius = InGridSize / 2.0f;
		StreamingQuerySources[0].Location = InLocation;

		const bool bIsLoaded = WorldPartitionSubsystem->IsStreamingCompleted(EWorldPartitionRuntimeCellState::Activated, StreamingQuerySources, /*bExactState*/ false);

		CachedStreamingQueryResults.Add(Key, bIsLoaded);

		if (!bIsLoaded)
		{
			UE_LOG(LogPCG, Verbose, TEXT("Holding back generation of cell at (%.2f, %.2f, %.2f), grid size %f, due to world not loaded."), InLocation.X, InLocation.Y, InLocation.Z, InGridSize);
		}

		return bIsLoaded;
	};

#if UE_ENABLE_DEBUG_DRAWING
	const bool bDebugDrawGenerationSources = PCGSystemSwitches::CVarPCGDebugDrawGeneratedCells.GetValueOnGameThread();
#endif

	// Collect local components from all partitioned components.
	for (UPCGComponent* OriginalComponent : Inputs.AllPartitionedComponents)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::CollectLocalComponents);

		if (!ensure(OriginalComponent) || !OriginalComponent->GetGraph() || !OriginalComponent->bActivated)
		{
			continue;
		}

		const UPCGSchedulingPolicyBase* Policy = OriginalComponent->GetRuntimeGenSchedulingPolicy();

		// TODO: For each execution domain (for now only GenAtRuntime/dynamic), assuming we run Preview through this scheduler, which it seems like we will.
		if (OriginalComponent->IsManagedByRuntimeGenSystem())
		{
			ensure(Policy);
			bool bHasUnbounded = false;
			PCGHiGenGrid::FSizeArray GridSizes;
			ensure(PCGHelpers::GetGenerationGridSizes(OriginalComponent->GetGraph(), Inputs.PCGWorldActor, GridSizes, bHasUnbounded));

			if (GridSizes.IsEmpty() && !bHasUnbounded)
			{
				continue;
			}

			// For each relevant grid index, the largest grid size that has been marked in the runtime gen policy as depending on world streaming.
			PCGHiGenGrid::FSizeArray WorldStreamingQueryGridSizes;

			if (bCheckStreaming)
			{
				WorldStreamingQueryGridSizes.SetNumUninitialized(GridSizes.Num());

				for (int GridIndex = 0; GridIndex < GridSizes.Num(); ++GridIndex)
				{
					WorldStreamingQueryGridSizes[GridIndex] = PCGHiGenGrid::UninitializedGridSize();

					PCGHiGenGrid::FSizeArray ParentGridsDescending;
					OriginalComponent->GetGraph()->GetParentGridSizes(GridSizes[GridIndex], ParentGridsDescending);
					for (uint32 ParentGridSize : ParentGridsDescending)
					{
						if (OriginalComponent->DoesGridDependOnWorldStreaming(ParentGridSize) && ensure(ParentGridSize > GridSizes[GridIndex]))
						{
							WorldStreamingQueryGridSizes[GridIndex] = ParentGridSize;
							break;
						}
					}

					if (WorldStreamingQueryGridSizes[GridIndex] == PCGHiGenGrid::UninitializedGridSize() && OriginalComponent->DoesGridDependOnWorldStreaming(GridSizes[GridIndex]))
					{
						WorldStreamingQueryGridSizes[GridIndex] = GridSizes[GridIndex];
					}
				}
			}
			else
			{
				WorldStreamingQueryGridSizes.SetNumZeroed(GridSizes.Num());
			}

			const EPCGHiGenGrid MaxGrid = bHasUnbounded ? EPCGHiGenGrid::Unbounded : PCGHiGenGrid::GridSizeToGrid(GridSizes[0]);
			const double MaxGenerationRadius = OriginalComponent->GetGenerationRadiusFromGrid(MaxGrid);

			for (const IPCGGenSourceBase* GenSource : *Inputs.GenSources)
			{
				const TOptional<FVector> GenSourcePositionOptional = GenSource->GetPosition();
				if (!GenSourcePositionOptional.IsSet())
				{
					continue;
				}

#if UE_ENABLE_DEBUG_DRAWING
				if (bDebugDrawGenerationSources)
				{
					if (TOptional<FVector> Pos = GenSource->GetPosition())
					{
						if (const UObject* GenSourceObject = Cast<UObject>(GenSource))
						{
							DrawDebugString(World, *GenSourcePositionOptional, GenSourceObject->GetName(), /*TestBaseActor=*/nullptr, FColor::Red, /*Duration=*/0.0f);
						}

						DrawDebugSphere(World, *GenSourcePositionOptional, MaxGenerationRadius, 64, FColor::Red, /*bPersistentLines=*/false, /*LifeTime=*/0.0f);
					}
				}
#endif // UE_ENABLE_DEBUG_DRAWING

				const FVector GenSourcePosition = GenSourcePositionOptional.GetValue();
				const FBox OriginalComponentBounds = OriginalComponent->GetGridBounds();
				const bool bIs2DGrid = OriginalComponent->Use2DGrid();

				FVector ModifiedGenSourcePosition = GenSourcePosition;
				if (bIs2DGrid)
				{
					ModifiedGenSourcePosition.Z = OriginalComponentBounds.Min.Z;
				}

				const double DistanceSquared = OriginalComponentBounds.ComputeSquaredDistanceToPoint(ModifiedGenSourcePosition);

				if (DistanceSquared > MaxGenerationRadius * MaxGenerationRadius)
				{
					// GenSource is not within range of the component, skip!
					continue;
				}

				if (bHasUnbounded)
				{
					// TODO check that unbounded is loaded, if dependent on world state. Or temporarily don't allow querying world state in unbounded, for runtime gen.
					
					// Ignore components that have already been generated or marked for generation. Unbounded grid size means not-partitioned.
					FGridGenerationKey Key(PCGHiGenGrid::UnboundedGridSize(), FIntVector(0), OriginalComponent);
					if (!Inputs.GeneratedComponents.Contains(Key) &&
						(!Policy || Policy->ShouldGenerate(GenSource, OriginalComponentBounds, bIs2DGrid)))
					{
						check(Key.GetGridDescriptor().Is2DGrid() == bIs2DGrid);
						AddComponentToGenerate(Key, GenSource, Policy, OriginalComponentBounds, bIs2DGrid);
					}
				}

				// TODO: once one of the larger grid sizes is out of range, we can forego checking any smaller grid sizes. they can't possibly be closer!
				// This assumes we enforce generation radii to increase monotonically.
				for (int GridIndex = 0; GridIndex < GridSizes.Num(); ++GridIndex)
				{
					const uint32 GridSize = GridSizes[GridIndex];

					ensure(PCGHiGenGrid::IsValidGridSize(GridSize));

					const FIntVector GenSourceGridPosition = UPCGActorHelpers::GetCellCoord(GenSourcePosition, GridSize, bIs2DGrid);
					const double GenerationRadius = OriginalComponent->GetGenerationRadiusFromGrid(PCGHiGenGrid::GridSizeToGrid(GridSize));
					const int32 GridRadius = FMath::CeilToInt32(GenerationRadius / GridSize); // Radius discretized to # of grid cells.
					const int32 VerticalGridRadius = bIs2DGrid ? 0 : GridRadius; // Flatten the vertical grid radius in the 2D case.

					const double HalfGridSize = GridSize / 2.0f;
					FVector HalfExtent(HalfGridSize, HalfGridSize, HalfGridSize);

					if (bIs2DGrid)
					{
						// In case of 2D grid, it's like the actor has infinite bounds on the Z axis.
						HalfExtent.Z = HALF_WORLD_MAX1;
					}

					// TODO: Perhaps rasterize sphere instead of walking a naive cube. although maybe the perf on that isn't worthwhile.
					for (int32 Z = GenSourceGridPosition.Z - VerticalGridRadius; Z <= GenSourceGridPosition.Z + VerticalGridRadius; ++Z)
					{
						for (int32 Y = GenSourceGridPosition.Y - GridRadius; Y <= GenSourceGridPosition.Y + GridRadius; ++Y)
						{
							for (int32 X = GenSourceGridPosition.X - GridRadius; X <= GenSourceGridPosition.X + GridRadius; ++X)
							{
								FIntVector GridCoords(X, Y, Z);
								FGridGenerationKey Key(GridSize, GridCoords, OriginalComponent);

								// Ignore components that have already been generated or marked for generation.
								if (Inputs.GeneratedComponents.Find({ GridSize, GridCoords, OriginalComponent }))
								{
									continue;
								}

								const FVector Center = FVector(GridCoords.X + 0.5, GridCoords.Y + 0.5, GridCoords.Z + 0.5) * GridSize;
								const FBox CellBounds(Center - HalfExtent, Center + HalfExtent);

								// Overlap cell with the partitioned component.
								const FBox IntersectedBounds = OriginalComponentBounds.Overlap(CellBounds);
								if (!IntersectedBounds.IsValid || IntersectedBounds.GetVolume() <= UE_DOUBLE_SMALL_NUMBER)
								{
									continue;
								}

								if (Key.GetGridDescriptor().Is2DGrid())
								{
									ModifiedGenSourcePosition.Z = IntersectedBounds.Min.Z;
								}

								// Verify the grid cell actually lies within the generation radius.
								// TODO: this is no longer necessary if we rasterize the sphere instead.
								const double LocalDistanceSquared = IntersectedBounds.ComputeSquaredDistanceToPoint(ModifiedGenSourcePosition);
								if (LocalDistanceSquared <= GenerationRadius * GenerationRadius && 
									Policy->ShouldGenerate(GenSource, IntersectedBounds, Key.GetGridDescriptor().Is2DGrid()))
								{
									bool bStreamingComplete = true;

									if (WorldStreamingQueryGridSizes[GridIndex] == GridSize)
									{
										bStreamingComplete = IsWorldStreamingComplete(Center, GridSize);

										if (!bStreamingComplete)
										{
											UE_LOG(LogPCG, VeryVerbose, TEXT("Cell %u (%d, %d, %d) rejected as world is not fully loaded."),
												GridSize, GridCoords.X, GridCoords.Y, GridCoords.Z);
										}
									}
									else if (WorldStreamingQueryGridSizes[GridIndex] != PCGHiGenGrid::UninitializedGridSize())
									{
										// Check world loaded status using the pre-calculated largest parent grid that depends on world streaming.
										const uint32 ParentGridSize = WorldStreamingQueryGridSizes[GridIndex];
										const FVector ParentCenter = UPCGActorHelpers::GetCellCenter(Center, ParentGridSize, Key.GetGridDescriptor().Is2DGrid());

										bStreamingComplete = IsWorldStreamingComplete(ParentCenter, ParentGridSize);

										if (!bStreamingComplete)
										{
											UE_LOG(LogPCG, VeryVerbose, TEXT("Cell %u (%d, %d, %d) rejected as parent on grid size %u is not fully loaded."),
												GridSize, GridCoords.X, GridCoords.Y, GridCoords.Z, ParentGridSize);
										}
									}

									if (bStreamingComplete)
									{
										AddComponentToGenerate(Key, GenSource, Policy, IntersectedBounds, Key.GetGridDescriptor().Is2DGrid());
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// Collect all non-partitioned components.
	for (UPCGComponent* OriginalComponent : Inputs.AllNonPartitionedComponents)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::CollectNonPartitionedComponents);

		if (!ensure(OriginalComponent) || !OriginalComponent->GetGraph() || !OriginalComponent->bActivated)
		{
			continue;
		}

		// The generation key for a non-partitioned component should always have unbounded grid size and 0,0,0 cell coord.
		if (Inputs.GeneratedComponents.Contains({ PCGHiGenGrid::UnboundedGridSize(), FIntVector(0), OriginalComponent }))
		{
			continue;
		}

		const UPCGSchedulingPolicyBase* Policy = OriginalComponent->GetRuntimeGenSchedulingPolicy();

		// TODO: For each execution domain (for now only GenAtRuntime/dynamic), assuming we run Preview through this scheduler, which it seems like we will.
		if (OriginalComponent->IsManagedByRuntimeGenSystem())
		{
			ensure(Policy);
			// Unbounded will grab the base GenerationRadius used for non-partitioned and unbounded.
			const double MaxGenerationRadius = OriginalComponent->GetGenerationRadiusFromGrid(EPCGHiGenGrid::Unbounded);

			for (const IPCGGenSourceBase* GenSource : *Inputs.GenSources)
			{
				const TOptional<FVector> GenSourcePositionOptional = GenSource->GetPosition();
				if (!GenSourcePositionOptional.IsSet())
				{
					continue;
				}

				const FVector GenSourcePosition = GenSourcePositionOptional.GetValue();
				const FBox OriginalComponentBounds = OriginalComponent->GetGridBounds();

				FVector ModifiedGenSourcePosition = GenSourcePosition;
				if (OriginalComponent->Use2DGrid())
				{
					ModifiedGenSourcePosition.Z = OriginalComponentBounds.Min.Z;
				}

				const double DistanceSquared = OriginalComponentBounds.ComputeSquaredDistanceToPoint(ModifiedGenSourcePosition);

				// Max radius for a non-partitioned component is just the base GenerationRadius.
				if (DistanceSquared <= MaxGenerationRadius * MaxGenerationRadius && 
					(!Policy || Policy->ShouldGenerate(GenSource, OriginalComponentBounds, /*bUse2DGrid=*/false)))
				{
					// Unbounded grid size means not-partitioned.
					FGridGenerationKey Key(PCGHiGenGrid::UnboundedGridSize(), FIntVector(0), OriginalComponent);
					AddComponentToGenerate(Key, GenSource, Policy, OriginalComponentBounds, /*bUse2DGrid=*/false);
				}
			}
		}
	}
}

void FPCGRuntimeGenScheduler::TickCleanup(const TSet<IPCGGenSourceBase*>& InGenSources, const APCGWorldActor* InPCGWorldActor, double InEndTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::TickCleanup);

	check(ActorAndComponentMapping && InPCGWorldActor);

	auto CheckIfAllGenSourcesWantToCleanup = [&InGenSources](const UPCGSchedulingPolicyBase* Policy, const FPCGGridDescriptor& GridDescriptor, const FBox& GridBounds, const double& CleanupRadiusSquared)
	{
		bool bAllGenSourcesWantToCleanup = true;

		for (const IPCGGenSourceBase* GenSource : InGenSources)
		{
			if (!ensure(GenSource))
			{
				continue;
			}

			const TOptional<FVector> GenSourcePositionOptional = GenSource->GetPosition();
			if (!GenSourcePositionOptional.IsSet())
			{
				continue;
			}

			FVector GenSourcePosition = GenSourcePositionOptional.GetValue();

			// Only consider 2D distance when using a 2D grid.
			if (GridDescriptor.Is2DGrid())
			{
				GenSourcePosition.Z = GridBounds.Min.Z;
			}

			const double SquaredDistToGenSource = GridBounds.ComputeSquaredDistanceToPoint(GenSourcePosition);

			// If the distance to the gen source is greater than the cleanup radius, it means this generation source votes for the component to be cleaned up.
			// Otherwise, the gen source might still vote for culling regardless.
			if (SquaredDistToGenSource <= CleanupRadiusSquared && 
				(!Policy || !Policy->ShouldCull(GenSource, GridBounds, GridDescriptor.Is2DGrid())))
			{
				bAllGenSourcesWantToCleanup = false;
				break;
			}
		}

		return bAllGenSourcesWantToCleanup;
	};

	TArray<FGridGenerationKey> GeneratedComponentsArray = GeneratedComponents.Array();
	const int32 NumGeneratedComponents = GeneratedComponentsArray.Num();

	// Generated Entry Key, Local/Generated Component
	using PCGComponentToClean = TTuple<FGridGenerationKey, UPCGComponent*>;
	TArray<PCGComponentToClean, TInlineAllocator<256>> ComponentsToClean;
	TArray<FGridGenerationKey, TInlineAllocator<16>> InvalidKeys;

	ComponentsToClean.SetNumUninitialized(NumGeneratedComponents);
	InvalidKeys.SetNumUninitialized(NumGeneratedComponents);

	std::atomic<int> ComponentsToCleanCounter = 0;
	std::atomic<int> InvalidKeysCounter = 0;

	ParallelFor(NumGeneratedComponents, [&GeneratedComponentsArray, &ComponentsToClean, &InvalidKeys, &ComponentsToCleanCounter, &InvalidKeysCounter, ActorAndComponentMapping=ActorAndComponentMapping, &CheckIfAllGenSourcesWantToCleanup](int32 Index)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SelectComponentForCleanup);

		const FGridGenerationKey& GenerationKey = GeneratedComponentsArray[Index];

		if (!GenerationKey.IsValid())
		{
			const int WriteIndex = InvalidKeysCounter.fetch_add(1);
			InvalidKeys[WriteIndex] = GenerationKey;
			return;
		}

		const FPCGGridDescriptor GridDescriptor = GenerationKey.GetGridDescriptor();
		const EPCGHiGenGrid Grid = GridDescriptor.GetHiGenGrid();
		const FIntVector& GridCoords = GenerationKey.GetGridCoords();
		UPCGComponent* OriginalComponent = GenerationKey.GetOriginalComponent();
		check(OriginalComponent);

		const UPCGSchedulingPolicyBase* Policy = OriginalComponent->GetRuntimeGenSchedulingPolicy();
		ensure(Policy);

		const double CleanupRadius = OriginalComponent->GetCleanupRadiusFromGrid(Grid);
		const double CleanupRadiusSquared = CleanupRadius * CleanupRadius;

		// If the Grid is unbounded, we have a non-partitioned or unbounded component.
		if (Grid == EPCGHiGenGrid::Unbounded)
		{
			if (!OriginalComponent->bActivated)
			{
				const int WriteIndex = ComponentsToCleanCounter.fetch_add(1);
				ComponentsToClean[WriteIndex] = { GenerationKey, OriginalComponent };
				return;
			}

			const FBox GridBounds = OriginalComponent->GetGridBounds();

			// Only clean up if all generation sources agreed to clean up.
			if(CheckIfAllGenSourcesWantToCleanup(Policy, GridDescriptor, GridBounds, CleanupRadiusSquared))
			{
				const int WriteIndex = ComponentsToCleanCounter.fetch_add(1);
				ComponentsToClean[WriteIndex] = { GenerationKey, OriginalComponent };
				return;
			}
		}
		// Otherwise, we have a local component.
		else
		{
			UPCGComponent* LocalComponent = GenerationKey.GetCachedLocalComponent();
			if (!LocalComponent)
			{
				LocalComponent = ActorAndComponentMapping->GetLocalComponent(GridDescriptor, GridCoords, OriginalComponent);
			}

			APCGPartitionActor* PartitionActor = LocalComponent ? Cast<APCGPartitionActor>(LocalComponent->GetOwner()) : nullptr;
			if (!PartitionActor || !OriginalComponent->bActivated)
			{
				// Attempt to clean even in failure case to avoid leaking resources.
				const int WriteIndex = ComponentsToCleanCounter.fetch_add(1);
				ComponentsToClean[WriteIndex] = { GenerationKey, LocalComponent };
				return;
			}

			const FBox GridBounds = LocalComponent->GetGridBounds();

			// Only clean up if all generation sources agreed to clean up.
			if (CheckIfAllGenSourcesWantToCleanup(Policy, GridDescriptor, GridBounds, CleanupRadiusSquared))
			{
				const int WriteIndex = ComponentsToCleanCounter.fetch_add(1);
				ComponentsToClean[WriteIndex] = { GenerationKey, LocalComponent };
				return;
			}
		}
	});

	const int NumInvalidKeys = InvalidKeysCounter.load();
	const int NumComponentsToClean = ComponentsToCleanCounter.load();

	for (int I = 0; I < NumInvalidKeys; ++I)
	{
		GeneratedComponents.Remove(InvalidKeys[I]);
	}

	for (int I = 0; I < NumComponentsToClean; ++I)
	{
		const PCGComponentToClean& ComponentToClean = ComponentsToClean[I];
		CleanupComponent(ComponentToClean.Get<0>(), ComponentToClean.Get<1>());

		if (FPlatformTime::Seconds() >= InEndTime)
		{
			UE_LOG(LogPCG, Verbose, TEXT("FPCGRuntimeGenScheduler: Time budget exceeded, aborted after cleaning up %d / %d components"), (I + 1), NumComponentsToClean);
			break;
		}
	}
}

void FPCGRuntimeGenScheduler::TickScheduleGeneration(TMap<FGridGenerationKey, double>& InComponentsToGenerate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::TickScheduleGeneration);

	check(Subsystem && ActorAndComponentMapping);

	// Count number of currently generating components
	int NumGenerating = 0;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::TickScheduleGeneration::CountingCurrentlyGenerating);
		for (const FGridGenerationKey& Key : GeneratedComponents)
		{
			if (!Key.IsValid())
			{
				continue;
			}

			UPCGComponent* Component = (Key.GetGridDescriptor().GetHiGenGrid() == EPCGHiGenGrid::Unbounded) ? Key.GetOriginalComponent() : Key.GetCachedLocalComponent();

			if (Component && Component->IsGenerating())
			{
				++NumGenerating;
			}
		}
	}

	const int MaxNumGenerating = FMath::Max(1, PCGRuntimeGenSchedulerHelpers::CVarNumGeneratingComponentsAtSameTime.GetValueOnAnyThread());

	GeneratedComponents.Reserve(GeneratedComponents.Num() + InComponentsToGenerate.Num());

	for (TPair<FGridGenerationKey, double>& GenerateEntry : InComponentsToGenerate)
	{
		if (NumGenerating >= MaxNumGenerating)
		{
			break;
		}

		FGridGenerationKey& Key = GenerateEntry.Key;
		const double Priority = GenerateEntry.Value;

		const FPCGGridDescriptor GridDescriptor = Key.GetGridDescriptor();
		const EPCGHiGenGrid Grid = GridDescriptor.GetHiGenGrid();

		const FIntVector GridCoords = GenerateEntry.Key.GetGridCoords();
		UPCGComponent* OriginalComponent = GenerateEntry.Key.GetOriginalComponent();
		check(OriginalComponent);

		// If the Grid is unbounded, we have a non-partitioned or unbounded component.
		if (Grid == EPCGHiGenGrid::Unbounded)
		{
			if (ensure(OriginalComponent) && !OriginalComponent->IsGenerating())
			{
				if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread() && OriginalComponent->GetOwner())
				{
					UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] GENERATE: '%s' (priority %lf)"), *OriginalComponent->GetOwner()->GetActorNameOrLabel(), Priority);
				}

				// Force to refresh if the component is already generated.
				OriginalComponent->GenerateLocal(EPCGComponentGenerationTrigger::GenerateAtRuntime, /*bForce=*/true, Grid);
			}
		}
		// Otherwise we have a local component.
		else
		{
			// Grab local component and PA if they exist already.
			UPCGComponent* LocalComponent = ActorAndComponentMapping->GetLocalComponent(GridDescriptor, GridCoords, OriginalComponent);
			TObjectPtr<APCGPartitionActor> PartitionActor = LocalComponent ? Cast<APCGPartitionActor>(LocalComponent->GetOwner()) : nullptr;

			if (!LocalComponent || !ensure(PartitionActor))
			{
				// Local component & PA do not exist, create them.
				if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread())
				{
					// Get RuntimeGenPA from pool.
					PartitionActor = GetPartitionActorFromPool(GridDescriptor, GridCoords);

					if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
					{
						UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] UNPOOL PARTITION ACTOR: '%s' (priority %lf, %d remaining out of %d)"),
							*APCGPartitionActor::GetPCGPartitionActorName(GridDescriptor, GridCoords),
							Priority,
							PartitionActorPool.Num(),
							PartitionActorPoolSize);
					}
				}
				else
				{
					if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
					{
						UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] CREATE PARTITION ACTOR: '%s' (priority %lf)"),
							*APCGPartitionActor::GetPCGPartitionActorName(GridDescriptor, GridCoords),
							Priority);
					}

					// Find or Create RuntimeGenPA.
					PartitionActor = Subsystem->FindOrCreatePCGPartitionActor(
						GridDescriptor,
						GridCoords,
						/*bCanCreateActor=*/true,
						PCGRuntimeGenSchedulerHelpers::CVarHideActorsFromOutliner.GetValueOnAnyThread());
				}

				if (!ensure(PartitionActor))
				{
					continue;
				}

				// Update component mapping for this PA (add local component).
				{
					FWriteScopeLock WriteLock(ActorAndComponentMapping->ComponentToPartitionActorsMapLock);
					TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ActorAndComponentMapping->ComponentToPartitionActorsMap.Find(OriginalComponent);

					if (!PartitionActorsPtr)
					{
						PartitionActorsPtr = &ActorAndComponentMapping->ComponentToPartitionActorsMap.Emplace(OriginalComponent);
					}

					// Log this original component before setting up the PA, so that we early out from RefreshComponent if it gets called
					// in the AddGraphInstance call below.
					OriginalComponentBeingGenerated = OriginalComponent;

					PartitionActor->AddGraphInstance(OriginalComponent);

					OriginalComponentBeingGenerated = nullptr;

					PartitionActorsPtr->Add(PartitionActor);
				}

				// Create local component.
				LocalComponent = PartitionActor->GetLocalComponent(OriginalComponent);
			}

			if (ensure(LocalComponent) && !LocalComponent->IsGenerating())
			{
				if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
				{
					UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] GENERATE: '%s' (priority %lf)"), *PartitionActor->GetActorNameOrLabel(), Priority);
				}

				// Higen graphs may have data links from original component to local components. The original component will be given a higher priority than local
				// components and will start generating first. If it is currently generating, local component needs to take a dependency to ensure execution completes.
				TArray<FPCGTaskId> Dependencies;
				if (OriginalComponent->IsGenerating() && OriginalComponent->GetGraph() && OriginalComponent->GetGraph()->IsHierarchicalGenerationEnabled())
				{
					const FPCGTaskId TaskId = OriginalComponent->GetGenerationTaskId();

					if (TaskId != InvalidPCGTaskId)
					{
						Dependencies.Add(TaskId);
					}
				}

				// Force to refresh if the component is already generated.
				LocalComponent->GenerateLocal(EPCGComponentGenerationTrigger::GenerateAtRuntime, /*bForce=*/true, LocalComponent->GetGenerationGrid(), Dependencies);

				Key.SetCachedLocalComponent(LocalComponent);
			}
		}

		GeneratedComponents.Add(Key);
		++NumGenerating;
	}
}

void FPCGRuntimeGenScheduler::TickRequestVirtualTexturePriming(const TSet<IPCGGenSourceBase*>& InGenSources)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::TickRequestVirtualTexturePriming);

	check(ActorAndComponentMapping);

	TArray<const UPCGComponent*> OriginalComponents;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GatherOriginalComponents);

		TSet<UPCGComponent*> PartitionedComponents = ActorAndComponentMapping->GetAllRegisteredPartitionedComponents();
		TSet<UPCGComponent*> NonPartitionedComponents = ActorAndComponentMapping->GetAllRegisteredNonPartitionedComponents();

		OriginalComponents.Reserve(PartitionedComponents.Num() + NonPartitionedComponents.Num());

		auto CanComponentRequestVirtualTexturePriming = [](const UPCGComponent* OriginalComponent)
		{
			return OriginalComponent && OriginalComponent->bActivated && OriginalComponent->IsManagedByRuntimeGenSystem() && OriginalComponent->GetGraphInstance();
		};

		for (UPCGComponent* OriginalComponent : PartitionedComponents)
		{
			if (CanComponentRequestVirtualTexturePriming(OriginalComponent))
			{
				OriginalComponents.Add(MoveTemp(OriginalComponent));
			}
		}

		for (UPCGComponent* OriginalComponent : NonPartitionedComponents)
		{
			if (CanComponentRequestVirtualTexturePriming(OriginalComponent))
			{
				OriginalComponents.Add(MoveTemp(OriginalComponent));
			}
		}
	}

	if (OriginalComponents.IsEmpty())
	{
		return;
	}

	TMap<TSoftObjectPtr<URuntimeVirtualTexture>, TArray<URuntimeVirtualTextureComponent*>> VirtualTextureToComponents;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindVirtualTextureComponents);

		// @todo_pcg: We could avoid polling for VT components every frame if they were registered somewhere instead.
		TArray<UObject*> FoundComponents;
		GetObjectsOfClass(URuntimeVirtualTextureComponent::StaticClass(), FoundComponents, /*bIncludeDerivedClasses=*/false, RF_ClassDefaultObject, EInternalObjectFlags::Garbage);

		for (UObject* FoundComponent : FoundComponents)
		{
			if (FoundComponent && FoundComponent->GetWorld() == World)
			{
				URuntimeVirtualTextureComponent* VirtualTextureComponent = Cast<URuntimeVirtualTextureComponent>(FoundComponent);
				URuntimeVirtualTexture* VirtualTexture = VirtualTextureComponent ? VirtualTextureComponent->GetVirtualTexture() : nullptr;

				if (VirtualTexture)
				{
					TArray<URuntimeVirtualTextureComponent*>& VirtualTextureComponents = VirtualTextureToComponents.FindOrAdd(VirtualTexture);
					VirtualTextureComponents.Add(VirtualTextureComponent);
				}
			}
		}
	}

	if (VirtualTextureToComponents.IsEmpty())
	{
		return;
	}

	for (const UPCGComponent* OriginalComponent : OriginalComponents)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RequestVirtualTexturePrimingForComponent);
		check(OriginalComponent);

		// @todo_pcg: Instead of polling the PrimingInfos every tick, we could probably cache them and only update when the graph params change.
		// Polling the PrimingInfos in this way is cheap for a single OC, but does not scale well if there are many OCs in the level.
		TArray<FPCGVirtualTexturePrimingInfo*> PrimingInfos;

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PollVirtualTexturePrimingInfos);

			const UPCGGraphInstance* GraphInstance = OriginalComponent->GetGraphInstance();
			const FInstancedPropertyBag* UserParametersStruct = GraphInstance ? GraphInstance->GetUserParametersStruct() : nullptr;
			const UPropertyBag* PropertyBag = UserParametersStruct ? UserParametersStruct->GetPropertyBagStruct() : nullptr;

			if (!PropertyBag)
			{
				continue;
			}

			TConstArrayView<FPropertyBagPropertyDesc> PropertyDescs = PropertyBag->GetPropertyDescs();

			for (const FPropertyBagPropertyDesc& PropertyDesc : PropertyDescs)
			{
				if (PropertyDesc.ValueType == EPropertyBagPropertyType::Struct && PropertyDesc.ValueTypeObject == TBaseStructure<FPCGVirtualTexturePrimingInfo>::Get())
				{
					TValueOrError<FPCGVirtualTexturePrimingInfo*, EPropertyBagResult> Property = UserParametersStruct->GetValueStruct<FPCGVirtualTexturePrimingInfo>(PropertyDesc);

					if (Property.HasValue() && Property.GetValue())
					{
						PrimingInfos.Add(Property.GetValue());
					}
				}
			}
		}

		const FBox OriginalComponentBounds = OriginalComponent->GetGridBounds();

		for (const FPCGVirtualTexturePrimingInfo* PrimingInfo : PrimingInfos)
		{
			if (!PrimingInfo || !PrimingInfo->VirtualTexture || PrimingInfo->WorldTexelSize < PCGRuntimeGenSchedulerConstants::MinWorldVirtualTextureTexelSize)
			{
				continue;
			}

			TArray<URuntimeVirtualTextureComponent*>* VirtualTextureComponents = VirtualTextureToComponents.Find(PrimingInfo->VirtualTexture);

			if (!VirtualTextureComponents)
			{
				continue;
			}

			for (URuntimeVirtualTextureComponent* VirtualTextureComponent : *VirtualTextureComponents)
			{
				check(VirtualTextureComponent);

				const double PrimingRadius = OriginalComponent->GetGenerationRadiusFromGrid(PrimingInfo->Grid) + PCGHiGenGrid::GridToGridSize(PrimingInfo->Grid);

				for (const IPCGGenSourceBase* GenSource : InGenSources)
				{
					const TOptional<FVector> GenSourcePositionOptional = GenSource->GetPosition();

					if (!GenSourcePositionOptional.IsSet())
					{
						continue;
					}

					FVector GenSourcePosition = GenSourcePositionOptional.GetValue();

					if (OriginalComponent->Use2DGrid())
					{
						GenSourcePosition.Z = OriginalComponentBounds.GetCenter().Z;
					}

					const FSphere PrimingBounds = FSphere(GenSourcePosition, PrimingRadius);

					if (OriginalComponentBounds.Intersect(FBox(PrimingBounds)))
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(RequestVirtualTexturePreload);

						const double BoundsMaxExtent = FMath::Max(VirtualTextureComponent->Bounds.BoxExtent.X, VirtualTextureComponent->Bounds.BoxExtent.Y);
						const int32 VirtualTextureSizeTexels = FMath::Max(1, PrimingInfo->VirtualTexture->GetSize());
						const int32 SizeTexelsLog2 = FMath::FloorLog2(VirtualTextureSizeTexels);
						const int32 RequestedNumTexels = FMath::Max(1, BoundsMaxExtent / PrimingInfo->WorldTexelSize);
						const int32 RequestedTexelsLog2 = FMath::FloorLog2(RequestedNumTexels);
						const int32 MipLevel = FMath::Max(0, SizeTexelsLog2 - RequestedTexelsLog2);

						VirtualTextureComponent->RequestPreload(PrimingBounds, MipLevel);

						if (PCGRuntimeGenSchedulerHelpers::CVarDebugDrawTexturePrimingBounds.GetValueOnAnyThread())
						{
							check(World);

							DrawDebugCylinder(
								World,
								FVector(GenSourcePosition.X, GenSourcePosition.Y, OriginalComponentBounds.Min.Z),
								FVector(GenSourcePosition.X, GenSourcePosition.Y, OriginalComponentBounds.Max.Z),
								/*Radius=*/PrimingBounds.W,
								/*Segments=*/8,
								/*Color=*/FColor::Red,
								/*bPersistentLines=*/false,
								/*LifeTime=*/0.02f);
						}
					}
				}
			}
		}
	}
}

void FPCGRuntimeGenScheduler::TickCVars(const APCGWorldActor* InPCGWorldActor)
{
	if (bActorFlushRequested && Subsystem && Subsystem->GetPCGWorldActor())
	{
		CleanupLocalComponents(Subsystem->GetPCGWorldActor());
		ResetPartitionActorPoolToSize(PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationBasePoolSize.GetValueOnAnyThread());
	}
	bActorFlushRequested = false;

	// If pooling has been disabled since last frame, we should destroy the pool.
	const bool bPoolingEnabled = PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread();

	if (bPoolingWasEnabledLastFrame && !bPoolingEnabled)
	{
		CleanupLocalComponents(InPCGWorldActor);
		ResetPartitionActorPoolToSize(/*NewPoolSize=*/0);
	}

	bPoolingWasEnabledLastFrame = bPoolingEnabled;

	// Handle when the base PA PoolSize is modified. Cleanup all local components and reset the pool with the correct number of PAs.
	if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread())
	{
		// Don't allow a pool size <= 0
		const uint32 BasePoolSize = FMath::Max(1, PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationBasePoolSize.GetValueOnAnyThread());

		if (BasePoolSizeLastFrame != BasePoolSize)
		{
			BasePoolSizeLastFrame = BasePoolSize;

			CleanupLocalComponents(InPCGWorldActor);
			ResetPartitionActorPoolToSize(BasePoolSize);
		}
	}
}

void FPCGRuntimeGenScheduler::OnOriginalComponentRegistered(UPCGComponent* InOriginalComponent)
{
	// Ensure we are non-local runtime managed component.
	if (!InOriginalComponent || !InOriginalComponent->IsManagedByRuntimeGenSystem() || Cast<APCGPartitionActor>(InOriginalComponent->GetOwner()))
	{
		return;
	}

	// When an original/non-partitioned component is registered, we need to dirty the state.
	bAnyRuntimeGenComponentsExistDirty = true;
}

void FPCGRuntimeGenScheduler::OnOriginalComponentUnregistered(UPCGComponent* InOriginalComponent)
{
	if (!InOriginalComponent || Cast<APCGPartitionActor>(InOriginalComponent->GetOwner()))
	{
		return;
	}

	check(ActorAndComponentMapping);

	// When an original/non-partitioned component is unregistered, we need to dirty the state.
	bAnyRuntimeGenComponentsExistDirty = true;

	// Gather all generated components which originated from this original component.
	TSet<FGridGenerationKey> KeysToCleanup;
	for (FGridGenerationKey GenerationKey : GeneratedComponents)
	{
		if (GenerationKey.GetOriginalComponent() == InOriginalComponent)
		{
			KeysToCleanup.Add(GenerationKey);
		}
	}

	TArray<FGridGenerationKey, TInlineAllocator<16>> InvalidKeys;

	for (const FGridGenerationKey& GenerationKey : KeysToCleanup)
	{
		if (!GenerationKey.IsValid())
		{
			InvalidKeys.Add(GenerationKey);
			continue;
		}

		const FPCGGridDescriptor GridDescriptor = GenerationKey.GetGridDescriptor();
		const bool bIsOriginalComponent = GridDescriptor.GetGridSize() == PCGHiGenGrid::UnboundedGridSize();

		// Get the generated component for this key (might be a local component).
		UPCGComponent* ComponentToCleanup = bIsOriginalComponent ? InOriginalComponent : ActorAndComponentMapping->GetLocalComponent(GridDescriptor, GenerationKey.GetGridCoords(), InOriginalComponent);

		// It is possible for a PartitionActor's LocalComponent to have been cleaned up by the APCGPartitionActor::EndPlay call depending on the order in which actors get called
		if (ComponentToCleanup)
		{
			CleanupComponent(GenerationKey, ComponentToCleanup);
		}
	}

	for (const FGridGenerationKey& InvalidKey : InvalidKeys)
	{
		GeneratedComponents.Remove(InvalidKey);
	}

	CleanupRemainingComponents(InOriginalComponent);
}

void FPCGRuntimeGenScheduler::CleanupRemainingComponents(UPCGComponent* InOriginalComponent)
{
	// Check for remaining PAs to cleanup
	// There are some cases when on Refresh of the original component that GeneratedComponents doesn't contain all PAs anymore.
	// If the PA is not far enough to be cleaned up but no longer is considered within the Gen Source Radius for Generation.
	if (InOriginalComponent && InOriginalComponent->IsManagedByRuntimeGenSystem())
	{
		TSet<TObjectPtr<APCGPartitionActor>> PartitionActors = ActorAndComponentMapping->GetPCGComponentPartitionActorMappings(InOriginalComponent);
		for (TObjectPtr<APCGPartitionActor> PartitionActor : PartitionActors)
		{
			if (UPCGComponent* ComponentToCleanup = PartitionActor->GetLocalComponent(InOriginalComponent))
			{
				CleanupLocalComponent(PartitionActor, ComponentToCleanup);
			}
		}
	}
}

void FPCGRuntimeGenScheduler::CleanupLocalComponents(const APCGWorldActor* InPCGWorldActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::CleanupLocalComponents);

	check(ActorAndComponentMapping && InPCGWorldActor);

	// Generated Entry Key, Local/Generated Component
	using PCGComponentToClean = TTuple<FGridGenerationKey, UPCGComponent*>;
	TArray<PCGComponentToClean, TInlineAllocator<16>> ComponentsToClean;

	TSet<UPCGComponent*> OriginalComponents;
	OriginalComponents.Reserve(16);

	TArray<FGridGenerationKey, TInlineAllocator<16>> InvalidKeys;

	// Find all generated local components.
	for (const FGridGenerationKey& GenerationKey : GeneratedComponents)
	{
		if (!GenerationKey.IsValid())
		{
			InvalidKeys.Add(GenerationKey);
			continue;
		}

		const FPCGGridDescriptor GridDescriptor = GenerationKey.GetGridDescriptor();
		const EPCGHiGenGrid Grid = GridDescriptor.GetHiGenGrid();
		const FIntVector& GridCoords = GenerationKey.GetGridCoords();
		UPCGComponent* OriginalComponent = GenerationKey.GetOriginalComponent();
		check(OriginalComponent);
		OriginalComponents.Add(OriginalComponent);

		// Only operate on LocalComponents.
		if (Grid != EPCGHiGenGrid::Unbounded)
		{
			UPCGComponent* LocalComponent = ActorAndComponentMapping->GetLocalComponent(GridDescriptor, GridCoords, OriginalComponent);
			ComponentsToClean.Add({ GenerationKey, LocalComponent });
		}
	}

	for (const FGridGenerationKey& InvalidKey : InvalidKeys)
	{
		GeneratedComponents.Remove(InvalidKey);
	}

	for (const PCGComponentToClean& ComponentToClean : ComponentsToClean)
	{
		CleanupComponent(ComponentToClean.Get<0>(), ComponentToClean.Get<1>());
	}

	for (UPCGComponent* OriginalComponent : OriginalComponents)
	{
		CleanupRemainingComponents(OriginalComponent);
	}
}

void FPCGRuntimeGenScheduler::OnLevelStreamingStateChanged(UWorld* InWorld, const ULevelStreaming* InLevelStreaming, ULevel* InLevelIfLoaded, ELevelStreamingState InPreviousState, ELevelStreamingState InNewState)
{
	if (World == InWorld && (InPreviousState == ELevelStreamingState::LoadedVisible || InNewState == ELevelStreamingState::LoadedVisible))
	{
		// todo_pcg: Fine-grained invalidation based on bounds overlap tests did not trivially work on first attempt, improve debug tools/vis and retry.
		CachedStreamingQueryResults.Empty(CachedStreamingQueryResults.Num());
	}
}

void FPCGRuntimeGenScheduler::CleanupLocalComponent(APCGPartitionActor* PartitionActor, UPCGComponent* LocalComponent)
{
	if (!PartitionActor)
	{
		return;
	}

	if (UPCGComponent* OriginalComponent = PartitionActor->GetOriginalComponent(LocalComponent))
	{
		if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
		{
			UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] CLEANUP: '%s'"), *PartitionActor->GetActorNameOrLabel());
		}

		// This performs a CleanupLocalImmediate for us, no need to clean up ourselves.
		PartitionActor->RemoveGraphInstance(OriginalComponent);

		// Remove component mapping.
		{
			FWriteScopeLock WriteLock(ActorAndComponentMapping->ComponentToPartitionActorsMapLock);
			TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ActorAndComponentMapping->ComponentToPartitionActorsMap.Find(OriginalComponent);

			if (PartitionActorsPtr)
			{
				PartitionActorsPtr->Remove(PartitionActor);

				if (PartitionActorsPtr->IsEmpty())
				{
					ActorAndComponentMapping->ComponentToPartitionActorsMap.Remove(OriginalComponent);
				}
			}
		}
	}

	// Cleanup the PA if it no longer has any components (return to pool or destroy).
	if (!PartitionActor->HasLocalPCGComponents())
	{
		PartitionActor->UnregisterPCG();

		if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread())
		{
			if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
			{
				UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] RETURNING PARTITION ACTOR TO POOL: '%s' (%d remaining out of %d)"), *PartitionActor->GetActorNameOrLabel(), PartitionActorPool.Num() + 1, PartitionActorPoolSize);
			}

#if WITH_EDITOR
			PartitionActor->Rename(nullptr, PartitionActor->GetOuter(), REN_NonTransactional | REN_DoNotDirty);
			PartitionActor->SetActorLabel(*PCGRuntimeGenSchedulerConstants::PooledPartitionActorName);
#endif
			PartitionActorPool.Push(PartitionActor);
		}
		else
		{
			if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
			{
				UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] DESTROY PARTITION ACTOR: '%s'"), *PartitionActor->GetActorNameOrLabel());
			}

#if WITH_EDITOR
			PartitionActor->Rename(nullptr, PartitionActor->GetOuter(), REN_NonTransactional | REN_DoNotDirty);
#endif
			World->DestroyActor(PartitionActor);
		}
	}
}

void FPCGRuntimeGenScheduler::CleanupComponent(const FGridGenerationKey& GenerationKey, UPCGComponent* GeneratedComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::CleanupComponent);

	check(ActorAndComponentMapping);

	const FPCGGridDescriptor GridDescriptor = GenerationKey.GetGridDescriptor();
	const EPCGHiGenGrid Grid = GridDescriptor.GetHiGenGrid();

	const FIntVector GridCoords = GenerationKey.GetGridCoords();

	APCGPartitionActor* PartitionActor = nullptr;

	if (!GeneratedComponent)
	{
		UE_LOG(LogPCG, Warning, TEXT("Runtime generated component could not be recovered on grid %d at (%d, %d, %d). It has been lost or destroyed."), GridDescriptor.GetGridSize(), GridCoords.X, GridCoords.Y, GridCoords.Z);

		// If the GeneratedComponent has been lost for some reason, get the PA directly from the ActorAndComponentMapping.
		PartitionActor = ActorAndComponentMapping->GetPartitionActor(GridDescriptor, GridCoords);
	}
	else // If the GeneratedComponent does exist, we can clean it up!.
	{
		PartitionActor = Cast<APCGPartitionActor>(GeneratedComponent->GetOwner());
		ensure(PartitionActor || Grid == EPCGHiGenGrid::Unbounded);

		if (Grid == EPCGHiGenGrid::Unbounded)
		{
			if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread() && GeneratedComponent->GetOwner())
			{
				UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] CLEANUP: '%s'"), *GeneratedComponent->GetOwner()->GetActorNameOrLabel());
			}

			GeneratedComponent->CleanupLocalImmediate(/*bRemoveComponents=*/true);
		}
	}

	CleanupLocalComponent(PartitionActor, GeneratedComponent);
	
	GeneratedComponents.Remove(GenerationKey);
}

void FPCGRuntimeGenScheduler::CleanupDelayedRefreshComponents()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::CleanupDelayedRefreshComponents);

	check(ActorAndComponentMapping);

	// Check that each refreshed local component is still intersecting its original component.
	// If it is not, it would be leaked instead of refreshed, so we should force a full cleanup.
	for (const FGridGenerationKey& GenerationKey : GeneratedComponentsToRemove)
	{
		if (!GenerationKey.IsValid())
		{
			continue;
		}

		const FPCGGridDescriptor GridDescriptor = GenerationKey.GetGridDescriptor();
		const EPCGHiGenGrid Grid = GridDescriptor.GetHiGenGrid();
		UPCGComponent* OriginalComponent = GenerationKey.GetOriginalComponent();
		const FIntVector& GridCoords = GenerationKey.GetGridCoords();

		// The unbounded grid level will always lie inside the original component, so we can skip it.
		if (Grid == EPCGHiGenGrid::Unbounded)
		{
			if (OriginalComponent && !OriginalComponent->bActivated)
			{
				CleanupComponent(GenerationKey, OriginalComponent);
			}

			continue;
		}

		UPCGComponent* LocalComponent = OriginalComponent ? ActorAndComponentMapping->GetLocalComponent(GridDescriptor, GridCoords, OriginalComponent) : nullptr;
		APCGPartitionActor* PartitionActor = LocalComponent ? Cast<APCGPartitionActor>(LocalComponent->GetOwner()) : nullptr;

		if (LocalComponent && PartitionActor)
		{
			const FBox OriginalBounds = OriginalComponent->GetGridBounds();
			const FBox LocalBounds = PartitionActor->GetFixedBounds();

			if (!OriginalBounds.Intersect(LocalBounds) || !OriginalComponent->bActivated)
			{
				CleanupComponent(GenerationKey, LocalComponent);
			}
		}
		else
		{
			// If the component or partition actor could not be recovered, just clean up.
			CleanupComponent(GenerationKey, /*GenerationKey=*/nullptr);
		}
	}

	// Remove any remaining generation keys that have been registered for deferred removal.
	GeneratedComponents = GeneratedComponents.Difference(GeneratedComponentsToRemove);
	GeneratedComponentsToRemove.Empty();
}

void FPCGRuntimeGenScheduler::RefreshComponent(UPCGComponent* InComponent, bool bRemovePartitionActors)
{
	if (!InComponent || !ensure(IsInGameThread()))
	{
		return;
	}

	APCGPartitionActor* PartitionActor = Cast<APCGPartitionActor>(InComponent->GetOwner());
	const bool bIsLocalComponent = PartitionActor != nullptr;
	UPCGComponent* OriginalComponent = bIsLocalComponent ? PartitionActor->GetOriginalComponent(InComponent) : InComponent;

	// If we are mid way through setting up an original component, early out from this refresh.
	if (!OriginalComponent || OriginalComponent == OriginalComponentBeingGenerated)
	{
		return;
	}

	const bool bLoggingEnabled = PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread();

	// Useful because we can run into generation order issues if components are left to continue generating.
	if (InComponent->IsGenerating())
	{
		InComponent->CancelGeneration();
	}

	if (!bRemovePartitionActors)
	{
		// Refresh path - mark component dirty and removed generated keys which will cause it to be scheduled for regeneration.

		// Register for deferred removal from generated components set, component will be regenerated later (and in grid order
		// so that e.g. unbounded is generated first).
		if (PartitionActor)
		{
			if (bLoggingEnabled)
			{
				UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] SHALLOW REFRESH LOCAL COMPONENT: '%s'"), *PartitionActor->GetActorNameOrLabel());
			}

			GeneratedComponentsToRemove.Emplace({ PartitionActor->GetPCGGridSize(), PartitionActor->GetGridCoord(), OriginalComponent, bIsLocalComponent ? InComponent : nullptr });
			InComponent->CleanupLocalImmediate(/*bRemoveComponents=*/false);
		}
		else
		{
			// Register original component for deferred removal.
			GeneratedComponentsToRemove.Emplace({ PCGHiGenGrid::UnboundedGridSize(), FIntVector(0), OriginalComponent });

			// Register local components for deferred removal if they have not already registered themselves.
			for (const FGridGenerationKey& Key : GeneratedComponents)
			{
				if (Key.GetOriginalComponent() == InComponent && !GeneratedComponentsToRemove.Contains(Key))
				{
					const FPCGGridDescriptor GridDescriptor = Key.GetGridDescriptor();
					
					// TODO - clean up local immediate will have a flag in the future to clean up the local components on its own, so this call to CleanupLocalImmediate will not be required
					UPCGComponent* LocalComponent = ActorAndComponentMapping->GetLocalComponent(
						GridDescriptor,
						Key.GetGridCoords(),
						OriginalComponent);

					if (bLoggingEnabled && LocalComponent && LocalComponent->GetOwner())
					{
						UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] SHALLOW REFRESH LOCAL COMPONENT: '%s'"), *LocalComponent->GetOwner()->GetActorNameOrLabel());
					}

					if (LocalComponent)
					{
						LocalComponent->CleanupLocalImmediate(/*bRemoveComponents=*/false);

						// We need to make sure that the next time this is generated that it matches the original
						LocalComponent->SetPropertiesFromOriginal(OriginalComponent);
					}

					GeneratedComponentsToRemove.Add(Key);
				}
			}

			if (bLoggingEnabled && OriginalComponent->GetOwner())
			{
				UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] SHALLOW REFRESH COMPONENT: '%s' PARTITIONED: %d"),
					*OriginalComponent->GetOwner()->GetActorNameOrLabel(),
					OriginalComponent->IsPartitioned() ? 1 : 0);
			}

			InComponent->CleanupLocalImmediate(/*bRemoveComponents=*/false);
		}
	}
	else
	{
		// Full cleanout path - cleanup existing components and return actors to the pool.

		auto RefreshLocalComponent = [this, OriginalComponent, bLoggingEnabled](UPCGComponent* LocalComponent)
		{
			check(LocalComponent);
			APCGPartitionActor* PartitionActor = CastChecked<APCGPartitionActor>(LocalComponent->GetOwner());

			// Find the specific generation key for this component, if it exists, cleanup and generate.
			FGridGenerationKey LocalComponentKey(PartitionActor->GetPCGGridSize(), PartitionActor->GetGridCoord(), OriginalComponent, LocalComponent);

			if (GeneratedComponents.Find(LocalComponentKey))
			{
				if (bLoggingEnabled)
				{
					UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] DEEP REFRESH LOCAL COMPONENT: '%s'"), *PartitionActor->GetActorNameOrLabel());
				}

				CleanupComponent(LocalComponentKey, LocalComponent);
			}
		};

		if (bIsLocalComponent)
		{
			RefreshLocalComponent(InComponent);
		}
		else
		{
			TArray<FGridGenerationKey> GenerationKeys;

			for (FGridGenerationKey GenerationKey : GeneratedComponents)
			{
				if (GenerationKey.GetOriginalComponent() == OriginalComponent)
				{
					GenerationKeys.Add(GenerationKey);
				}
			}

			// Gather all generated components which originated from this original component.
			for (FGridGenerationKey GenerationKey : GenerationKeys)
			{
				const FPCGGridDescriptor GridDescriptor = GenerationKey.GetGridDescriptor();
				const EPCGHiGenGrid Grid = GridDescriptor.GetHiGenGrid();

				// If the Grid is unbounded, we have a non-partitioned or unbounded component.
				if (Grid == EPCGHiGenGrid::Unbounded)
				{
					if (bLoggingEnabled && OriginalComponent->GetOwner())
					{
						UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] DEEP REFRESH COMPONENT: '%s' PARTITIONED: %d"),
							*OriginalComponent->GetOwner()->GetActorNameOrLabel(),
							OriginalComponent->IsPartitioned() ? 1 : 0);
					}

					CleanupComponent(GenerationKey, OriginalComponent);
				}
				// Otherwise we have a local component.
				else
				{
					const FIntVector GridCoords = GenerationKey.GetGridCoords();

					if (UPCGComponent* LocalComponent = ActorAndComponentMapping->GetLocalComponent(GridDescriptor, GridCoords, OriginalComponent))
					{
						RefreshLocalComponent(LocalComponent);
					}
					else
					{
						// If the local component could not be recovered, cleanup its entry to avoid leaking resources/locking the grid cell.
						CleanupComponent(GenerationKey, nullptr);
					}
				}
			}
		}
	}

	if (!bIsLocalComponent)
	{
		// When an original/non-partitioned component is refreshed, we need to dirty the state.
		bAnyRuntimeGenComponentsExistDirty = true;
	}
}

APCGPartitionActor* FPCGRuntimeGenScheduler::GetPartitionActorFromPool(const FPCGGridDescriptor& GridDescriptor, const FIntVector& GridCoords)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::GetPartitionActorFromPool);

	check(ActorAndComponentMapping);

	if (!World)
	{
		UE_LOG(LogPCG, Error, TEXT("[GetPartitionActorFromPool] World is null."));
		return nullptr;
	}

	// Attempt to find an existing RuntimeGen PA.
	if (APCGPartitionActor* ExistingActor = ActorAndComponentMapping->GetPartitionActor(GridDescriptor, GridCoords))
	{
		return ExistingActor;
	}

	// Double size of the pool if it is empty.
	if (PartitionActorPool.IsEmpty())
	{
		// If PartitionActorPoolSize is zero, then we should use the CVarBasePoolSize instead. Result must always at least be >= 1.
		const uint32 CurrentPoolSize = FMath::Max(1, PartitionActorPoolSize > 0 ? PartitionActorPoolSize : PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationBasePoolSize.GetValueOnAnyThread());

		if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
		{
			UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] INCREASING TRANSIENT PARTITION ACTOR POOL SIZE BY (%d)"), CurrentPoolSize);
		}

		// If pooling was enabled late, the editor world RuntimeGenScheduler will not have created the initial pool, so we should create it now.
		AddPartitionActorPoolCount(CurrentPoolSize);
	}

	check(!PartitionActorPool.IsEmpty());
	APCGPartitionActor* PartitionActor = PartitionActorPool.Pop();

#if WITH_EDITOR
	const FName ActorName = *APCGPartitionActor::GetPCGPartitionActorName(GridDescriptor, GridCoords);

	PartitionActor->Rename(*ActorName.ToString(), PartitionActor->GetOuter(), REN_NonTransactional | REN_DoNotDirty);
	PartitionActor->SetActorLabel(ActorName.ToString());
#endif

	const FVector CellCenter(FVector(GridCoords.X + 0.5, GridCoords.Y + 0.5, GridCoords.Z + 0.5) * GridDescriptor.GetGridSize());
	if (!PartitionActor->Teleport(CellCenter))
	{
		UE_LOG(LogPCG, Error, TEXT("[RUNTIMEGEN] Could not set the location of RuntimeGen partition actor '%s'."), *PartitionActor->GetActorNameOrLabel());
	}

#if WITH_EDITOR
	PartitionActor->SetLockLocation(true);
#endif

	// Empty GUID, RuntimeGen PAs don't need one.
	PartitionActor->PostCreation(GridDescriptor);

	return PartitionActor;
}

void FPCGRuntimeGenScheduler::AddPartitionActorPoolCount(int32 Count)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::AddPartitionActorPoolCount);

	PartitionActorPoolSize += Count;

	FActorSpawnParameters SpawnParams;
#if WITH_EDITOR
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	SpawnParams.Name = *PCGRuntimeGenSchedulerConstants::PooledPartitionActorName;

	// Always hide pooled actors from outliner. Note that outliner tree view updates can incur significant costs in Slate code.
	SpawnParams.bHideFromSceneOutliner = PCGRuntimeGenSchedulerHelpers::CVarHideActorsFromOutliner.GetValueOnAnyThread();
#endif

	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.ObjectFlags &= ~RF_Transactional;

	// Create RuntimeGen PA pool.
	for (int32 I = 0; I < Count; ++I)
	{
		// TODO: do these actors get networked automatically? do we want that or not?
		APCGPartitionActor* NewActor = World->SpawnActor<APCGPartitionActor>(SpawnParams);
		check(NewActor);
		NewActor->SetToRuntimeGenerated();
		PartitionActorPool.Add(NewActor);
#if WITH_EDITOR
		NewActor->SetActorLabel(PCGRuntimeGenSchedulerConstants::PooledPartitionActorName);
#endif
	}
}

void FPCGRuntimeGenScheduler::ResetPartitionActorPoolToSize(uint32 NewPoolSize)
{
	for (APCGPartitionActor* PartitionActor : PartitionActorPool)
	{
#if WITH_EDITOR
		PartitionActor->Rename(nullptr, PartitionActor->GetOuter(), REN_NonTransactional | REN_DoNotDirty);
#endif
		World->DestroyActor(PartitionActor);
	}

	PartitionActorPool.Empty();
	PartitionActorPoolSize = 0;
	AddPartitionActorPoolCount(NewPoolSize);
}

void FPCGRuntimeGenScheduler::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (GenSourceManager)
	{
		GenSourceManager->AddReferencedObjects(Collector);
	}

	// The level should be keeping the pooled PAs visible to GC. This is just a tentative fix for a crash in GetPartitionActorFromPool(), to understand if the crash is happening because of unreferenced GCed actors.
	Collector.AddReferencedObjects(PartitionActorPool);
}
