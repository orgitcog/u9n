// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchLibrary.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSubsystem_Tag.h"
#include "Animation/BlendSpace.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "Animation/AnimTrace.h"
#include "GameFramework/Character.h"
#include "StructUtils/InstancedStruct.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/MultiAnimAsset.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchFeatureChannel_Trajectory.h"
#include "PoseSearch/Trace/PoseSearchTraceLogger.h"
#include "PoseSearch/PoseSearchFeatureChannel_PermutationTime.h"
#include "UObject/FastReferenceCollector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoseSearchLibrary)

#define LOCTEXT_NAMESPACE "PoseSearchLibrary"

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
static bool GVarAnimMotionMatchDrawQueryEnable = false;
static FAutoConsoleVariableRef CVarAnimMotionMatchDrawQueryEnable(TEXT("a.MotionMatch.DrawQuery.Enable"), GVarAnimMotionMatchDrawQueryEnable, TEXT("Enable / Disable MotionMatch Draw Query"));

static bool GVarAnimMotionMatchDrawMatchEnable = false;
static FAutoConsoleVariableRef CVarAnimMotionMatchDrawMatchEnable(TEXT("a.MotionMatch.DrawMatch.Enable"), GVarAnimMotionMatchDrawMatchEnable, TEXT("Enable / Disable MotionMatch Draw Match"));
#endif

namespace UE::PoseSearch
{
	// budgeting some stack allocations for simple use cases. bigger requests of AnimationAssets containing UAnimNotifyState_PoseSearchBranchIn 
	// referencing multiple databases will default to a slower TMemStackAllocator (that hides heap allocations)
	enum { MAX_STACK_ALLOCATED_ANIMATIONS = 16 };
	enum { MAX_STACK_ALLOCATED_SETS = 2 };
	typedef	TArray<const UObject*, TInlineAllocator<MAX_STACK_ALLOCATED_ANIMATIONS, TMemStackAllocator<>>> FAssetsToSearch;
	// an empty FAssetsToSearch associated to Database means we need to search ALL the assets
	typedef TMap<const UPoseSearchDatabase*, FAssetsToSearch, TInlineSetAllocator<MAX_STACK_ALLOCATED_SETS, TMemStackSetAllocator<>>> FAssetsToSearchPerDatabaseMap;
	typedef TPair<const UPoseSearchDatabase*, FAssetsToSearch> TAssetsToSearchPerDatabasePair;
	typedef TMap<const UPoseSearchDatabase*, FSearchResult, TInlineSetAllocator<MAX_STACK_ALLOCATED_SETS, TMemStackSetAllocator<>>> FReconstructedPreviousSearchBestResultMap;

	// this function adds AssetToSearch to the search of Database
	// returns bAsyncBuildIndexInProgress
	static bool AddToSearchForDatabase(FAssetsToSearchPerDatabaseMap& AssetsToSearchPerDatabaseMap, const UObject* AssetToSearch, const UPoseSearchDatabase* Database, bool bContainsIsMandatory)
	{
		FAssetsToSearch* AssetsToSearch = AssetsToSearchPerDatabaseMap.Find(Database);

#if WITH_EDITOR
		// no need to check if Database is indexing if found into AssetsToSearchPerDatabaseMap, since it already passed RequestAsyncBuildIndex successfully in a previous AddToSearchForDatabase call
		if (!AssetsToSearch && (EAsyncBuildIndexResult::Success != FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest)))
		{
			// database is still indexing.. moving on
			return true;
		}
#endif // WITH_EDITOR

		if (!Database->Contains(AssetToSearch))
		{
			if (bContainsIsMandatory)
			{
				UE_LOG(LogPoseSearch, Error, TEXT("improperly setup UAnimSequenceBase. Database %s doesn't contain UAnimSequenceBase %s"), *Database->GetName(), *AssetToSearch->GetName());
			}
			return false;
		}

		// making sure AssetToSearch is not a databases! later on we could add support for nested databases, but currently we don't support that
		check(Cast<const UPoseSearchDatabase>(AssetToSearch) == nullptr);

		if (AssetsToSearch)
		{
			// an empty FAssetsToSearch associated to Database means we need to search ALL the assets, so we don't need to add this AssetToSearch
			if (!AssetsToSearch->IsEmpty())
			{
				AssetsToSearch->AddUnique(AssetToSearch);
			}
		}
		else
		{
			// no need to AddUnique since it's the first one
			AssetsToSearchPerDatabaseMap.Add(Database).Add(AssetToSearch);
		}

		return false;
	}

	// this function is looking for UPoseSearchDatabase(s) to search for the input AssetToSearch:
	// if AssetToSearch is a database search it ALL,
	// if it's a sequence containing UAnimNotifyState_PoseSearchBranchIn, we add to the search of the dabase UAnimNotifyState_PoseSearchBranchIn::Database the asset AssetToSearch
	// returns bAsyncBuildIndexInProgress
	static bool AddToSearch(FAssetsToSearchPerDatabaseMap& AssetsToSearchPerDatabaseMap, const UObject* AssetToSearch)
	{
		bool bAsyncBuildIndexInProgress = false;
		if (const UAnimSequenceBase* SequenceBase = Cast<const UAnimSequenceBase>(AssetToSearch))
		{
			for (const FAnimNotifyEvent& NotifyEvent : SequenceBase->Notifies)
			{
				if (const UAnimNotifyState_PoseSearchBranchIn* PoseSearchBranchIn = Cast<UAnimNotifyState_PoseSearchBranchIn>(NotifyEvent.NotifyStateClass))
				{
					if (!PoseSearchBranchIn->Database)
					{
						UE_LOG(LogPoseSearch, Error, TEXT("improperly setup UAnimNotifyState_PoseSearchBranchIn with null Database in %s"), *SequenceBase->GetName());
						continue;
					}
					
					// we just skip indexing databases to keep the experience as smooth as possible
					if (AddToSearchForDatabase(AssetsToSearchPerDatabaseMap, SequenceBase, PoseSearchBranchIn->Database, true))
					{
						bAsyncBuildIndexInProgress = true;
					}
				}
			}
		}
		else if (const UPoseSearchDatabase* Database = Cast<UPoseSearchDatabase>(AssetToSearch))
		{
			// we already added Database to AssetsToSearchPerDatabaseMap, so it already successfully passed RequestAsyncBuildIndex
			if (FAssetsToSearch* AssetsToSearch = AssetsToSearchPerDatabaseMap.Find(Database))
			{
				// an empty FAssetsToSearch associated to Database means we need to search ALL the assets
				AssetsToSearch->Reset();
			}
			else
#if WITH_EDITOR
			if (EAsyncBuildIndexResult::Success != FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
			{
				bAsyncBuildIndexInProgress = true;
			}
			else
#endif // WITH_EDITOR
			{
				// an empty FAssetsToSearch associated to Database means we need to search ALL the assets
				AssetsToSearchPerDatabaseMap.Add(Database);
			}
		}

		return bAsyncBuildIndexInProgress;
	}

	static void PopulateContinuingPoseSearches(const UObject* PlayingAnimationAsset, const TArrayView<const UObject*> AssetsToSearch, FSearchContext& SearchContext, FAssetsToSearchPerDatabaseMap& ContinuingPoseAssetsToSearchPerDatabaseMap)
	{
		// checking if PlayingAnimationAsset has an associated database
		if (AddToSearch(ContinuingPoseAssetsToSearchPerDatabaseMap, PlayingAnimationAsset))
		{
#if WITH_EDITOR
			SearchContext.SetAsyncBuildIndexInProgress();
#endif // WITH_EDITOR
		}
		
		// checking if any of the AssetsToSearch (databse) contains PlayingAnimationAsset
		for (const UObject* AssetToSearch : AssetsToSearch)
		{
			if (const UPoseSearchDatabase* Database = Cast<UPoseSearchDatabase>(AssetToSearch))
			{
				// since it cannot be a database we can directly add it to ContinuingPoseAssetsToSearchPerDatabaseMap
				if (AddToSearchForDatabase(ContinuingPoseAssetsToSearchPerDatabaseMap, PlayingAnimationAsset, Database, false))
				{
#if WITH_EDITOR
					SearchContext.SetAsyncBuildIndexInProgress();
#endif // WITH_EDITOR
				}
			}
		}
	}
	
	static void PopulateSearches(const TArrayView<const UObject*> AssetsToSearch, FSearchContext& SearchContext, FAssetsToSearchPerDatabaseMap& AssetsToSearchPerDatabaseMap)
	{
		if (!AssetsToSearch.IsEmpty())
		{
			for (const UObject* AssetToSearch : AssetsToSearch)
			{
				if (AddToSearch(AssetsToSearchPerDatabaseMap, AssetToSearch))
				{
#if WITH_EDITOR
					SearchContext.SetAsyncBuildIndexInProgress();
#endif // WITH_EDITOR
				}
			}
		}
	}

	template <typename DatabasesContainer>
	static bool IsForceInterrupt(EPoseSearchInterruptMode InterruptMode, const UPoseSearchDatabase* CurrentResultDatabase, const DatabasesContainer& Databases)
	{
		switch (InterruptMode)
		{
		case EPoseSearchInterruptMode::DoNotInterrupt:
			return false;

		case EPoseSearchInterruptMode::InterruptOnDatabaseChange:	// Fall through
		case EPoseSearchInterruptMode::InterruptOnDatabaseChangeAndInvalidateContinuingPose:
			return !Databases.Contains(CurrentResultDatabase);

		case EPoseSearchInterruptMode::ForceInterrupt:				// Fall through
		case EPoseSearchInterruptMode::ForceInterruptAndInvalidateContinuingPose:
			return true;

		default:
			checkNoEntry();
			return false;
		}
	}

	template <typename DatabasesContainer>
	static bool IsInvalidatingContinuingPose(EPoseSearchInterruptMode InterruptMode, const UPoseSearchDatabase* CurrentResultDatabase, const DatabasesContainer& Databases)
	{
		switch (InterruptMode)
		{
		case EPoseSearchInterruptMode::DoNotInterrupt:				// Fall through
		case EPoseSearchInterruptMode::InterruptOnDatabaseChange:	// Fall through
		case EPoseSearchInterruptMode::ForceInterrupt:	
			return false;

		case EPoseSearchInterruptMode::InterruptOnDatabaseChangeAndInvalidateContinuingPose:
			return !Databases.Contains(CurrentResultDatabase);

		case EPoseSearchInterruptMode::ForceInterruptAndInvalidateContinuingPose:
			return true;

		default:
			checkNoEntry();
			return false;
		}
	}

	static bool ShouldUseCachedChannelData(const UPoseSearchDatabase* CurrentResultDatabase, const TConstArrayView<TObjectPtr<const UPoseSearchDatabase>> Databases)
	{
		const UPoseSearchSchema* OneOfTheSchemas = nullptr;
		if (CurrentResultDatabase)
		{
			OneOfTheSchemas = CurrentResultDatabase->Schema;
		}

		for (const TObjectPtr<const UPoseSearchDatabase>& Database : Databases)
		{
			if (Database)
			{
				if (OneOfTheSchemas != Database->Schema)
				{
					if (OneOfTheSchemas == nullptr)
					{
						OneOfTheSchemas = Database->Schema;
					}
					else
					{
						// we found we need to search multiple schemas
						return true;
					}
				}
			}
		}

		return false;
	}

	static FRole GetCommonDefaultRole(const TConstArrayView<TObjectPtr<const UPoseSearchDatabase>> Databases)
	{
		FRole Role = DefaultRole;

		if (!Databases.IsEmpty())
		{
			if (const UPoseSearchDatabase* Database = Databases[0].Get())
			{
				if (const UPoseSearchSchema* Schema = Database->Schema)
				{
					Role = Schema->GetDefaultRole();
				}
			}

#if WITH_EDITOR && ENABLE_ANIM_DEBUG 
			for (int32 DatabaseIndex = 1; DatabaseIndex < Databases.Num(); ++DatabaseIndex)
			{
				if (const UPoseSearchDatabase* Database = Databases[DatabaseIndex].Get())
				{
					if (const UPoseSearchSchema* Schema = Database->Schema)
					{
						if (Role != Schema->GetDefaultRole())
						{
							UE_LOG(LogPoseSearch, Error, TEXT("GetCommonDefaultRole - inconsistent Role between provided Databases!"));
							break;
						}
					}
				}
			}
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
		}

		return Role;
	}

	static float CalculateWantedPlayRate(const UE::PoseSearch::FSearchResult& CurrentSearchResult, const UE::PoseSearch::FSearchContext& SearchContext, const FFloatInterval& PlayRate, float TrajectorySpeedMultiplier, const FPoseSearchEvent& EventToSearch)
	{
		float WantedPlayRate = 1.f;

		if (CurrentSearchResult.IsValid())
		{
			if (CurrentSearchResult.IsEventSearchResult())
			{
				// checking if CurrentSearchResult.EventPoseIdx is part of the EventToSearch.EventTag.
				// If not, it's an event from a continuing pose search that hasn't been interrupted,
				// so we keep the previously calculated WantedPlayRate
				const bool bIsEventSearchFromTag = CurrentSearchResult.IsEventSearchFromTag(EventToSearch.EventTag);
				if (bIsEventSearchFromTag)
				{
					const float TimeToEvent = CurrentSearchResult.CalculateTimeToEvent();
					if (TimeToEvent > UE_KINDA_SMALL_NUMBER && EventToSearch.TimeToEvent > UE_KINDA_SMALL_NUMBER)
					{
						// EventToSearch.TimeToEvent is the desired time to event, and TimeToEvent is the actually current time to event. we calculate WantedPlayRate as ratio between the two
						WantedPlayRate = TimeToEvent / EventToSearch.TimeToEvent;
					}
					// if we passed the event (TimeToEvent <= 0) we leave the WantedPlayRate as previously calculated
				}
			}
			else if (!ensure(PlayRate.Min <= PlayRate.Max && PlayRate.Min > UE_KINDA_SMALL_NUMBER))
			{
				UE_LOG(LogPoseSearch, Error, TEXT("Couldn't update the WantedPlayRate in CalculateWantedPlayRate, because of invalid PlayRate interval (%f, %f)"), PlayRate.Min, PlayRate.Max);
				WantedPlayRate = 1.f;
			}
			else if (!FMath::IsNearlyEqual(PlayRate.Min, PlayRate.Max, UE_KINDA_SMALL_NUMBER))
			{
				TConstArrayView<float> QueryData = SearchContext.GetCachedQuery(CurrentSearchResult.Database->Schema);
				if (!QueryData.IsEmpty())
				{
					if (const UPoseSearchFeatureChannel_Trajectory* TrajectoryChannel = CurrentSearchResult.Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_Trajectory>())
					{
						TConstArrayView<float> ResultData = CurrentSearchResult.Database->GetSearchIndex().GetPoseValues(CurrentSearchResult.PoseIdx);
						const float EstimatedSpeedRatio = TrajectoryChannel->GetEstimatedSpeedRatio(QueryData, ResultData);

						WantedPlayRate = FMath::Clamp(EstimatedSpeedRatio, PlayRate.Min, PlayRate.Max);
					}
					else
					{
						UE_LOG(LogPoseSearch, Warning,
							TEXT("Couldn't update the WantedPlayRate in CalculateWantedPlayRate, because Schema '%s' couldn't find a UPoseSearchFeatureChannel_Trajectory channel"),
							*GetNameSafe(CurrentSearchResult.Database->Schema));
					}
				}
			}
			else if (!FMath::IsNearlyZero(TrajectorySpeedMultiplier))
			{
				WantedPlayRate = PlayRate.Min / TrajectorySpeedMultiplier;
			}
			else
			{
				WantedPlayRate = PlayRate.Min;
			}
		}

		return WantedPlayRate;
	}
}

//////////////////////////////////////////////////////////////////////////
// FMotionMatchingState

void FMotionMatchingState::Reset(const FTransform& ComponentTransform)
{
	Reset();
}

void FMotionMatchingState::Reset()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bJumpedToPose = false;
	WantedPlayRate = 1.f;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	SearchResult = FPoseSearchBlueprintResult();
	// Set the elapsed time to INFINITY to trigger a search right away
	ElapsedPoseSearchTime = std::numeric_limits<float>::infinity();
	PoseIndicesHistory.Reset();
}

void FMotionMatchingState::AdjustAssetTime(float AssetTime)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	CurrentSearchResult.UpdateWithNormalizedTime(AssetTime);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FVector FMotionMatchingState::GetEstimatedFutureRootMotionVelocity() const
{
	using namespace UE::PoseSearch;
	if (const UPoseSearchDatabase* Database = SearchResult.SelectedDatabase.Get())
	{
		if (const UPoseSearchFeatureChannel_Trajectory* TrajectoryChannel = Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_Trajectory>())
		{
			const int32 PoseIndex = Database->GetPoseIndex(SearchResult.SelectedAnim.Get(), SearchResult.SelectedTime, SearchResult.bIsMirrored, SearchResult.BlendParameters);
			const FSearchIndex& SearchIndex = Database->GetSearchIndex();
			if (!SearchIndex.IsValuesEmpty())
			{
				TConstArrayView<float> ResultData = SearchIndex.GetPoseValues(PoseIndex);
				return TrajectoryChannel->GetEstimatedFutureRootMotionVelocity(ResultData);
			}
		}
	}

	return FVector::ZeroVector;
}

void FMotionMatchingState::UpdateWantedPlayRate(const UE::PoseSearch::FSearchContext& SearchContext, const FFloatInterval& PlayRate, float TrajectorySpeedMultiplier, const FPoseSearchEvent& EventToSearch)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	WantedPlayRate = CalculateWantedPlayRate(CurrentSearchResult, SearchContext, PlayRate, TrajectorySpeedMultiplier, EventToSearch);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#if UE_POSE_SEARCH_TRACE_ENABLED
void UPoseSearchLibrary::TraceMotionMatching(
		UE::PoseSearch::FSearchContext& SearchContext,
		const UE::PoseSearch::FSearchResult& SearchResult,
		float ElapsedPoseSearchTime,
		float DeltaTime,
		bool bSearch,
		float WantedPlayRate,
		EPoseSearchInterruptMode InterruptMode)
{
	using namespace UE::PoseSearch;

	const bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(PoseSearchChannel);
	if (!bChannelEnabled)
	{
		return;
	}

	float RecordingTime = 0.f;
	if (!SearchContext.GetContexts().IsEmpty())
	{
		if (const UObject* FirstObject = SearchContext.GetContexts()[0]->GetFirstObjectParam())
		{
			RecordingTime = FObjectTrace::GetWorldElapsedTime(FirstObject->GetWorld());
		}
	}

	uint32 SearchId = 787;

	FTraceMotionMatchingStateMessage TraceState;

	TraceState.InterruptMode = InterruptMode;

	const int32 AnimContextsNum = SearchContext.GetContexts().Num();
	TraceState.SkeletalMeshComponentIds.SetNum(AnimContextsNum);

	for (int32 AnimInstanceIndex = 0; AnimInstanceIndex < AnimContextsNum; ++AnimInstanceIndex)
	{
		if (const FChooserEvaluationContext* AnimContext = SearchContext.GetContexts()[AnimInstanceIndex])
		{
			const UObject* FirstObject = AnimContext->GetFirstObjectParam();
			const UObject* SkeletalMeshComponent = nullptr;
			if (const UAnimInstance* AnimInstance = Cast<UAnimInstance>(FirstObject))
			{
				SkeletalMeshComponent = AnimInstance->GetOuter();
			}
			else if (const UActorComponent* ActorComponent = Cast<UActorComponent>(FirstObject))
			{
				const AActor* Actor = ActorComponent->GetOwner();
				check(Actor);
				
				SkeletalMeshComponent = Actor->GetComponentByClass<USkeletalMeshComponent>();
			}

			if (!SkeletalMeshComponent || CANNOT_TRACE_OBJECT(SkeletalMeshComponent))
			{
				return;
			}
			
			TraceState.SkeletalMeshComponentIds[AnimInstanceIndex] = FObjectTrace::GetObjectId(SkeletalMeshComponent);
		}
	}

	for (int32 AnimInstanceIndex = 0; AnimInstanceIndex < AnimContextsNum; ++AnimInstanceIndex)
	{
		const FChooserEvaluationContext* Context = SearchContext.GetContexts()[AnimInstanceIndex];
		if (const UObject* Object = Context->GetFirstObjectParam())
		{
			TRACE_OBJECT(Object);
			SearchId = HashCombineFast(SearchId, GetTypeHash(FObjectTrace::GetObjectId(Object)));
		}
	}

	TraceState.Roles.SetNum(AnimContextsNum);
	for (const FRoleToIndexPair& RoleToIndexPair : SearchContext.GetRoleToIndex())
	{
		TraceState.Roles[RoleToIndexPair.Value] = RoleToIndexPair.Key;
	}

	SearchId = HashCombineFast(SearchId, GetTypeHash(TraceState.Roles));

	// @todo: do we need to hash pose history names in SearchId as well?
	TraceState.PoseHistories.SetNum(AnimContextsNum);
	for (int32 AnimInstanceIndex = 0; AnimInstanceIndex < AnimContextsNum; ++AnimInstanceIndex)
	{
		TraceState.PoseHistories[AnimInstanceIndex].InitFrom(SearchContext.GetPoseHistories()[AnimInstanceIndex]);
	}

	TArray<uint64, TInlineAllocator<64>> DatabaseIds;
	int32 DbEntryIdx = 0;
	const int32 CurrentPoseIdx = bSearch && SearchResult.PoseCost.IsValid() ? SearchResult.PoseIdx : INDEX_NONE;
	TraceState.DatabaseEntries.SetNum(SearchContext.GetBestPoseCandidatesMap().Num());
	for (TPair<const UPoseSearchDatabase*, FSearchContext::FBestPoseCandidates> DatabaseBestPoseCandidates : SearchContext.GetBestPoseCandidatesMap())
	{
		const UPoseSearchDatabase* Database = DatabaseBestPoseCandidates.Key;
		check(Database);

		FTraceMotionMatchingStateDatabaseEntry& DbEntry = TraceState.DatabaseEntries[DbEntryIdx];

		// if throttling is on, the continuing pose can be valid, but no actual search occurred, so the query will not be cached, and we need to build it
		DbEntry.QueryVector = SearchContext.GetOrBuildQuery(Database->Schema);
		DbEntry.DatabaseId = FTraceMotionMatchingStateMessage::GetIdFromObject(Database);
		DatabaseIds.Add(DbEntry.DatabaseId);

		for (int32 CandidateIdx = 0; CandidateIdx < DatabaseBestPoseCandidates.Value.Num(); ++CandidateIdx)
		{
			const FSearchContext::FPoseCandidate PoseCandidate = DatabaseBestPoseCandidates.Value.GetUnsortedCandidate(CandidateIdx);

			FTraceMotionMatchingStatePoseEntry PoseEntry;
			PoseEntry.DbPoseIdx = PoseCandidate.PoseIdx;
			PoseEntry.Cost = PoseCandidate.Cost;
			PoseEntry.PoseCandidateFlags = PoseCandidate.PoseCandidateFlags;
			if (CurrentPoseIdx == PoseCandidate.PoseIdx && SearchResult.Database.Get() == Database)
			{
				check(EnumHasAnyFlags(PoseEntry.PoseCandidateFlags, EPoseCandidateFlags::Valid_Pose | EPoseCandidateFlags::Valid_ContinuingPose));

				EnumAddFlags(PoseEntry.PoseCandidateFlags, EPoseCandidateFlags::Valid_CurrentPose);

				TraceState.CurrentDbEntryIdx = DbEntryIdx;
				TraceState.CurrentPoseEntryIdx = DbEntry.PoseEntries.Add(PoseEntry);
			}
			else
			{
				DbEntry.PoseEntries.Add(PoseEntry);
			}
		}

		++DbEntryIdx;
	}

	DatabaseIds.Sort();
	SearchId = HashCombineFast(SearchId, GetTypeHash(DatabaseIds));

	if (DeltaTime > SMALL_NUMBER)
	{
		// simulation
		if (SearchContext.AnyCachedQuery())
		{
			TraceState.SimLinearVelocity = 0.f;
			TraceState.SimAngularVelocity = 0.f;

			const int32 NumRoles = SearchContext.GetRoleToIndex().Num();
			for (const FRoleToIndexPair& RoleToIndexPair : SearchContext.GetRoleToIndex())
			{
				const FRole& Role = RoleToIndexPair.Key;

				const FTransform PrevRoot = SearchContext.GetWorldBoneTransformAtTime(-DeltaTime, Role, RootSchemaBoneIdx);
				const FTransform CurrRoot = SearchContext.GetWorldBoneTransformAtTime(0.f, Role, RootSchemaBoneIdx);
				
				const FTransform SimDelta = CurrRoot.GetRelativeTransform(PrevRoot);
				TraceState.SimLinearVelocity += SimDelta.GetTranslation().Size() / (DeltaTime * NumRoles);
				TraceState.SimAngularVelocity += FMath::RadiansToDegrees(SimDelta.GetRotation().GetAngle()) / (DeltaTime * NumRoles);
			}
		}

		const FSearchIndexAsset* SearchIndexAsset = SearchResult.GetSearchIndexAsset();
		const UPoseSearchDatabase* CurrentResultDatabase = SearchResult.Database.Get();
		if (SearchIndexAsset && CurrentResultDatabase)
		{
			const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = CurrentResultDatabase->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(*SearchIndexAsset);
			check(DatabaseAsset);
			if (UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(DatabaseAsset->GetAnimationAsset()))
			{
				// Simulate the time step to get accurate root motion prediction for this frame.
				FAnimationAssetSampler Sampler(AnimationAsset, FTransform::Identity,FVector::ZeroVector, FAnimationAssetSampler::DefaultRootTransformSamplingRate, true, false);

				const float TimeStep = DeltaTime * WantedPlayRate;
				const FTransform PrevRoot = Sampler.ExtractRootTransform(SearchResult.AssetTime);
				const FTransform CurrRoot = Sampler.ExtractRootTransform(SearchResult.AssetTime + TimeStep);
				const FTransform RootMotionTransformDelta = PrevRoot.GetRelativeTransform(CurrRoot);
				TraceState.AnimLinearVelocity = RootMotionTransformDelta.GetTranslation().Size() / DeltaTime;
				TraceState.AnimAngularVelocity = FMath::RadiansToDegrees(RootMotionTransformDelta.GetRotation().GetAngle()) / DeltaTime;

				// Need another root motion extraction for non-playrate version in case acceleration isn't the same.
				const FTransform CurrRootNoTimescale = Sampler.ExtractRootTransform(SearchResult.AssetTime + DeltaTime);
				const FTransform RootMotionTransformDeltaNoTimescale = PrevRoot.GetRelativeTransform(CurrRootNoTimescale);
				TraceState.AnimLinearVelocityNoTimescale = RootMotionTransformDeltaNoTimescale.GetTranslation().Size() / DeltaTime;
				TraceState.AnimAngularVelocityNoTimescale = FMath::RadiansToDegrees(RootMotionTransformDeltaNoTimescale.GetRotation().GetAngle()) / DeltaTime;
			}
		}
		TraceState.Playrate = WantedPlayRate;
	}

	TraceState.ElapsedPoseSearchTime = ElapsedPoseSearchTime;
	TraceState.AssetPlayerTime = SearchResult.AssetTime;
	TraceState.DeltaTime = DeltaTime;

	TraceState.RecordingTime = RecordingTime;
	TraceState.SearchBestCost = SearchResult.PoseCost;
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
	TraceState.SearchBruteForceCost = SearchResult.BruteForcePoseCost;
	TraceState.SearchBestPosePos = SearchResult.BestPosePos;
#else // WITH_EDITOR && ENABLE_ANIM_DEBUG
	TraceState.SearchBruteForceCost = 0.f;
	TraceState.SearchBestPosePos = 0;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG

	TraceState.Cycle = FPlatformTime::Cycles64();

	// @todo: avoid publishing duplicated TraceState in ALL the AnimContexts! -currently necessary for multi character-
	for (const FChooserEvaluationContext* Context : SearchContext.GetContexts())
	{
		TraceState.AnimInstanceId = FObjectTrace::GetObjectId(Context->GetFirstObjectParam());
		TraceState.NodeId = SearchId;
		TraceState.Output();
	}
}
#endif // UE_POSE_SEARCH_TRACE_ENABLED

void UPoseSearchLibrary::UpdateMotionMatchingState(
	const FAnimationUpdateContext& Context,
	const TArray<TObjectPtr<const UPoseSearchDatabase>>& Databases,
	float BlendTime,
	int32 MaxActiveBlends,
	const FFloatInterval& PoseJumpThresholdTime,
	float PoseReselectHistory,
	float SearchThrottleTime,
	const FFloatInterval& PlayRate,
	FMotionMatchingState& InOutMotionMatchingState,
	EPoseSearchInterruptMode InterruptMode,
	bool bShouldSearch,
	bool bShouldUseCachedChannelData,
	bool bDebugDrawQuery,
	bool bDebugDrawCurResult)
{
	using namespace UE::PoseSearch;

	if (Databases.IsEmpty())
	{
		Context.LogMessage(
			EMessageSeverity::Error,
			LOCTEXT("NoDatabases", "No database assets provided for motion matching."));
		return;
	}

	const IPoseHistory* PoseHistory = nullptr;
	if (const FPoseHistoryProvider* PoseHistoryProvider = Context.GetMessage<FPoseHistoryProvider>())
	{
		PoseHistory = &PoseHistoryProvider->GetPoseHistory();
	}

	check(Context.AnimInstanceProxy);
	FChooserEvaluationContext AnimContext(Context.AnimInstanceProxy->GetAnimInstanceObject());
	UpdateMotionMatchingState(&AnimContext, PoseHistory, Databases, Context.GetDeltaTime(),
		PoseJumpThresholdTime, PoseReselectHistory, bShouldSearch ? SearchThrottleTime : UE_BIG_NUMBER, PlayRate, InOutMotionMatchingState,
		InterruptMode, bShouldUseCachedChannelData, bDebugDrawQuery, bDebugDrawCurResult);
}

void UPoseSearchLibrary::UpdateMotionMatchingState(
	const UObject* AnimContext,
	const UE::PoseSearch::IPoseHistory* PoseHistory,
	const TConstArrayView<TObjectPtr<const UPoseSearchDatabase>> Databases,
	float DeltaTime,
	const FFloatInterval& PoseJumpThresholdTime,
	float PoseReselectHistory,
	float SearchThrottleTime,
	const FFloatInterval& PlayRate,
	FMotionMatchingState& InOutMotionMatchingState,
	EPoseSearchInterruptMode InterruptMode,
	bool bShouldUseCachedChannelData,
	bool bDebugDrawQuery,
	bool bDebugDrawCurResult,
	const FPoseSearchEvent& EventToSearch)
{
	FChooserEvaluationContext Context(const_cast<UObject*>(AnimContext));
	UpdateMotionMatchingState(&Context,
		PoseHistory,
		Databases,
		DeltaTime,
		PoseJumpThresholdTime,
		PoseReselectHistory,
		SearchThrottleTime,
		PlayRate,
		InOutMotionMatchingState,
		InterruptMode,
		bShouldUseCachedChannelData,
		bDebugDrawQuery, bDebugDrawCurResult);
}

void UPoseSearchLibrary::UpdateMotionMatchingState(
	FChooserEvaluationContext* AnimContext,
	const UE::PoseSearch::IPoseHistory* PoseHistory,
	const TConstArrayView<TObjectPtr<const UPoseSearchDatabase>> Databases,
	float DeltaTime,
	const FFloatInterval& PoseJumpThresholdTime,
	float PoseReselectHistory,
	float SearchThrottleTime,
	const FFloatInterval& PlayRate,
	FMotionMatchingState& InOutMotionMatchingState,
	EPoseSearchInterruptMode InterruptMode,
	bool bShouldUseCachedChannelData,
	bool bDebugDrawQuery,
	bool bDebugDrawCurResult,
	const FPoseSearchEvent& EventToSearch)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_Update);

	using namespace UE::PoseSearch;

	FMemMark Mark(FMemStack::Get());

	UE::PoseSearch::FSearchResult InternalSearchResult;

	const UPoseSearchDatabase* CurrentResultDatabase = InOutMotionMatchingState.SearchResult.SelectedDatabase.Get();
	if (IsInvalidatingContinuingPose(InterruptMode, CurrentResultDatabase, Databases))
	{
		InOutMotionMatchingState.SearchResult = FPoseSearchBlueprintResult();
	}
	else
	{
		InternalSearchResult.InitFrom(InOutMotionMatchingState.SearchResult);

#if DO_CHECK
		if (InternalSearchResult.PoseIdx != INDEX_NONE)
		{
			FPoseSearchBlueprintResult TestSearchResult;
			TestSearchResult.InitFrom(InternalSearchResult, InOutMotionMatchingState.SearchResult.WantedPlayRate);

			if (InOutMotionMatchingState.SearchResult.SelectedAnim != TestSearchResult.SelectedAnim ||
				InOutMotionMatchingState.SearchResult.SelectedTime != TestSearchResult.SelectedTime ||
				InOutMotionMatchingState.SearchResult.bIsContinuingPoseSearch != TestSearchResult.bIsContinuingPoseSearch ||
				InOutMotionMatchingState.SearchResult.WantedPlayRate != TestSearchResult.WantedPlayRate ||
				InOutMotionMatchingState.SearchResult.bLoop != TestSearchResult.bLoop ||
				InOutMotionMatchingState.SearchResult.bIsMirrored != TestSearchResult.bIsMirrored ||
				InOutMotionMatchingState.SearchResult.BlendParameters != TestSearchResult.BlendParameters ||
				InOutMotionMatchingState.SearchResult.SelectedDatabase != TestSearchResult.SelectedDatabase ||
				InOutMotionMatchingState.SearchResult.SearchCost != TestSearchResult.SearchCost ||
				InOutMotionMatchingState.SearchResult.bIsInteraction != TestSearchResult.bIsInteraction)
			{
				UE_LOG(LogPoseSearch, Error, TEXT("Error converting FPoseSearchBlueprintResult to UE::PoseSearch::FSearchResult!"));
			}
		}
#endif // DO_CHECK
	}

	const FPoseSearchEvent PlayRateOverriddenEvent = EventToSearch.GetPlayRateOverriddenEvent(PlayRate);
	FSearchContext SearchContext(0.f, &InOutMotionMatchingState.PoseIndicesHistory, InternalSearchResult, PoseJumpThresholdTime, PlayRateOverriddenEvent);
	SearchContext.AddRole(GetCommonDefaultRole(Databases), AnimContext, PoseHistory);

	const bool bCanAdvance = InternalSearchResult.PoseIdx != INDEX_NONE;

	// If we can't advance or enough time has elapsed since the last pose jump then search
	const bool bSearch = !bCanAdvance || (InOutMotionMatchingState.ElapsedPoseSearchTime >= SearchThrottleTime);
	if (bSearch)
	{
		InOutMotionMatchingState.ElapsedPoseSearchTime = 0.f;
		const bool bForceInterrupt = IsForceInterrupt(InterruptMode, CurrentResultDatabase, Databases);
		const bool bSearchContinuingPose = !bForceInterrupt && bCanAdvance;

		// calculating if it's worth bUseCachedChannelData (if we potentially have to build query with multiple schemas)
		SearchContext.SetUseCachedChannelData(bShouldUseCachedChannelData && ShouldUseCachedChannelData(bSearchContinuingPose ? CurrentResultDatabase : nullptr, Databases));

		FSearchResult SearchResult;
		// Evaluate continuing pose
		if (bSearchContinuingPose)
		{
			SearchResult = CurrentResultDatabase->SearchContinuingPose(SearchContext);
			SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
		}

		for (const TObjectPtr<const UPoseSearchDatabase>& Database : Databases)
		{
			if (Database)
			{
				const FSearchResult NewSearchResult = Database->Search(SearchContext);

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
				const FPoseSearchCost BestBruteForcePoseCost = NewSearchResult.BruteForcePoseCost < SearchResult.BruteForcePoseCost ? NewSearchResult.BruteForcePoseCost : SearchResult.BruteForcePoseCost;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED

				if (NewSearchResult.PoseCost < SearchResult.PoseCost)
				{
					SearchResult = NewSearchResult;
					SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
				}

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
				SearchResult.BruteForcePoseCost = BestBruteForcePoseCost;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
			}
		}

#if WITH_EDITOR
		// resetting CurrentSearchResult if any DDC indexing on the requested databases is still in progress
		if (SearchContext.IsAsyncBuildIndexInProgress())
		{
			InternalSearchResult.Reset();
		}
#endif // WITH_EDITOR

#if !NO_LOGGING
		if (!SearchResult.IsValid())
		{
			TStringBuilder<1024> StringBuilder;
			StringBuilder << "UPoseSearchLibrary::UpdateMotionMatchingState invalid search result : ForceInterrupt [";
			StringBuilder << bForceInterrupt;
			StringBuilder << "], CanAdvance [";
			StringBuilder << bCanAdvance;
			StringBuilder << "], Indexing [";

			bool bIsIndexing = false;
#if WITH_EDITOR
			bIsIndexing = SearchContext.IsAsyncBuildIndexInProgress();
#endif // WITH_EDITOR
			StringBuilder << bIsIndexing;

			StringBuilder << "], Databases [";

			for (int32 DatabaseIndex = 0; DatabaseIndex < Databases.Num(); ++DatabaseIndex)
			{
				StringBuilder << GetNameSafe(Databases[DatabaseIndex]);
				if (DatabaseIndex != Databases.Num() - 1)
				{
					StringBuilder << ", ";
				}
			}

			StringBuilder << "] ";

			FString String = StringBuilder.ToString();

			if (bIsIndexing)
			{
				UE_LOG(LogPoseSearch, Log, TEXT("%s"), *String);
			}
			else
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("%s"), *String);
			}
		}
#endif // !NO_LOGGING

		// Remember which pose and sequence we're playing from the database
		InternalSearchResult = SearchResult;
	}
	else
	{
		// @todo: for blendspaces ElapsedPoseSearchTime should be incremented by a normalized DeltaTime, also didn't we already synchronized the result???
		InOutMotionMatchingState.ElapsedPoseSearchTime += DeltaTime;
		InternalSearchResult.bIsContinuingPoseSearch = true;
	}

	// @todo: consider moving this into if (bSearch) to avoid calling SearchContext.GetCachedQuery if no search is required
	const float WantedPlayRate = CalculateWantedPlayRate(InternalSearchResult, SearchContext, PlayRate, PoseHistory ? PoseHistory->GetTrajectorySpeedMultiplier() : 1.f, EventToSearch);

	InOutMotionMatchingState.PoseIndicesHistory.Update(InternalSearchResult, DeltaTime, PoseReselectHistory);
	InOutMotionMatchingState.SearchResult.InitFrom(InternalSearchResult, WantedPlayRate);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InOutMotionMatchingState.WantedPlayRate = WantedPlayRate;
	InOutMotionMatchingState.CurrentSearchResult = InternalSearchResult;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if UE_POSE_SEARCH_TRACE_ENABLED
	TraceMotionMatching(SearchContext, InternalSearchResult, InOutMotionMatchingState.ElapsedPoseSearchTime, DeltaTime, bSearch, InOutMotionMatchingState.SearchResult.WantedPlayRate, InterruptMode);
#endif // UE_POSE_SEARCH_TRACE_ENABLED

#if WITH_EDITORONLY_DATA && ENABLE_ANIM_DEBUG
	const FSearchResult& CurResult = InternalSearchResult;
	if (bDebugDrawQuery || bDebugDrawCurResult)
	{
		const UPoseSearchDatabase* CurResultDatabase = CurResult.Database.Get();

#if WITH_EDITOR
		if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(CurResultDatabase, ERequestAsyncBuildFlag::ContinueRequest))
#endif // WITH_EDITOR
		{
			FDebugDrawParams DrawParams(SearchContext.GetContexts(), SearchContext.GetPoseHistories(), SearchContext.GetRoleToIndex(), CurResultDatabase);
			if (bDebugDrawCurResult)
			{
				DrawParams.DrawFeatureVector(CurResult.PoseIdx);
			}

			if (bDebugDrawQuery)
			{
				DrawParams.DrawFeatureVector(SearchContext.GetOrBuildQuery(CurResultDatabase->Schema));
			}
		}
	}
#endif
}

void UPoseSearchLibrary::IsAnimationAssetLooping(const UObject* Asset, bool& bIsAssetLooping)
{
	if (const UAnimSequenceBase* SequenceBase = Cast<const UAnimSequenceBase>(Asset))
	{
		bIsAssetLooping = SequenceBase->bLoop;
	}
	else if (const UBlendSpace* BlendSpace = Cast<const UBlendSpace>(Asset))
	{
		bIsAssetLooping = BlendSpace->bLoop;
	}
	else if (const UMultiAnimAsset* MultiAnimAsset = Cast<const UMultiAnimAsset>(Asset))
	{
		bIsAssetLooping = MultiAnimAsset->IsLooping();
	}
	else
	{
		bIsAssetLooping = false;
	}
}

void UPoseSearchLibrary::GetDatabaseTags(const UPoseSearchDatabase* Database, TArray<FName>& Tags)
{
	if (Database)
	{
		Tags = Database->Tags;
	}
	else
	{
		Tags.Reset();
	}
}

void UPoseSearchLibrary::MotionMatch(
	UAnimInstance* AnimInstance,
	TArray<UObject*> AssetsToSearch,
	const FName PoseHistoryName,
	const FPoseSearchContinuingProperties ContinuingProperties,
	const FPoseSearchFutureProperties Future,
	FPoseSearchBlueprintResult& Result)
{
	using namespace UE::PoseSearch;

	FMemMark Mark(FMemStack::Get());

	TArray<UAnimInstance*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> AnimInstances;
	AnimInstances.Add(AnimInstance);

	TArray<FName, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> Roles;
	Roles.Add(DefaultRole);

	TArray<const UObject*>& AssetsToSearchConst = reinterpret_cast<TArray<const UObject*>&>(AssetsToSearch);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	MotionMatch(AnimInstances, Roles, AssetsToSearchConst, PoseHistoryName, ContinuingProperties, Future, Result);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UPoseSearchLibrary::MotionMatch(
	const TArrayView<UAnimInstance*> AnimInstances,
	const TArrayView<const UE::PoseSearch::FRole> Roles,
	const TArrayView<const UObject*> AssetsToSearch,
	const FName PoseHistoryName,
	const FPoseSearchContinuingProperties& ContinuingProperties,
	const FPoseSearchFutureProperties& Future,
	FPoseSearchBlueprintResult& Result)
{
	using namespace UE::Anim;
	using namespace UE::PoseSearch;

	Result = FPoseSearchBlueprintResult();

	if (AnimInstances.IsEmpty() || AnimInstances.Num() != Roles.Num())
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchLibrary::MotionMatch - invalid input AnimInstances or Roles"));
		return;
	}
	
	for (UAnimInstance* AnimInstance : AnimInstances)
	{
		if (!AnimInstance)
		{
			UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchLibrary::MotionMatch - null AnimInstances"));
			return;
		}

		if (!AnimInstance->CurrentSkeleton)
		{
			UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchLibrary::MotionMatch - null AnimInstances->CurrentSkeleton"));
			return;
		}
	}

	FMemMark Mark(FMemStack::Get());

	TArray<const IPoseHistory*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> PoseHistories;
	TArray<const UObject*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> AnimContexts;
	for (UAnimInstance* AnimInstance : AnimInstances)
	{
		if (const FAnimNode_PoseSearchHistoryCollector_Base* PoseHistoryNode = FindPoseHistoryNode(PoseHistoryName, AnimInstance))
		{
			PoseHistories.Add(&PoseHistoryNode->GetPoseHistory());
		}

		AnimContexts.Add(AnimInstance);
	}

	if (PoseHistories.Num() != AnimInstances.Num())
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchLibrary::MotionMatch - Couldn't find pose history with name '%s'"), *PoseHistoryName.ToString());
		return;
	}

	const FSearchResult SearchResult = MotionMatch(AnimContexts, Roles, PoseHistories, AssetsToSearch, ContinuingProperties, Future, FPoseSearchEvent());
	if (SearchResult.IsValid())
	{
		const UPoseSearchDatabase* Database = SearchResult.Database.Get();
		check(Database);
		
		// figuring out the WantedPlayRate
		float WantedPlayRate = 1.f;
		if (Future.Animation && Future.IntervalTime > 0.f)
		{
			if (const UPoseSearchFeatureChannel_PermutationTime* PermutationTimeChannel = Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_PermutationTime>())
			{
				const FSearchIndex& SearchIndex = Database->GetSearchIndex();
				if (!SearchIndex.IsValuesEmpty())
				{
					TConstArrayView<float> ResultData = Database->GetSearchIndex().GetPoseValues(SearchResult.PoseIdx);
					const float ActualIntervalTime = PermutationTimeChannel->GetPermutationTime(ResultData);
					WantedPlayRate = ActualIntervalTime / Future.IntervalTime;
				}
			}
		}

		Result.InitFrom(SearchResult, WantedPlayRate);
	}
}

UE::PoseSearch::FSearchResult UPoseSearchLibrary::MotionMatch(
	const TArrayView<const UObject*> AnimContexts,
	const TArrayView<const UE::PoseSearch::FRole> Roles,
	const TArrayView<const UE::PoseSearch::IPoseHistory*> PoseHistories, 
	const TArrayView<const UObject*> AssetsToSearch,
	const FPoseSearchContinuingProperties& ContinuingProperties,
	const FPoseSearchFutureProperties& Future,
	const FPoseSearchEvent& EventToSearch)
{
	TArray<FChooserEvaluationContext, TInlineAllocator<UE::PoseSearch::PreallocatedRolesNum>> Contexts;
	const int NumContexts = AnimContexts.Num(); 
	Contexts.SetNum(NumContexts);
	for(int i = 0; i < NumContexts; i++)
	{
		Contexts[i].AddObjectParam(const_cast<UObject*>(AnimContexts[i]));
	}

	return MotionMatch(Contexts, Roles, PoseHistories, AssetsToSearch, ContinuingProperties, Future, EventToSearch);
}

UE::PoseSearch::FSearchResult UPoseSearchLibrary::MotionMatch(
	const TArrayView<const UObject*> AnimContexts,
	const TArrayView<const UE::PoseSearch::FRole> Roles,
	const TArrayView<const UE::PoseSearch::IPoseHistory*> PoseHistories, 
	const TArrayView<const UObject*> AssetsToSearch,
	const FPoseSearchContinuingProperties& ContinuingProperties,
	float DesiredPermutationTimeOffset,
	const FPoseSearchEvent& EventToSearch)
{
	TArray<FChooserEvaluationContext, TInlineAllocator<UE::PoseSearch::PreallocatedRolesNum>> Contexts;
	const int NumContexts = AnimContexts.Num(); 
	Contexts.SetNum(NumContexts);
	for(int i = 0; i < NumContexts; i++)
	{
		Contexts[i].AddObjectParam(const_cast<UObject*>(AnimContexts[i]));
	}

	return MotionMatch(Contexts, Roles, PoseHistories, AssetsToSearch, ContinuingProperties, DesiredPermutationTimeOffset, EventToSearch);
}

UE::PoseSearch::FSearchResult UPoseSearchLibrary::MotionMatch(
	const TArrayView<FChooserEvaluationContext> Contexts,
	const TArrayView<const UE::PoseSearch::FRole> Roles,
	const TArrayView<const UE::PoseSearch::IPoseHistory*> PoseHistories, 
	const TArrayView<const UObject*> AssetsToSearch,
	const FPoseSearchContinuingProperties& ContinuingProperties,
	const FPoseSearchFutureProperties& Future,
	const FPoseSearchEvent& EventToSearch)
{
	check(!Contexts.IsEmpty() && Contexts.Num() == Roles.Num() && Contexts.Num() == PoseHistories.Num());

	using namespace UE::PoseSearch;

	FMemMark Mark(FMemStack::Get());

	TArray<const IPoseHistory*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> InternalPoseHistories;
	InternalPoseHistories = PoseHistories;

	// MemStackPoseHistories will hold future poses to match AssetSamplerBase (at FutureAnimationStartTime) TimeToFutureAnimationStart seconds in the future
	TArray<FMemStackPoseHistory, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> MemStackPoseHistories;
	float FutureIntervalTime = Future.IntervalTime;
	if (Future.Animation)
	{
		MemStackPoseHistories.SetNum(InternalPoseHistories.Num());

		float FutureAnimationTime = Future.AnimationTime;
		if (FutureAnimationTime < FiniteDelta)
		{
			UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchLibrary::MotionMatch - provided Future.AnimationTime (%f) is too small to be able to calculate velocities. Clamping it to minimum value of %f"), FutureAnimationTime, FiniteDelta);
			FutureAnimationTime = FiniteDelta;
		}

		const float MinFutureIntervalTime = FiniteDelta + UE_KINDA_SMALL_NUMBER;
		if (FutureIntervalTime < MinFutureIntervalTime)
		{
			UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchLibrary::MotionMatch - provided TimeToFutureAnimationStart (%f) is too small. Clamping it to minimum value of %f"), FutureIntervalTime, MinFutureIntervalTime);
			FutureIntervalTime = MinFutureIntervalTime;
		}

		for (int32 RoleIndex = 0; RoleIndex < Roles.Num(); ++RoleIndex)
		{
			if (const IPoseHistory* PoseHistory = InternalPoseHistories[RoleIndex])
			{
				if (const USkeleton* Skeleton = GetContextSkeleton(Contexts[RoleIndex]))
				{
					// @todo: add input BlendParameters to support sampling FutureAnimation blendspaces and support for multi character
					const UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(Future.Animation);
					if (!AnimationAsset)
					{
						if (const UMultiAnimAsset* MultiAnimAsset = Cast<UMultiAnimAsset>(Future.Animation))
						{
							AnimationAsset = MultiAnimAsset->GetAnimationAsset(Roles[RoleIndex]);
						}
						else
						{
							checkNoEntry();
						}
					}

					MemStackPoseHistories[RoleIndex].Init(InternalPoseHistories[RoleIndex]);
					MemStackPoseHistories[RoleIndex].ExtractAndAddFuturePoses(AnimationAsset, FutureAnimationTime, FiniteDelta, FVector::ZeroVector, FutureIntervalTime, Skeleton);
					InternalPoseHistories[RoleIndex] = MemStackPoseHistories[RoleIndex].GetThisOrPoseHistory();
				}
			}
		}
	}		

	return MotionMatch(Contexts, Roles, InternalPoseHistories, AssetsToSearch, ContinuingProperties, FutureIntervalTime, EventToSearch);
}

UE::PoseSearch::FSearchResult UPoseSearchLibrary::MotionMatch(
	const TArrayView<FChooserEvaluationContext> Contexts,
	const TArrayView<const UE::PoseSearch::FRole> Roles,
	const TArrayView<const UE::PoseSearch::IPoseHistory*> PoseHistories, 
	const TArrayView<const UObject*> AssetsToSearch,
	const FPoseSearchContinuingProperties& ContinuingProperties,
	const float DesiredPermutationTimeOffset,
	const FPoseSearchEvent& EventToSearch)
{
	using namespace UE::PoseSearch;

	FSearchResult SearchResult;

	FReconstructedPreviousSearchBestResultMap ReconstructedPreviousSearchBestResultMap;
	FSearchResult ReconstructedPreviousSearchResult;
	FSearchContext SearchContext(DesiredPermutationTimeOffset, nullptr, ReconstructedPreviousSearchResult, FFloatInterval(0.f, 0.f), EventToSearch);
	SearchContext.SetIsContinuingInteraction(ContinuingProperties.bIsContinuingInteraction);

	for (int32 RoleIndex = 0; RoleIndex < Roles.Num(); ++RoleIndex)
	{
		SearchContext.AddRole(Roles[RoleIndex], &Contexts[RoleIndex], PoseHistories[RoleIndex]);
	}
	
	// collecting all the possible continuing pose search (it could be multiple searches, but most likely only one)
	float DeltaSeconds = FiniteDelta;
	if (Contexts[0].ObjectParams.Num() > 0)
	{
		if (const UAnimInstance* AnimInstance = Cast<UAnimInstance>(Contexts[0].GetFirstObjectParam()))
		{
			DeltaSeconds = AnimInstance->GetDeltaSeconds();
		}
	}

	// collecting all the databases searches in AssetsToSearchPerDatabaseMap
	// and all the continuing pose searches in ContinuingPoseAssetsToSearchPerDatabaseMap
	FAssetsToSearchPerDatabaseMap AssetsToSearchPerDatabaseMap;
	FAssetsToSearchPerDatabaseMap ContinuingPoseAssetsToSearchPerDatabaseMap;
	PopulateSearches(AssetsToSearch, SearchContext, AssetsToSearchPerDatabaseMap);
	PopulateContinuingPoseSearches(ContinuingProperties.PlayingAsset.Get(), AssetsToSearch, SearchContext, ContinuingPoseAssetsToSearchPerDatabaseMap);

	for (const TAssetsToSearchPerDatabasePair& AssetsToSearchPerDatabasePair : ContinuingPoseAssetsToSearchPerDatabaseMap)
	{
		const UPoseSearchDatabase* Database = AssetsToSearchPerDatabasePair.Key;
		check(Database);

		const bool bInvalidatingContinuingPose = IsInvalidatingContinuingPose(ContinuingProperties.InterruptMode, Database, AssetsToSearchPerDatabaseMap);
		if (!bInvalidatingContinuingPose)
		{
			ReconstructedPreviousSearchResult.AssetTime = ContinuingProperties.PlayingAssetAccumulatedTime;
			ReconstructedPreviousSearchResult.PoseIdx = Database->GetPoseIndex(ContinuingProperties.PlayingAsset.Get(), ContinuingProperties.PlayingAssetAccumulatedTime, ContinuingProperties.bIsPlayingAssetMirrored, ContinuingProperties.PlayingAssetBlendParameters);
			ReconstructedPreviousSearchResult.Database = Database;

			const bool bForceInterrupt = IsForceInterrupt(ContinuingProperties.InterruptMode, Database, AssetsToSearchPerDatabaseMap);
			const bool bCanAdvance = ReconstructedPreviousSearchResult.PoseIdx != INDEX_NONE;
			if (bCanAdvance && !bForceInterrupt)
			{
				SearchContext.UpdateCurrentResultPoseVector();
				const FSearchResult NewSearchResult = Database->SearchContinuingPose(SearchContext);

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
				const FPoseSearchCost BestBruteForcePoseCost = NewSearchResult.BruteForcePoseCost < SearchResult.BruteForcePoseCost ? NewSearchResult.BruteForcePoseCost : SearchResult.BruteForcePoseCost;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED

				if (NewSearchResult.PoseCost < SearchResult.PoseCost)
				{
					SearchResult = NewSearchResult;
					SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
				}

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
				SearchResult.BruteForcePoseCost = BestBruteForcePoseCost;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED

				if (NewSearchResult.IsValid())
				{
					// keeping track of the best ReconstructedPreviousSearchResult, as best continuing pose search result for the full Database search happening after
					if (FSearchResult* FoundReconstructedPreviousSearchBestResult = ReconstructedPreviousSearchBestResultMap.Find(Database))
					{
						if (NewSearchResult.PoseCost == FoundReconstructedPreviousSearchBestResult->PoseCost)
						{
							check(NewSearchResult.PoseIdx != FoundReconstructedPreviousSearchBestResult->PoseIdx);
							if (NewSearchResult.PoseIdx < FoundReconstructedPreviousSearchBestResult->PoseIdx)
							{
								// choosing to update with the lowest PoseIdx to avoid indeterminism 
								// since ReconstructedPreviousSearchBestResultMap is an unsorted Map
								*FoundReconstructedPreviousSearchBestResult = ReconstructedPreviousSearchResult;
								FoundReconstructedPreviousSearchBestResult->PoseCost = NewSearchResult.PoseCost;
							}
						}
						else if (NewSearchResult.PoseCost < FoundReconstructedPreviousSearchBestResult->PoseCost)
						{
							*FoundReconstructedPreviousSearchBestResult = ReconstructedPreviousSearchResult;
							FoundReconstructedPreviousSearchBestResult->PoseCost = NewSearchResult.PoseCost;
						}
					}
					else
					{
						FSearchResult& NewReconstructedPreviousSearchBestResult = ReconstructedPreviousSearchBestResultMap.Add(Database);
						NewReconstructedPreviousSearchBestResult = ReconstructedPreviousSearchResult;
						NewReconstructedPreviousSearchBestResult.PoseCost = NewSearchResult.PoseCost;
					}
				}
			}
		}
	}

	// performing all the other databases searches
	for (const TAssetsToSearchPerDatabasePair& AssetsToSearchPerDatabasePair : AssetsToSearchPerDatabaseMap)
	{
		const UPoseSearchDatabase* Database = AssetsToSearchPerDatabasePair.Key;
		check(Database);

		// setting up the best continuing pose environment from the continuing pose searches we already performed
		if (FSearchResult* ReconstructedPreviousSearchBestResult = ReconstructedPreviousSearchBestResultMap.Find(Database))
		{
			ReconstructedPreviousSearchResult = *ReconstructedPreviousSearchBestResult;
		}
		else
		{
			ReconstructedPreviousSearchResult.Reset();
		}

		SearchContext.SetAssetsToConsider(AssetsToSearchPerDatabasePair.Value);

		// in case we haven't searched the continuing pose for this Database, we haven't created and cached the query yet,
		// but if we didn't invalidated the continuing pose (when IsInvalidatingContinuingPose is true), we still can reuse
		// the updated ReconstructedPreviousSearchResult data, and by calling UpdateCurrentResultPoseVector we set the 
		// SearchContext to be able to create a query for Database using the continuing pose data.
		SearchContext.UpdateCurrentResultPoseVector();
		const FSearchResult NewSearchResult = Database->Search(SearchContext);

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
		const FPoseSearchCost BestBruteForcePoseCost = NewSearchResult.BruteForcePoseCost < SearchResult.BruteForcePoseCost ? NewSearchResult.BruteForcePoseCost : SearchResult.BruteForcePoseCost;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED

		if (NewSearchResult.PoseCost < SearchResult.PoseCost)
		{
			SearchResult = NewSearchResult;
			SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
		}

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
		SearchResult.BruteForcePoseCost = BestBruteForcePoseCost;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
	}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	if (SearchResult.IsValid())
	{
		const bool bDrawMatch = GVarAnimMotionMatchDrawMatchEnable;
		const bool bDrawquery = GVarAnimMotionMatchDrawQueryEnable;

		if (bDrawMatch || bDrawquery)
		{
			FDebugDrawParams DrawParams(SearchContext.GetContexts(), SearchContext.GetPoseHistories(), SearchContext.GetRoleToIndex(), SearchResult.Database.Get());
			if (bDrawMatch)
			{
				DrawParams.DrawFeatureVector(SearchResult.PoseIdx);
			}

			if (bDrawquery)
			{
				DrawParams.DrawFeatureVector(SearchContext.GetOrBuildQuery(SearchResult.Database->Schema));
			}
		}
	}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

#if UE_POSE_SEARCH_TRACE_ENABLED
	// @todo: add and handle an interrupt mode input param MotionMatch
	TraceMotionMatching(SearchContext, SearchResult, 0.f, DeltaSeconds, true, 1.f, EPoseSearchInterruptMode::DoNotInterrupt);
#endif // UE_POSE_SEARCH_TRACE_ENABLED

	return SearchResult;
}

const FAnimNode_PoseSearchHistoryCollector_Base* UPoseSearchLibrary::FindPoseHistoryNode(
	const FName PoseHistoryName,
	const UAnimInstance* AnimInstance)
{
	if (AnimInstance)
	{
		TSet<const UAnimInstance*, DefaultKeyFuncs<const UAnimInstance*>, TInlineSetAllocator<128>> AlreadyVisited;
		TArray<const UAnimInstance*, TInlineAllocator<128>> ToVisit;

		ToVisit.Add(AnimInstance);
		AlreadyVisited.Add(AnimInstance);

		while (!ToVisit.IsEmpty())
		{
			const UAnimInstance* Visiting = ToVisit.Pop();

			if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(Visiting->GetClass()))
			{
				if (const FAnimSubsystem_Tag* TagSubsystem = AnimBlueprintClass->FindSubsystem<FAnimSubsystem_Tag>())
				{
					if (const FAnimNode_PoseSearchHistoryCollector_Base* HistoryCollector = TagSubsystem->FindNodeByTag<FAnimNode_PoseSearchHistoryCollector_Base>(PoseHistoryName, Visiting))
					{
						return HistoryCollector;
					}
				}
			}

			const USkeletalMeshComponent* SkeletalMeshComponent = Visiting->GetSkelMeshComponent();
			const TArray<UAnimInstance*>& LinkedAnimInstances = SkeletalMeshComponent->GetLinkedAnimInstances();
			for (const UAnimInstance* LinkedAnimInstance : LinkedAnimInstances)
			{
				bool bIsAlreadyInSet = false;
				AlreadyVisited.Add(LinkedAnimInstance, &bIsAlreadyInSet);
				if (!bIsAlreadyInSet)
				{
					ToVisit.Add(LinkedAnimInstance);
				}
			}
		}
	}
	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Begin deprecated signatures
UE::PoseSearch::FSearchResult UPoseSearchLibrary::MotionMatch(
	const TArrayView<UAnimInstance*> AnimInstances,
	const TArrayView<const UE::PoseSearch::FRole> Roles,
	const TArrayView<const UE::PoseSearch::IPoseHistory*> PoseHistories,
	const TArrayView<const UObject*> AssetsToSearch,
	const FPoseSearchContinuingProperties& ContinuingProperties,
	const FPoseSearchFutureProperties& Future)
{
	TArray<const UObject*, TInlineAllocator<UE::PoseSearch::PreallocatedRolesNum>> AnimContexts;
	AnimContexts.Reserve(AnimInstances.Num());

	for (UAnimInstance* AnimInstance : AnimInstances)
	{
		AnimContexts.Add(AnimInstance);
	}

	return MotionMatch(AnimContexts, Roles, PoseHistories, AssetsToSearch, ContinuingProperties, Future, FPoseSearchEvent());
}

// End deprecated signatures
///////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
