// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchInteractionSubsystem.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchInteractionIsland.h"
#include "PoseSearch/PoseSearchInteractionUtils.h"
#include "PoseSearch/PoseSearchRole.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "VisualLogger/VisualLogger.h"

namespace UE::PoseSearch
{
#if !NO_CVARS
static bool GVarPoseSearchInteractionEnabled = true;
static FAutoConsoleVariableRef CVarPoseSearchInteractionEnabled(TEXT("a.PoseSearchInteraction.Enabled"), GVarPoseSearchInteractionEnabled, TEXT("Enable/Disable Pose Search Interaction"));

static bool GVarPoseSearchInteractionCacheIslands = true;
static FAutoConsoleVariableRef CVarPoseSearchInteractionCacheIslands(TEXT("a.PoseSearchInteraction.CacheIslands"), GVarPoseSearchInteractionCacheIslands, TEXT("Cache Pose Search Interaction Islands for future reuse instead of destrying them"));

static bool GVarPoseSearchInteractionLoglandsTickDependencies = false;
static FAutoConsoleVariableRef CVarPoseSearchInteractionLoglandsTickDependencies(TEXT("a.PoseSearchInteraction.LoglandsTickDependencies"), GVarPoseSearchInteractionLoglandsTickDependencies, TEXT("Log islands tick dependencies"));
#endif // !NO_CVARS

struct FAnimContextInfo
{
	void Init(const FPoseSearchInteractionAnimContextAvailabilities& InAnimContextAvailabilities)
	{
		check(InAnimContextAvailabilities.AnimContext && !InAnimContextAvailabilities.Availabilities.IsEmpty());
		AnimContextAvailabilities = &InAnimContextAvailabilities;
		Location = GetContextLocation(InAnimContextAvailabilities.AnimContext);
	}

	// performs broad phase analysis checking if at least one of the Availabilities associated to AnimContext can interact with OtherAnimContextInfo.
	// This is a more relaxed analysis than the one performed in FRoledAnimContextInfo::CanInteractWith
	bool CanInteractWith(const FAnimContextInfo& OtherAnimContextInfo) const
	{
		check(this != &OtherAnimContextInfo);
		
		// @todo: enable this code if we ended up requiring preventing interactions between the same actor!
		//check(AnimContextAvailabilities && OtherAnimContextInfo.AnimContextAvailabilities);
		//const AActor* AnimContextActor = GetContextOwningActor(AnimContextAvailabilities->AnimContext);
		//const AActor* OtherAnimContextActor = GetContextOwningActor(OtherAnimContextInfo.AnimContextAvailabilities->AnimContext);
		//if (AnimContextActor == OtherAnimContextActor)
		//{
		//	return false;
		//}

		const FVector DeltaLocation = Location - OtherAnimContextInfo.Location;
		const float DistanceSquared = DeltaLocation.SquaredLength();
		const float MaxDistance = FMath::Min(GetAvailabilitiesMaxBroadPhaseRadius(), OtherAnimContextInfo.GetAvailabilitiesMaxBroadPhaseRadius());
		const float MaxDistanceSquared = MaxDistance * MaxDistance;
		return DistanceSquared <= MaxDistanceSquared;
	}

	float GetAvailabilitiesMaxBroadPhaseRadius() const
	{
		float AvailabilitiesMaxBroadPhaseRadius = 0.f;
		check(AnimContextAvailabilities);
		for (const FPoseSearchInteractionAvailabilityEx& Availability : AnimContextAvailabilities->Availabilities)
		{
			// @todo: optimize the AvailabilitiesMaxBroadPhaseRadius, since adding Availability.BroadPhaseRadiusIncrementOnInteraction is required ONLY if AnimContext is already part of an interaction
			AvailabilitiesMaxBroadPhaseRadius = FMath::Max(AvailabilitiesMaxBroadPhaseRadius, Availability.BroadPhaseRadius + Availability.BroadPhaseRadiusIncrementOnInteraction);
		}
		return AvailabilitiesMaxBroadPhaseRadius;
	}

	const FPoseSearchInteractionAnimContextAvailabilities* AnimContextAvailabilities = nullptr;

	// cached AnimContext location
	FVector Location = FVector::ZeroVector;
	TArray<const FAnimContextInfo*, TInlineAllocator<8, TMemStackAllocator<>>> NearbyAnimContextInfos;
};
struct FAnimContextInfos : TArray<FAnimContextInfo, TMemStackAllocator<>> {};

typedef TArray<const UPoseSearchDatabase*, TInlineAllocator<32, TMemStackAllocator<>>> FDatabasesPerTag;
struct FTagToDatabases : TMap<FName, FDatabasesPerTag, TInlineSetAllocator<8, TMemStackSetAllocator<>>> {};

} // namespace UE::PoseSearch

// FPoseSearchInteractionAvailabilityEx
///////////////////////////////////////////////
FString FPoseSearchInteractionAvailabilityEx::GetPoseHistoryName() const
{
	if (PoseHistory)
	{
		return "HistoryProvider";
	}
	return PoseHistoryName.ToString();
}

const UE::PoseSearch::IPoseHistory* FPoseSearchInteractionAvailabilityEx::GetPoseHistory(const UObject* AnimContext) const
{
	if (PoseHistory)
	{
		return PoseHistory;
	}
		
	if (const UAnimInstance* AnimInstance = Cast<UAnimInstance>(AnimContext))
	{
		if (const FAnimNode_PoseSearchHistoryCollector_Base* PoseSearchHistoryCollector = UPoseSearchLibrary::FindPoseHistoryNode(PoseHistoryName, AnimInstance))
		{
			return &PoseSearchHistoryCollector->GetPoseHistory();
		}
	}

	unimplemented();
	return nullptr;
}

// UPoseSearchInteractionSubsystem
///////////////////////////////////////////////
UE::PoseSearch::FInteractionIsland& UPoseSearchInteractionSubsystem::CreateIsland()
{
	return *Islands.Add_GetRef(new UE::PoseSearch::FInteractionIsland(ToRawPtr(GetWorld()->PersistentLevel), this));
}

void UPoseSearchInteractionSubsystem::DestroyIsland(int32 Index)
{
	delete Islands[Index];
	Islands.RemoveAt(Index);
}

UE::PoseSearch::FInteractionIsland& UPoseSearchInteractionSubsystem::GetAvailableIsland()
{
	using namespace UE::PoseSearch;

	for (FInteractionIsland* Island : Islands)
	{
		if (!Island->IsInitialized())
		{
			return *Island;
		}
	}

	return CreateIsland();
}

void UPoseSearchInteractionSubsystem::DestroyAllIslands()
{
	for (int32 IslandIndex = Islands.Num() - 1; IslandIndex >= 0; --IslandIndex)
	{
		DestroyIsland(IslandIndex);
	}
}

void UPoseSearchInteractionSubsystem::RegenerateAllIslands(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UPoseSearchInteractionSubsystem_RegenerateAllIslands);

	using namespace UE::PoseSearch;

	check(IsInGameThread());

	// FScopeLock Lock(&AnimContextsAvailabilitiesMutex); is not necessary since UPoseSearchInteractionSubsystem gets ticked outside the parallel animation jobs

	// generating all the possible interaction tuples of AnimContext(s) with roles and pose histories (defined in FInteractionSearchContext)
	FInteractionSearchContexts SearchContexts;
	GenerateSearchContexts(DeltaSeconds, SearchContexts);

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	// drawing the current frame islands to be consistent with the search, before regenerating the islands with the newly published availabilities
	DebugDrawIslands();
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	
#if ENABLE_ANIM_DEBUG
	DebugLogTickDependencies();
#endif // ENABLE_ANIM_DEBUG

#if !NO_CVARS
	if (!GVarPoseSearchInteractionCacheIslands)
	{
		// not caching the islands. Destroy them all!
		DestroyAllIslands();
	}
	else
#endif // !NO_CVARS
	{
		for (FInteractionIsland* Island : Islands)
		{
			CheckInteractionThreadSafety(Island);
			Island->Uninitialize(true);
		}
	}

	struct FInteractionSearchContextGroup
	{
		bool Contains(const FInteractionSearchContext& SearchContext) const
		{
			for (int32 AnimContextIndex = 0; AnimContextIndex < SearchContext.Num(); ++AnimContextIndex)
			{
				if (AnimContextToTickPriority.Find(SearchContext.GetAnimContext(AnimContextIndex)))
				{
					return true;
				}
			}
			return false;
		}

		void Add(const FInteractionSearchContext& SearchContext, int32 SearchContextIndex)
		{
			for (int32 AnimContextIndex = 0; AnimContextIndex < SearchContext.Num(); ++AnimContextIndex)
			{
				if (const UObject* AnimContext = SearchContext.GetAnimContext(AnimContextIndex))
				{
					if (int32* TickPriority = AnimContextToTickPriority.Find(AnimContext))
					{
						*TickPriority = FMath::Max(*TickPriority, SearchContext.TickPriorities[AnimContextIndex]);
					}
					else
					{
						AnimContextToTickPriority.Add(AnimContext) = SearchContext.TickPriorities[AnimContextIndex];
					}
				}
			}

			SearchContextsIndices.Add(SearchContextIndex);
		}

		void Merge(const FInteractionSearchContextGroup& SearchContextGroup)
		{
			for (const FAnimContextToTickPriorityPair& AnimContextToTickPriorityPair : SearchContextGroup.AnimContextToTickPriority)
			{
				if (int32* TickPriority = AnimContextToTickPriority.Find(AnimContextToTickPriorityPair.Key))
				{
					*TickPriority = FMath::Max(*TickPriority, AnimContextToTickPriorityPair.Value);
				}
				else
				{
					AnimContextToTickPriority.Add(AnimContextToTickPriorityPair.Key) = AnimContextToTickPriorityPair.Value;
				}
			}

			for (int32 SearchContextsIndex : SearchContextGroup.SearchContextsIndices)
			{
				SearchContextsIndices.Add(SearchContextsIndex);
			}
		}

		// all the AnimContexts in this group with their TickPriority
		typedef TPair<const UObject*, int32> FAnimContextToTickPriorityPair;
		typedef TMap<const UObject*, int32, TInlineSetAllocator<16, TMemStackSetAllocator<>>> FAnimContextToTickPriority;
		FAnimContextToTickPriority AnimContextToTickPriority;

		// indexes to the searchcontexts assigned to this group
		TArray<int32, TInlineAllocator<16, TMemStackAllocator<>>> SearchContextsIndices;
	};

	// grouping SearchContexts AnimContext(s) in FInteractionSearchContextGroup(s). We'll create as many interaction islands as many groups
	TArray<FInteractionSearchContextGroup, TInlineAllocator<PreallocatedSearchesNum, TMemStackAllocator<>>> SearchContextGroups;
	for (int32 SearchContextIndex = 0; SearchContextIndex < SearchContexts.Num(); ++SearchContextIndex)
	{
		// evaluating where to place SearchContext..
		const FInteractionSearchContext& SearchContext = SearchContexts[SearchContextIndex];

		int32 MainSearchContextGroupIndex = INDEX_NONE;
		for (int32 SearchContextGroupIndex = 0; SearchContextGroupIndex < SearchContextGroups.Num();)
		{
			// ..if SearchContextGroups[SearchContextGroupIndex] contains ANY of the AnimContexts from SearchContext..
			if (SearchContextGroups[SearchContextGroupIndex].Contains(SearchContext))
			{
				if (MainSearchContextGroupIndex == INDEX_NONE)
				{
					// ..we add SearchContext to SearchContextGroups[SearchContextGroupIndex] 
					// and set MainSearchContextGroupIndex to SearchContextGroupIndex to know what is the group containing SearchContext, so..
					MainSearchContextGroupIndex = SearchContextGroupIndex;
					SearchContextGroups[MainSearchContextGroupIndex].Add(SearchContext, SearchContextIndex);
					++SearchContextGroupIndex;
				}
				else
				{
					// ..in case SearchContext has already being inserted in MainSearchContextGroupIndex group 
					// we merge the newly found SearchContextGroups[SearchContextGroupIndex] to SearchContextGroups[MainSearchContextGroupIndex]
					// (containing another of the the AnimContexts)
					SearchContextGroups[MainSearchContextGroupIndex].Merge(SearchContextGroups[SearchContextGroupIndex]);
					SearchContextGroups.RemoveAt(SearchContextGroupIndex);
				}
			}
			else
			{
				++SearchContextGroupIndex;
			}
		}
		if (MainSearchContextGroupIndex == INDEX_NONE)
		{
			SearchContextGroups.AddDefaulted_GetRef().Add(SearchContext, SearchContextIndex);
		}
	}

	TArray<FInteractionSearchContextGroup::FAnimContextToTickPriorityPair, TInlineAllocator<16, TMemStackAllocator<>>> SortedByTickPriorityAnimContexts;
	for (FInteractionSearchContextGroup& SearchContextGroup : SearchContextGroups)
	{
		// @todo: search for the most suitable island to reuse to avoid having to Uninitialize/RemoveTickDependencies and InjectToActor right away
		FInteractionIsland& Island = GetAvailableIsland();
		CheckInteractionThreadSafety(&Island);

		// initializing the island with its assigned SearchContexts
		bool bAreTickDependenciesRequired = false;
		check(Island.GetSearchContexts().IsEmpty());
		for (int32 SearchContextsIndex : SearchContextGroup.SearchContextsIndices)
		{
			const FInteractionSearchContext& SearchContext = SearchContexts[SearchContextsIndex];
			// if there're at least two AnimContext(s) potentially interacting with each other 
			// (where the search involves 2+ characters) tick dependencies are required to be thread safe
			bAreTickDependenciesRequired |= SearchContext.Num() > 1;
			Island.AddSearchContext(SearchContext);
		}

		// sorting SearchContextGroup.AnimContextToTickPriority by TickPriority
		// (using SortedByTickPriorityAnimContexts since SearchContextGroup.AnimContextToTickPriority it's a TMap)
		SortedByTickPriorityAnimContexts.Reset();
		SortedByTickPriorityAnimContexts.Reserve(SearchContextGroup.AnimContextToTickPriority.Num());
		for (const FInteractionSearchContextGroup::FAnimContextToTickPriorityPair& AnimContextToTickPriorityPair : SearchContextGroup.AnimContextToTickPriority)
		{
			SortedByTickPriorityAnimContexts.Add(AnimContextToTickPriorityPair);
		}
		SortedByTickPriorityAnimContexts.Sort(
			[](const FInteractionSearchContextGroup::FAnimContextToTickPriorityPair& A, const FInteractionSearchContextGroup::FAnimContextToTickPriorityPair& B)
			{
				return B.Value < A.Value;
			});

		// injecting tick dependencies between island AnimContext following their TickPriorities, 
		// so the AnimContext with the highest TickPriority will be elected as "Main Actor", being evaluated, 
		// and performing all the island searches, before any other Actor in the same island\
		// (that'll end up using the cached search results in a multithread manner)
		for (const FInteractionSearchContextGroup::FAnimContextToTickPriorityPair& SortedByTickPriorityAnimContext : SortedByTickPriorityAnimContexts)
		{
			Island.InjectToActor(SortedByTickPriorityAnimContext.Key, bAreTickDependenciesRequired);
		}
	}
}

#if DO_CHECK
bool UPoseSearchInteractionSubsystem::ValidateAllIslands() const
{
	using namespace UE::PoseSearch;

	TSet<TWeakObjectPtr<UActorComponent>> TickActorComponents;

	typedef TSet<const UObject*> FIslandAnimContexts;
	TArray<FIslandAnimContexts> IslandsAnimContexts;

	const int32 NumIslands = Islands.Num();
	IslandsAnimContexts.Reserve(NumIslands);

	bool bAlreadyInSet = false;
	for (const FInteractionIsland* Island : Islands)
	{
		for (const TWeakObjectPtr<UActorComponent>& TickActorComponent : Island->GetTickActorComponents())
		{
			TickActorComponents.Add(TickActorComponent, &bAlreadyInSet);
			if (bAlreadyInSet)
			{
				return false;
			}
		}

		FIslandAnimContexts& IslandAnimContexts = IslandsAnimContexts.AddDefaulted_GetRef();
		for (const FInteractionSearchContext& SearchContext : Island->GetSearchContexts())
		{
			for (int32 AnimContextIndex = 0; AnimContextIndex < SearchContext.Num(); ++AnimContextIndex)
			{
				if (const UObject* AnimContext = SearchContext.GetAnimContext(AnimContextIndex))
				{
					IslandAnimContexts.Add(AnimContext);
				}
			}
		}
	}

	for (int32 IslandIndex = 0; IslandIndex < NumIslands; ++IslandIndex)
	{
		for (const UObject* AnimContext : IslandsAnimContexts[IslandIndex])
		{
			for (int32 OtherIslandIndex = 0; OtherIslandIndex < NumIslands; ++OtherIslandIndex)
			{
				if (IslandIndex != OtherIslandIndex)
				{
					if (IslandsAnimContexts[OtherIslandIndex].Find(AnimContext))
					{
						// AnimContext is shared between multiple islands. it'd cause multi threadind issues!
						return false;
					}
				}
			}
		}				
	}

	return true;
}
#endif // DO_CHECK

void UPoseSearchInteractionSubsystem::PopulateContinuingProperties(float DeltaSeconds, TArrayView<UE::PoseSearch::FInteractionSearchContext> SearchContexts) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UPoseSearchInteractionSubsystem_PopulateContinuingProperties);

	using namespace UE::PoseSearch;

	check(IsInGameThread());

	for (FInteractionSearchContext& SearchContext : SearchContexts)
	{
		// searching this SearchContext in all the islands to initialize its continuing pose
		for (const FInteractionIsland* Island : Islands)
		{
			if (const FSearchResult* SearchResult = Island->FindSearchResult(SearchContext))
			{
				// is still valid...
				if (SearchResult->IsValid())
				{
					if (const UE::PoseSearch::FSearchIndexAsset* SearchIndexAsset = SearchResult->GetSearchIndexAsset())
					{
						if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = SearchResult->Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(*SearchIndexAsset))
						{
							check(SearchIndexAsset->GetToRealTimeFactor() > UE_KINDA_SMALL_NUMBER);
							// in case DatabaseAsset->GetAnimationAsset() is a blendspace, SearchResult->AssetTime is a normalized time in the interval [0,1] 
							// so we need to convert the delta time in seconds to the asset normalized time before integrating SearchResult->AssetTime
							const float NormalizedDeltaTime = DeltaSeconds / SearchIndexAsset->GetToRealTimeFactor();
							SearchContext.PlayingAssetAccumulatedTime = SearchResult->AssetTime + NormalizedDeltaTime;
							SearchContext.PlayingAsset = DatabaseAsset->GetAnimationAsset();
							SearchContext.bIsPlayingAssetMirrored = SearchIndexAsset->IsMirrored();
							SearchContext.PlayingAssetBlendParameters = SearchIndexAsset->GetBlendParameters();
							// @todo: populate SearchContext.InterruptMode
						}
					}
				}
				break;
			}
		}
	}
}

UE::PoseSearch::FInteractionIsland* UPoseSearchInteractionSubsystem::FindIsland(const UObject* AnimContext, bool bCompareOwningActors)
{
	using namespace UE::PoseSearch;

	if (AnimContext)
	{
		if (bCompareOwningActors)
		{
			const AActor* Actor = GetContextOwningActor(AnimContext);

			for (FInteractionIsland* Island : Islands)
			{
				for (const TWeakObjectPtr<const UObject>& IslandAnimContext : Island->GetIslandAnimContexts())
				{ 
					if (GetContextOwningActor(IslandAnimContext.Get()) == Actor)
					{
						return Island;
					}
				}
			}
		}
		else
		{
			for (FInteractionIsland* Island : Islands)
			{
				if (Island->GetIslandAnimContexts().Contains(AnimContext))
				{
					return Island;
				}
			}
		}
	}
	return nullptr;
}

UPoseSearchInteractionSubsystem* UPoseSearchInteractionSubsystem::GetSubsystem_AnyThread(const UObject* AnimContext)
{
	if (AnimContext)
	{
		if (UWorld* World = AnimContext->GetWorld())
		{
			// We expect the subsystem to be already created from the GameThread.
			// We don't create the subsystem from any thread
			if (World->HasSubsystem<UPoseSearchInteractionSubsystem>())
			{
				return World->GetSubsystem<UPoseSearchInteractionSubsystem>();
			}
		}
	}
	return nullptr;
}

void UPoseSearchInteractionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	check(IsInGameThread());
	Super::Initialize(Collection);
}

void UPoseSearchInteractionSubsystem::Deinitialize()
{
	UpdateValidInteractionSearches();

	DestroyAllIslands();

	Super::Deinitialize();
}

void UPoseSearchInteractionSubsystem::AddAvailabilities(const TArrayView<const FPoseSearchInteractionAvailability> Availabilities, const UObject* AnimContext, FName PoseHistoryName, const UE::PoseSearch::IPoseHistory* PoseHistory)
{
	using namespace UE::PoseSearch;

	check(AnimContext && AnimContext->GetWorld() && AnimContext->GetWorld() == GetWorld());

	// collecting valid availabilities indexes here to minimize the time spent on the lock and avoid locking at all 
	// if there're actually no valid availabilities (as well as avoiding polluting AnimContextsAvailabilities with an empty entry).
	// This is unfortunately a common setup, and that's the main reason we optimize against it
	TArray<int32, TInlineAllocator<64>> ValidAvailabilityIndexes;
	for (int32 AvailabilityIndex = 0; AvailabilityIndex < Availabilities.Num(); ++AvailabilityIndex)
	{
		const FPoseSearchInteractionAvailability& Availability = Availabilities[AvailabilityIndex];
		if ((Availability.Database && Availability.Database->Schema) || Availability.IsTagValid())
		{
			ValidAvailabilityIndexes.Add(AvailabilityIndex);
		}
	}

	if (!ValidAvailabilityIndexes.IsEmpty())
	{
		FScopeLock Lock(&AnimContextsAvailabilitiesMutex);

		const int32 UpperBoundIndex = Algo::UpperBoundBy(AnimContextsAvailabilities, AnimContext,
			[](const FPoseSearchInteractionAnimContextAvailabilities& AnimContextAvailabilities)
			{
				return AnimContextAvailabilities.AnimContext.Get();
			},
			[](const UObject* AnimContextA, const UObject* AnimContextB)
			{
				return AnimContextA < AnimContextB;
			});

		int32 AnimContextAvailabilityIndex;
		if (UpperBoundIndex > 0 && AnimContextsAvailabilities[UpperBoundIndex - 1].AnimContext == AnimContext)
		{
			// we found the AnimContextsAvailabilities for AnimContext
			AnimContextAvailabilityIndex = UpperBoundIndex - 1;
		}
		else
		{
			// we create a new AnimContextsAvailabilities for AnimContext, preserving AnimContextsAvailabilities sorting
			AnimContextsAvailabilities.InsertDefaulted_GetRef(UpperBoundIndex).AnimContext = AnimContext;
			AnimContextAvailabilityIndex = UpperBoundIndex;
		}

		for (const int32 AvailabilityIndex : ValidAvailabilityIndexes)
		{
			// Avoiding adding trivial duplicates. FPoseSearchInteractionAvailabilityEx could not be fully specified to understand if it's an actual duplicate in case the pose
			// history is passed by name or the Availability.Database is null and supposed to be resolved using other availabilities Database(s) with the same Availability.Tag.
			// The duplicated availabilities are excluded when creating the combinations of possible interactions during FAnimContextInfoVisitor when 
			// FRoledAnimContextInfos.AddRoledAnimContextInfos calls FRoledAnimContextInfos.AddUnique
			FPoseSearchInteractionAvailabilityEx AvailabilityEx;
			AvailabilityEx.Init(Availabilities[AvailabilityIndex], PoseHistoryName, PoseHistory);
			AnimContextsAvailabilities[AnimContextAvailabilityIndex].Availabilities.AddUnique(AvailabilityEx);
		}
	}
}

void UPoseSearchInteractionSubsystem::GenerateAnimContextInfosAndTagToDatabases(UE::PoseSearch::FAnimContextInfos& AnimContextInfos, UE::PoseSearch::FTagToDatabases& TagToDatabases) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UPoseSearchInteractionSubsystem_GenerateAnimContextInfos);

	using namespace UE::PoseSearch;

	const UWorld* SubsystemWorld = GetWorld();
	check(SubsystemWorld);
	check(AnimContextInfos.IsEmpty() && TagToDatabases.IsEmpty());

	check(Algo::IsSorted(AnimContextsAvailabilities, [](const FPoseSearchInteractionAnimContextAvailabilities& A, const FPoseSearchInteractionAnimContextAvailabilities& B)
		{
			return A.AnimContext < B.AnimContext;
		}));

	for (const FPoseSearchInteractionAnimContextAvailabilities& AnimContextAvailabilities : AnimContextsAvailabilities)
	{
		check(AnimContextAvailabilities.AnimContext && AnimContextAvailabilities.AnimContext->GetWorld() && AnimContextAvailabilities.AnimContext->GetWorld() == GetWorld());
		check(!AnimContextAvailabilities.Availabilities.IsEmpty());
		
		// adding AnimContext to AnimContextsWithAvailabilities only if at least one availability has a valid database or has a valid tag
		for (const FPoseSearchInteractionAvailabilityEx& InteractionAvailabilityEx : AnimContextAvailabilities.Availabilities)
		{
			bool bAnyValidAvailability = false;
			if (const UPoseSearchDatabase* Database = InteractionAvailabilityEx.Database.Get())
			{
				check(Database->Schema);
				if (InteractionAvailabilityEx.IsTagValid())
				{
					TagToDatabases.FindOrAdd(InteractionAvailabilityEx.Tag).AddUnique(InteractionAvailabilityEx.Database);
				}
			}
		}
	}

	const int32 NumAnimContextInfos = AnimContextsAvailabilities.Num();
	AnimContextInfos.SetNum(NumAnimContextInfos);
	for (int32 AnimContextInfoIndex = 0; AnimContextInfoIndex < NumAnimContextInfos; ++AnimContextInfoIndex)
	{
		AnimContextInfos[AnimContextInfoIndex].Init(AnimContextsAvailabilities[AnimContextInfoIndex]);
	}

	// solving the broad phase using the AnimContextInfos
	for (int32 AnimContextInfoIndexA = 0; AnimContextInfoIndexA < NumAnimContextInfos; ++AnimContextInfoIndexA)
	{
		for (int32 AnimContextInfoIndexB = AnimContextInfoIndexA + 1; AnimContextInfoIndexB < NumAnimContextInfos; ++AnimContextInfoIndexB)
		{
			if (AnimContextInfos[AnimContextInfoIndexA].CanInteractWith(AnimContextInfos[AnimContextInfoIndexB]))
			{
				// the AnimContext of AnimContextInfos[AnimContextInfoIndexA] can potentially interact with the one from AnimContextInfos[AnimContextInfoIndexB]:
				// linking AnimContextInfos[AnimContextInfoIndexA] to AnimContextInfos[AnimContextInfoIndexA] and vice versa to keep track of this when evaluating the broad phase.
				// Since AnimContextInfos does't reallocate anymore, it's safe to store pointers to internal elements of the array!
				AnimContextInfos[AnimContextInfoIndexA].NearbyAnimContextInfos.Emplace(&AnimContextInfos[AnimContextInfoIndexB]);
				AnimContextInfos[AnimContextInfoIndexB].NearbyAnimContextInfos.Emplace(&AnimContextInfos[AnimContextInfoIndexA]);
			}
		}
	}
}

void UPoseSearchInteractionSubsystem::GenerateSearchContexts(float DeltaSeconds, UE::PoseSearch::FInteractionSearchContexts& SearchContexts) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UPoseSearchInteractionSubsystem_GenerateSearchContexts);

	using namespace UE::PoseSearch;

	check(SearchContexts.IsEmpty());

	struct FRoledAnimContextInfo
	{
		FRoledAnimContextInfo(const FPoseSearchInteractionAvailabilityEx& InAvailability, const FAnimContextInfo& InAnimContextInfo, const FRole InRole, const IPoseHistory* InPoseHistory, const UPoseSearchDatabase& InDatabase)
			: Availability(&InAvailability)
			, AnimContextInfo(&InAnimContextInfo)
			, Role(InRole)
			, PoseHistory(InPoseHistory)
			, Database(&InDatabase)
		{
		}

		// perform narrow phase analysis checking if the AnimContextInfo with the specialized properties from Availability, Role, PoseHistory, Database,
		// can interact with OtherRoledAnimContextInfo. This is a less relaxed analysis than the one performed in FAnimContextInfo::CanInteractWith
		bool CanInteractWith(const FRoledAnimContextInfo& OtherRoledAnimContextInfo, bool bWasSearchContextInteracting) const
		{
			check(this != &OtherRoledAnimContextInfo);

			const FVector DeltaLocation = AnimContextInfo->Location - OtherRoledAnimContextInfo.AnimContextInfo->Location;
			const float DistanceSquared = DeltaLocation.SquaredLength();

			float MaxDistance;
			if (bWasSearchContextInteracting)
			{
				MaxDistance = FMath::Min(Availability->BroadPhaseRadius + Availability->BroadPhaseRadiusIncrementOnInteraction, OtherRoledAnimContextInfo.Availability->BroadPhaseRadius + OtherRoledAnimContextInfo.Availability->BroadPhaseRadiusIncrementOnInteraction);
			}
			else
			{
				MaxDistance = FMath::Min(Availability->BroadPhaseRadius, OtherRoledAnimContextInfo.Availability->BroadPhaseRadius);
			}
			
			const float MaxDistanceSquared = MaxDistance * MaxDistance;
			return DistanceSquared <= MaxDistanceSquared;
		}

		bool operator==(const FRoledAnimContextInfo& Other) const = default;

		// Availability that spawned this FRoledAnimContextInfo
		const FPoseSearchInteractionAvailabilityEx* Availability = nullptr;
		// AnimContextInfo containing all the information regarding the AnimContext that spawned this FRoledAnimContextInfo,
		// including all the availabilities associated to the AnimContext as well as all the other AnimContext(s) it can potentially interact with
		const FAnimContextInfo* AnimContextInfo = nullptr;
		const FRole Role = DefaultRole;
		const IPoseHistory* PoseHistory = nullptr;
		const UPoseSearchDatabase* Database = nullptr;
	};
	
	struct FRoledAnimContextInfos : TArray<FRoledAnimContextInfo, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>>
	{
		void AddRoledAnimContextInfos(const FPoseSearchInteractionAvailabilityEx& Availability, const FAnimContextInfo& AnimContextInfo, const IPoseHistory* PoseHistory, const UPoseSearchDatabase& Database)
		{
			const UPoseSearchSchema* Schema = Database.Schema;
			check(Schema);
			if (Availability.RolesFilter.IsEmpty())
			{
				// adding ALL the possible roles from the database:
				for (const FPoseSearchRoledSkeleton& RoledSkeleton : Schema->GetRoledSkeletons())
				{
					AddUnique(FRoledAnimContextInfo(Availability, AnimContextInfo, RoledSkeleton.Role, PoseHistory, Database));
				}
			}
			else
			{
				for (const FRole& Role : Availability.RolesFilter)
				{
					if (Schema->GetRoledSkeleton(Role))
					{
						AddUnique(FRoledAnimContextInfo(Availability, AnimContextInfo, Role, PoseHistory, Database));
					}
					else
					{
						UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchInteractionSubsystem::GenerateSearchContexts unsupported Role %s for Database %s"), *Role.ToString(), *Database.GetName());
					}
				}
			}
		}
	};

	// visits the FAnimContextInfos recursively to identify groups of nearby AnimContextInfo(s), relying on FAnimContextInfo::NearbyAnimContextInfos information.
	// it calls OnNewAnimAnimContextInfoFound on every new FAnimContextInfo found/visited, and OnDoneGroupingAnimContexts once reaches the end of the current group AnimContext(s).
	// it's then restart calling OnNewAnimAnimContextInfoFound in case there are still unvisited FAnimContextInfo(s), untill it visited ALL the FAnimContextInfo(s) in the input FAnimContextInfos
	struct FAnimContextInfoVisitor
	{
		FAnimContextInfoVisitor(
			const FAnimContextInfos& AnimContextInfos,
			TFunctionRef<void(const FAnimContextInfo&)> OnNewAnimAnimContextInfoFound,
			TFunctionRef<void()> OnDoneGroupingAnimContexts)
		{
			for (const FAnimContextInfo& AnimContextInfo : AnimContextInfos)
			{
				if (AnimContextInfo.NearbyAnimContextInfos.IsEmpty())
				{
					check(!VisitedAnimContextInfos.Find(&AnimContextInfo));
					// no need to add this context to the VisitedAnimContextInfos since it's isolated!
					OnNewAnimAnimContextInfoFound(AnimContextInfo);
					OnDoneGroupingAnimContexts();
				}
				else if(!VisitedAnimContextInfos.Find(&AnimContextInfo))
				{
					// starting the evaluation of a new set of grouped AnimContext(s)

					// processing the AnimContextsAvailabilities of the current AnimContextArray to fill up a map of Databases pointing to an array of all the AnimContexts with related roles
					VisitRecursively(AnimContextInfo, OnNewAnimAnimContextInfoFound);

					OnDoneGroupingAnimContexts();
				}
			}
		}

	private:
		void VisitRecursively(const FAnimContextInfo& AnimContextInfoToVisit, TFunctionRef<void(const FAnimContextInfo&)> OnNewAnimAnimContextInfoFound)
		{
			check(!AnimContextInfoToVisit.NearbyAnimContextInfos.IsEmpty());

			bool bIsAlreadyInSet;
			VisitedAnimContextInfos.FindOrAdd(&AnimContextInfoToVisit, &bIsAlreadyInSet);

			if (!bIsAlreadyInSet)
			{
				OnNewAnimAnimContextInfoFound(AnimContextInfoToVisit);

				for (const FAnimContextInfo* NearbyAnimContextInfo : AnimContextInfoToVisit.NearbyAnimContextInfos)
				{
					check(NearbyAnimContextInfo);
					VisitRecursively(*NearbyAnimContextInfo, OnNewAnimAnimContextInfoFound);
				}
			}
		}

		TSet<const FAnimContextInfo*, DefaultKeyFuncs<const FAnimContextInfo*>, TInlineSetAllocator<32, TMemStackSetAllocator<>>> VisitedAnimContextInfos;
	};

	// caching AnimContext(s) locations, max broad phase radiuses (squared) and collect relations of possible interactions between AnimContext(s)
	// (stored in FAnimContextInfo::NearbyAnimContextInfos::Index) as fast broad phase evaluation refined later on during FInteractionSearchContexts generation
	// and generating a mapping between availabilities Tag(s) to availabilities published databases
	FAnimContextInfos AnimContextInfos;
	FTagToDatabases TagToDatabases;
	GenerateAnimContextInfosAndTagToDatabases(AnimContextInfos, TagToDatabases);

	const TConstArrayView<FValidInteractionSearch> PreviousValidInteractionSearches = ValidInteractionSearches;

	TMap<const UPoseSearchDatabase*, FRoledAnimContextInfos, TInlineSetAllocator<PreallocatedSearchesNum, TMemStackSetAllocator<>>> DatabaseToRoledAnimContextInfos;
	// visiting ALL the AnimContexts in AnimContextInfos and relying on the cached information to refine potential interactions
	FAnimContextInfoVisitor AnimContextInfoVisitor(AnimContextInfos,
		// OnNewAnimContextFound: called when the FAnimContextInfoVisitor finds a new AnimContext that can be grouped in the current group of possibly interacting AnimContext(s)
		[&TagToDatabases, &DatabaseToRoledAnimContextInfos](const FAnimContextInfo& AnimContextInfo)
		{
			// analyzing all the Availability(s) associated with this AnimContext and eventually generate the associated FRoledAnimContextInfos,
			// inserted in a per database sorted data structure (DatabaseToRoledAnimContextInfos)
			check(AnimContextInfo.AnimContextAvailabilities);
			for (const FPoseSearchInteractionAvailabilityEx& Availability : AnimContextInfo.AnimContextAvailabilities->Availabilities)
			{
				if (const IPoseHistory* PoseHistory = Availability.GetPoseHistory(AnimContextInfo.AnimContextAvailabilities->AnimContext))
				{
					if (const UPoseSearchDatabase* Database = Availability.Database.Get())
					{
						check(Database->Schema);
						FRoledAnimContextInfos& RoledAnimContextInfos = DatabaseToRoledAnimContextInfos.FindOrAdd(Database);
						RoledAnimContextInfos.AddRoledAnimContextInfos(Availability, AnimContextInfo, PoseHistory, *Database);
					}
					else if (Availability.IsTagValid())
					{
						// since Database is null, but this availability has a valid Tag, we're looking for valid databases by Availability.Tag
						if (const FDatabasesPerTag* DatabasesPerTag = TagToDatabases.Find(Availability.Tag))
						{
							check(!DatabasesPerTag->IsEmpty());
							for (const UPoseSearchDatabase* DatabaseFromTag : *DatabasesPerTag)
							{
								check(DatabaseFromTag && DatabaseFromTag->Schema);
								FRoledAnimContextInfos& RoledAnimContextInfos = DatabaseToRoledAnimContextInfos.FindOrAdd(DatabaseFromTag);
								RoledAnimContextInfos.AddRoledAnimContextInfos(Availability, AnimContextInfo, PoseHistory, *DatabaseFromTag);
							}
						}
						else
						{
							//@todo: should we add a verbose LOG here? not sure since it'd be very spammy...
							
							// this is a valid condition we shouldn't log: for example when the "main character" is loaded and publishing availabilities with a valid tag and null database,
							// looking for other NPC / seconday characters to interact with, but they are not present of didn't publish any availability
						}
					}
					else
					{
						UE_LOG(LogPoseSearch, Log, TEXT("UPoseSearchInteractionSubsystem::GenerateSearchContexts null Availability.Database (with invalid Availability.Tag)"));
					}
				}
				else
				{
					UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchInteractionSubsystem::GenerateSearchContexts couldn't find PoseHistory %s for AnimContext %s"), *Availability.GetPoseHistoryName(), *AnimContextInfo.AnimContextAvailabilities->AnimContext->GetName());
				}
			}
		},
		// OnDoneGroupingAnimContexts: called when the FAnimContextInfoVisitor reaches the end of a group of possibly interacting AnimContext(s)
		[&DatabaseToRoledAnimContextInfos, &SearchContexts, &PreviousValidInteractionSearches]()
		{
			// for each database now we try to create all the possible combinations of the roled anim instances
			// for example, given a database set up with assets for 2 characters interactions with roles RoleA and RoleB
			// and 2 anim instances, all of them willing to partecipate in the 2 characters interaction with both roles RoleA and RoleB:
			// CharA could be taking RoleA and RoleB,
			// CharB could be taking RoleA and RoleB,
			// we generate all the combinations from the array of options:
			// CharA/RoleA, CharA/RoleB, CharB/RoleA, CharB/RoleB
			//
			// and we prune the invalid tuples:
			//
			// CharA/RoleA - CharA/RoleB -> invalid because of same CharA
			// CharA/RoleA - CharB/RoleA -> invalid because of same RoleA
			// CharA/RoleA - CharB/RoleB -> VALID!
			//
			// CharA/RoleB - CharB/RoleA -> VALID!
			// CharA/RoleB - CharB/RoleB -> invalid because of same RoleB
			//
			// CharB/RoleA - CharB/RoleB -> invalid because of same CharB

			for (TPair<const UPoseSearchDatabase*, FRoledAnimContextInfos>& DatabaseToRoledAnimContextInfosPair : DatabaseToRoledAnimContextInfos)
			{
				const UPoseSearchDatabase* Database = DatabaseToRoledAnimContextInfosPair.Key;
				check(Database->Schema);
				const TArray<FPoseSearchRoledSkeleton>& RoledSkeletons = Database->Schema->GetRoledSkeletons();
				const int32 CombinationCardinality = RoledSkeletons.Num();
				FRoledAnimContextInfos& RoledAnimContextInfos = DatabaseToRoledAnimContextInfosPair.Value;

				// sort RoledAnimContextInfos to generate deterministic SearchContext across multiple frames!
				RoledAnimContextInfos.Sort([](const FRoledAnimContextInfo& RoledAnimContextInfoA, const FRoledAnimContextInfo& RoledAnimContextInfoB)
					{
						return RoledAnimContextInfoA.AnimContextInfo->AnimContextAvailabilities->AnimContext < RoledAnimContextInfoB.AnimContextInfo->AnimContextAvailabilities->AnimContext;
					});

				GenerateCombinations(RoledAnimContextInfos.Num(), CombinationCardinality,
					// Combination is an array of indexes in RoledAnimContextInfos: 0 <= Combination[i] < RoledAnimContextInfos.Num()
					[Database, &RoledSkeletons, &RoledAnimContextInfos, &SearchContexts, &PreviousValidInteractionSearches](const TConstArrayView<int32> Combination)
					{
						// CombinationCardinality represents the number of roles as well as the number interacting AnimContext(s) (ultimately number of Characters involved in the interaction)
						const int32 CombinationCardinality = Combination.Num();
						TSet<const UObject*, DefaultKeyFuncs<const UObject*>, TInlineSetAllocator<PreallocatedRolesNum, TMemStackSetAllocator<>>> UniqueAnimContexts;

						for (int32 CombinationIndex = 0; CombinationIndex < CombinationCardinality; ++CombinationIndex)
						{
							const int32 RoledAnimContextIndex = Combination[CombinationIndex];
							const FRoledAnimContextInfo& RoledAnimContextInfo = RoledAnimContextInfos[RoledAnimContextIndex];
							check(RoledAnimContextInfo.AnimContextInfo);
							bool bIsAlreadyInSet;
							UniqueAnimContexts.Add(RoledAnimContextInfo.AnimContextInfo->AnimContextAvailabilities->AnimContext, &bIsAlreadyInSet);
							if (bIsAlreadyInSet)
							{
								// we have a duplicate AnimContext. this combination is NOT valid
								return false;
							}
						}

						FInteractionSearchContext SearchContext;
						SearchContext.Database = Database;

						// setting up a FRoledAnimContextInfo in RoledAnimContextInfos describing 
						// this potential interaction properties about how to perform the search
						for (int32 CombinationIndex = 0; CombinationIndex < CombinationCardinality; ++CombinationIndex)
						{
							const int32 RoledAnimContextIndex = Combination[CombinationIndex];
							const FRoledAnimContextInfo& RoledAnimContextInfo = RoledAnimContextInfos[RoledAnimContextIndex];

							check(RoledAnimContextInfo.PoseHistory && RoledAnimContextInfo.Availability);
							SearchContext.Add(RoledAnimContextInfo.AnimContextInfo->AnimContextAvailabilities->AnimContext, RoledAnimContextInfo.PoseHistory, RoledAnimContextInfo.Role);
							SearchContext.bDisableCollisions |= RoledAnimContextInfo.Availability->bDisableCollisions;
							SearchContext.TickPriorities.Add(RoledAnimContextInfo.Availability->TickPriority);
							
							#if ENABLE_ANIM_DEBUG
							SearchContext.DebugAvailabilities.Add(*RoledAnimContextInfo.Availability);
							#endif // ENABLE_ANIM_DEBUG
						}

						// does SearchContext cover all the roles required by this interaction?
						for (const FPoseSearchRoledSkeleton& RoledSkeleton : RoledSkeletons)
						{
							// CombinationCardinality is usually 2-3, so we can search the SearchContext.Roles array for duplicates without requiring a faster container like TSet
							if (!SearchContext.GetRoles().Contains(RoledSkeleton.Role))
							{
								return false;
							}
						}

						// looking for a preexisting valid interaction resmbling SearchContext
						for (const FValidInteractionSearch& PreviousValidInteractionSearch : PreviousValidInteractionSearches)
						{
							if (PreviousValidInteractionSearch.SearchContext.IsEquivalent(SearchContext))
							{
								SearchContext.bIsContinuingInteraction = true;
								break;
							}
						}

						// checking if this combination is valid for the Database:
						for (int32 CombinationIndex = 0; CombinationIndex < CombinationCardinality; ++CombinationIndex)
						{
							const int32 RoledAnimContextIndex = Combination[CombinationIndex];
							const FRoledAnimContextInfo& RoledAnimContextInfo = RoledAnimContextInfos[RoledAnimContextIndex];

							// checking the narrow phase!
							for (int32 OtherCombinationIndex = CombinationIndex + 1; OtherCombinationIndex < CombinationCardinality; ++OtherCombinationIndex)
							{
								const int32 OtherRoledAnimContextIndex = Combination[OtherCombinationIndex];
								const FRoledAnimContextInfo& OtherRoledAnimContextInfo = RoledAnimContextInfos[OtherRoledAnimContextIndex];

								// if any of the RoledAnimContextInfo cannot interact with any OtherRoledAnimContextInfo the inteaction cannot happen!
								if (!RoledAnimContextInfo.CanInteractWith(OtherRoledAnimContextInfo, SearchContext.bIsContinuingInteraction))
								{
									return false;
								}
							}
						}
#if DO_CHECK
						for (const FInteractionSearchContext& ContainedSearchContext : SearchContexts)
						{
							check(!ContainedSearchContext.IsEquivalent(SearchContext));
						}

						check(SearchContext.CheckForConsistency());
#endif // DO_CHECK
						SearchContexts.Emplace(SearchContext);
						return true;
					});
			}

			// done using DatabaseToRoledAnimContextInfos. clearing up for the next group of AnimContext(s)
			DatabaseToRoledAnimContextInfos.Reset();
		});

	// populating the continuing pose properties for the SearchContexts from the current Islands
	PopulateContinuingProperties(DeltaSeconds, SearchContexts);
}

void UPoseSearchInteractionSubsystem::OnInteractionStart(UE::PoseSearch::FValidInteractionSearch& ValidInteractionSearch)
{
	using namespace UE::PoseSearch;

#if ENABLE_VISUAL_LOG
	ValidInteractionSearch.SearchContext.VLogContext(FColor::Blue);
#endif

	check(ValidInteractionSearch.DisabledCollisions.IsEmpty());
	if (ValidInteractionSearch.SearchContext.bDisableCollisions)
	{
		TArray<AActor*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> Actors;
		TArray<UPrimitiveComponent*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> PrimitiveComponents;

		for (int32 AnimContextIndex = 0; AnimContextIndex < ValidInteractionSearch.SearchContext.Num(); ++AnimContextIndex)
		{
			if (const UObject* AnimContext = ValidInteractionSearch.SearchContext.GetAnimContext(AnimContextIndex))
			{
				AActor* Actor = const_cast<AActor*>(GetContextOwningActor(AnimContext));
				check(Actor);
				Actors.Add(Actor);
				PrimitiveComponents.Add(Cast<UPrimitiveComponent>(Actor->GetRootComponent()));
			}
		}

		for (int32 IndexA = 0; IndexA < Actors.Num(); ++IndexA)
		{
			for (int32 IndexB = IndexA + 1; IndexB < Actors.Num(); ++IndexB)
			{
				AActor* ActorA = Actors[IndexA];
				AActor* ActorB = Actors[IndexB];
				check(ActorA && ActorB);

				UPrimitiveComponent* PrimitiveComponentA = PrimitiveComponents[IndexA];
				UPrimitiveComponent* PrimitiveComponentB = PrimitiveComponents[IndexB];

				if (PrimitiveComponentA && !PrimitiveComponentA->GetMoveIgnoreActors().Contains(ActorB))
				{
					ValidInteractionSearch.DisabledCollisions.Add({ ActorA, ActorB });
					PrimitiveComponentA->IgnoreActorWhenMoving(ActorB, true);
				}

				if (PrimitiveComponentB && !PrimitiveComponentB->GetMoveIgnoreActors().Contains(ActorA))
				{
					ValidInteractionSearch.DisabledCollisions.Add({ ActorB, ActorA });
					PrimitiveComponentB->IgnoreActorWhenMoving(ActorA, true);
				}
			}
		}
	}
}

void UPoseSearchInteractionSubsystem::OnInteractionContinuing(UE::PoseSearch::FValidInteractionSearch& ValidInteractionSearch)
{
#if ENABLE_VISUAL_LOG
	ValidInteractionSearch.SearchContext.VLogContext(FColor::Green);
#endif
}

void UPoseSearchInteractionSubsystem::OnInteractionEnd(UE::PoseSearch::FValidInteractionSearch& ValidInteractionSearch)
{
	using namespace UE::PoseSearch;

#if ENABLE_VISUAL_LOG
	ValidInteractionSearch.SearchContext.VLogContext(FColor::Black);
#endif

	for (const FDisabledCollisions::ElementType& DisabledCollision : ValidInteractionSearch.DisabledCollisions)
	{
		if (AActor* ActorA = DisabledCollision.Key.Get())
		{
			if (AActor* ActorB = DisabledCollision.Value.Get())
			{
				if (UPrimitiveComponent* PrimitiveComponentA = Cast<UPrimitiveComponent>(ActorA->GetRootComponent()))
				{
					PrimitiveComponentA->IgnoreActorWhenMoving(ActorB, false);
				}
			}
		}
	}
}

void UPoseSearchInteractionSubsystem::UpdateValidInteractionSearches()
{
	using namespace UE::PoseSearch;

	const int32 ValidInteractionSearchesNum = ValidInteractionSearches.Num();
	TArray<bool, TInlineAllocator<PreallocatedSearchesNum * 4>> Visited;
	Visited.SetNum(ValidInteractionSearchesNum);

	TArray<FValidInteractionSearch, TInlineAllocator<PreallocatedSearchesNum * 4>> NewValidInteractionSearches;
	for (FInteractionIsland* Island : Islands)
	{
		if (Island->IsInitialized())
		{
			// analyzing ALL current tick interaction results
			for (const FInteractionSearchResult& SearchResult : Island->GetSearchResults())
			{
				const FInteractionSearchContext& SearchContext = Island->GetSearchContexts()[SearchResult.SearchIndex];

				int32 Index = 0;
				for (; Index < ValidInteractionSearchesNum; ++Index)
				{
					if (ValidInteractionSearches[Index].SearchContext.IsEquivalent(SearchContext))
					{
						check(!Visited[Index]);
						Visited[Index] = true;
						OnInteractionContinuing(ValidInteractionSearches[Index]);
						break;
					}
				}

				if (Index == ValidInteractionSearchesNum)
				{
					// we haven't found an equivalent SearchContext in ValidInteractionSearches, so it's a new interaction!
					FValidInteractionSearch& NewValidInteractionSearch = NewValidInteractionSearches.AddDefaulted_GetRef();
					NewValidInteractionSearch.SearchContext = SearchContext;
					OnInteractionStart(NewValidInteractionSearch);
				}
			}
		}
	}

	// checking for leftover unvisited ValidInteractionSearches. Those are interactions that just ended
	for (int32 Index = 0; Index < ValidInteractionSearchesNum; ++Index)
	{
		if (Visited[Index])
		{
			NewValidInteractionSearches.Add(ValidInteractionSearches[Index]);
		}
		else
		{
			OnInteractionEnd(ValidInteractionSearches[Index]);
		}
	}

	ValidInteractionSearches = NewValidInteractionSearches;
}

void UPoseSearchInteractionSubsystem::Tick(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UPoseSearchInteractionSubsystem_Tick);

	using namespace UE::PoseSearch;

	Super::Tick(DeltaSeconds);

	FMemMark Mark(FMemStack::Get());
	UpdateValidInteractionSearches();

	if (AnimContextsAvailabilities.IsEmpty())
	{
		bool bAllUninjected = true;
		for (FInteractionIsland* Island : Islands)
		{
			if (Island->IsInitialized())
			{
				bAllUninjected = false;
			}
		}

		if (bAllUninjected)
		{
			// nothing to do. early out
			return;
		}
	}

	check(IsInGameThread());

	RegenerateAllIslands(DeltaSeconds);

	// clearing up AnimContextsAvailabilities for the next frame
	AnimContextsAvailabilities.Reset();

#if DO_CHECK
	check(ValidateAllIslands());
#endif
}

TStatId UPoseSearchInteractionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPoseSearchInteractionSubsystem, STATGROUP_Tickables);
}

void UPoseSearchInteractionSubsystem::Query_AnyThread(const TArrayView<const FPoseSearchInteractionAvailability> Availabilities, const UObject* AnimContext, 
	FPoseSearchBlueprintResult& Result, FName PoseHistoryName, const UE::PoseSearch::IPoseHistory* PoseHistory, bool bValidateResultAgainstAvailabilities)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UPoseSearchInteractionSubsystem_Query_AnyThread);

	using namespace UE::PoseSearch;

	Result = FPoseSearchBlueprintResult();

#if !NO_CVARS
	if (!GVarPoseSearchInteractionEnabled)
	{
		return;
	}
#endif // !NO_CVARS

	// if we find AnimContext in an island, we perform ALL the Island motion matching searches.
	if (FInteractionIsland* Island = FindIsland(AnimContext))
	{
		Island->DoSearch_AnyThread(AnimContext, ValidInteractionSearches, Result);

		if (bValidateResultAgainstAvailabilities && Result.SelectedAnim)
		{
			bool bResultValidated = false;

			for (const FPoseSearchInteractionAvailability& Availability : Availabilities)
			{
				const bool bIsDatabaseValidates = (Availability.IsTagValid() && !Availability.Database) || (Availability.Database == Result.SelectedDatabase);
				if (bIsDatabaseValidates &&	(Availability.RolesFilter.IsEmpty() || Availability.RolesFilter.Contains(Result.Role)))
				{
					bResultValidated = true;
					break;
				}
			}

			if (!bResultValidated)
			{
				Result = FPoseSearchBlueprintResult();
			}
		}
	}

	// queuing the availabilities for the next frame Query_AnyThread
	AddAvailabilities(Availabilities, AnimContext, PoseHistoryName, PoseHistory);
}

void UPoseSearchInteractionSubsystem::GetResult_AnyThread(const UObject* AnimContext, FPoseSearchBlueprintResult& Result, bool bCompareOwningActors)
{
	using namespace UE::PoseSearch;

	if (FInteractionIsland* Island = FindIsland(AnimContext, bCompareOwningActors))
	{
		Island->GetResult_AnyThread(AnimContext, Result, bCompareOwningActors);
	}
	else
	{
		Result = FPoseSearchBlueprintResult();
	}
}

#if ENABLE_ANIM_DEBUG
void UPoseSearchInteractionSubsystem::DebugDrawIslands() const
{
#if ENABLE_VISUAL_LOG

	using namespace UE::PoseSearch;

	check(IsInGameThread());

	if (!FVisualLogger::IsRecording())
	{
		return;
	}

	static const FColor Colors[] =
	{
		FColor::White,
		FColor::Black,
		FColor::Red,
		FColor::Green,
		FColor::Blue,
		FColor::Yellow,
		FColor::Cyan,
		FColor::Magenta,
		FColor::Orange,
		FColor::Purple,
		FColor::Turquoise,
		FColor::Silver,
		FColor::Emerald
	};
	static const int32 NumColors = sizeof(Colors) / sizeof(Colors[0]);
	int32 CurrentColorIndex = 0;

	TArray<const UObject*, TInlineAllocator<256>> AllAnimContexts;
	for (const FInteractionIsland* Island : Islands)
	{
		for (const TWeakObjectPtr<const UObject>& IslandAnimContextPtr : Island->GetIslandAnimContexts())
		{
			if (const UObject* IslandAnimContext = IslandAnimContextPtr.Get())
			{
				AllAnimContexts.Add(IslandAnimContext);
			}
		}
	}

	for (const FInteractionIsland* Island : Islands)
	{
		if (Island->IsInitialized())
		{
			const FColor& Color = Colors[CurrentColorIndex];

			for (const FInteractionSearchContext& SearchContext : Island->GetSearchContexts())
			{
				for (int32 Index = 0; Index < SearchContext.Num(); ++Index)
				{
					if (const UObject* AnimContext = SearchContext.GetAnimContext(Index))
					{
						const FPoseSearchInteractionAvailability& DebugAvailability = SearchContext.DebugAvailabilities[Index];
						float MaxBroadPhaseRadius;
						if (SearchContext.bIsContinuingInteraction)
						{
							MaxBroadPhaseRadius = DebugAvailability.BroadPhaseRadius + DebugAvailability.BroadPhaseRadiusIncrementOnInteraction;
						}
						else
						{
							MaxBroadPhaseRadius = DebugAvailability.BroadPhaseRadius;
						}

						if (MaxBroadPhaseRadius > UE_SMALL_NUMBER)
						{
							const FTransform& Transform = GetContextTransform(AnimContext);
							static const TCHAR* LogName = TEXT("PoseSearchInteraction");
								
							for (const UObject* IslandAnimContext : AllAnimContexts)
							{
								UE_VLOG_CIRCLE(IslandAnimContext, LogName, Display, Transform.GetLocation(), FVector::UpVector, MaxBroadPhaseRadius, Color, TEXT(""));
							}

							if (!Island->HasTickDependencies())
							{
								const FVector ForwardAxisStart = Transform.TransformPosition(FVector::ForwardVector * MaxBroadPhaseRadius);
								const FVector ForwardAxisEnd = Transform.TransformPosition(FVector::ForwardVector * -MaxBroadPhaseRadius);

								const FVector LeftAxisStart = Transform.TransformPosition(FVector::LeftVector * MaxBroadPhaseRadius);
								const FVector LeftAxisEnd = Transform.TransformPosition(FVector::LeftVector * -MaxBroadPhaseRadius);

								for (const UObject* IslandAnimContext : AllAnimContexts)
								{
									UE_VLOG_SEGMENT(IslandAnimContext, LogName, Display, ForwardAxisStart, ForwardAxisEnd, Color, TEXT(""));
									UE_VLOG_SEGMENT(IslandAnimContext, LogName, Display, LeftAxisStart, LeftAxisEnd, Color, TEXT(""));
								}
							}
						}
					}
				}
			}

			CurrentColorIndex = (CurrentColorIndex + 1) % NumColors;
		}
	}
#endif // ENABLE_VISUAL_LOG
}

void UPoseSearchInteractionSubsystem::DebugLogTickDependencies() const
{
#if !NO_CVARS
	using namespace UE::PoseSearch;

	if (GVarPoseSearchInteractionLoglandsTickDependencies)
	{
		UE_LOG(LogPoseSearch, Log, TEXT("=================================================================="));
		for (const FInteractionIsland* Island : Islands)
		{
			if (Island->IsInitialized())
			{
				Island->LogTickDependencies();
			}
		}
	}
#endif // !NO_CVARS
}
#endif // ENABLE_ANIM_DEBUG
