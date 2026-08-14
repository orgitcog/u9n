// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchInteractionAvailability.h"
#include "PoseSearch/PoseSearchInteractionValidator.h"
#include "PoseSearch/PoseSearchLibrary.h"

struct FPoseSearchBlueprintResult;
struct FPoseSearchContinuingProperties;
class UMultiAnimAsset;
class UPoseSearchInteractionSubsystem;

namespace UE::PoseSearch
{
struct FInteractionIsland;
struct IInteractionIslandDependency;

// Experimental, this feature might be removed without warning, not for production use
struct FInteractionSearchContextBase
{
private:
	TArray<TWeakObjectPtr<const UObject>, TInlineAllocator<PreallocatedRolesNum>> AnimContexts;
	TArray<TWeakPtr<const UE::PoseSearch::IPoseHistory, ESPMode::ThreadSafe>, TInlineAllocator<PreallocatedRolesNum>> PoseHistories;
	TArray<UE::PoseSearch::FRole, TInlineAllocator<PreallocatedRolesNum>> Roles;

public:
	TWeakObjectPtr<const UPoseSearchDatabase> Database;
	bool bDisableCollisions = false;

	void Add(const UObject* AnimContext, const UE::PoseSearch::IPoseHistory* PoseHistory, const UE::PoseSearch::FRole Role)
	{
		check(AnimContext && PoseHistory);
		
		// AnimContexts must be sorted to have deterministic searches across multiple frames
		// as well as for faster IsEquivalent implementation
		check(AnimContexts.IsEmpty() || GetAnimContext(AnimContexts.Num() - 1) < AnimContext);

		AnimContexts.Add(AnimContext);
		PoseHistories.Add(PoseHistory->AsWeak());
		Roles.Add(Role);
	}

	int32 Num() const { return AnimContexts.Num(); }
	const UObject* GetAnimContext(int32 Index) const { return AnimContexts[Index].Get(); }
	const UE::PoseSearch::IPoseHistory* GetPoseHistory(int32 Index) const;
	const UE::PoseSearch::FRole GetRole(int32 Index) const { return Roles[Index]; }
	const TConstArrayView<UE::PoseSearch::FRole> GetRoles() const { return Roles; }

	bool IsValid() const { return !AnimContexts.IsEmpty(); }
	bool IsEquivalent(const FInteractionSearchContextBase& Other) const;

#if ENABLE_VISUAL_LOG
	void VLogContext(const FColor& Color) const;
#endif // ENABLE_VISUAL_LOG

#if DO_CHECK
	// method that queries the HistoryCollectors. the PoseHistory will complain if accessed in a non thread safe manner!
	void TestHistoryCollectorsThreadingAccess() const;
	bool CheckForConsistency() const;
#endif // DO_CHECK
};

// Experimental, this feature might be removed without warning, not for production use
typedef TArray<TPair<TWeakObjectPtr<AActor>, TWeakObjectPtr<AActor>>> FDisabledCollisions;

// Experimental, this feature might be removed without warning, not for production use
struct FValidInteractionSearch
{
	FInteractionSearchContextBase SearchContext;
	FDisabledCollisions DisabledCollisions;
};

// @todo: make FPoseSearchContinuingProperties.PlayingAsset a TWeakObjectPtr and remove this struct!
// Experimental, this feature might be removed without warning, not for production use
struct FInteractionSearchContext : FInteractionSearchContextBase
{
	// TWeakObjectPtr version of FPoseSearchContinuingProperties to be GC friendly
	TWeakObjectPtr<const UObject> PlayingAsset = nullptr;
	float PlayingAssetAccumulatedTime = 0.f;
	bool bIsPlayingAssetMirrored = false;
	FVector PlayingAssetBlendParameters = FVector::ZeroVector;
	EPoseSearchInterruptMode InterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;
	bool bIsContinuingInteraction = false;
	TArray<int32, TInlineAllocator<PreallocatedRolesNum>> TickPriorities;

#if ENABLE_ANIM_DEBUG
	TArray<FPoseSearchInteractionAvailability, TInlineAllocator<PreallocatedRolesNum>> DebugAvailabilities;
#endif // ENABLE_ANIM_DEBUG

	FPoseSearchContinuingProperties GetContinuingProperties() const
	{
		FPoseSearchContinuingProperties ContinuingProperties;
		ContinuingProperties.PlayingAsset = PlayingAsset.Get();
		ContinuingProperties.PlayingAssetAccumulatedTime = PlayingAssetAccumulatedTime;
		ContinuingProperties.bIsPlayingAssetMirrored = bIsPlayingAssetMirrored;
		ContinuingProperties.PlayingAssetBlendParameters = PlayingAssetBlendParameters;
		ContinuingProperties.InterruptMode = InterruptMode;
		ContinuingProperties.bIsContinuingInteraction = bIsContinuingInteraction;
		return ContinuingProperties;
	}
};
// Experimental, this feature might be removed without warning, not for production use
typedef TArray<FInteractionSearchContext, TInlineAllocator<PreallocatedSearchesNum, TMemStackAllocator<>>> FInteractionSearchContexts;

// Experimental, this feature might be removed without warning, not for production use
struct FInteractionSearchResult : public FSearchResult
{
	int32 SearchIndex = INDEX_NONE;

	// cached actors root transforms for all the roles in SelectedAnimation (as UMultiAnimAsset),
	// so we don't have to query the pose history to gather it when it's not thread safe to do so
	TArray<FTransform, TInlineAllocator<PreallocatedRolesNum>> ActorRootTransforms;

	// cached actors root bone transforms for all the roles in SelectedAnimation (as UMultiAnimAsset),
	// so we don't have to query the pose history to gather it when it's not thread safe to do so
	TArray<FTransform, TInlineAllocator<PreallocatedRolesNum>> ActorRootBoneTransforms;

	bool operator==(const FInteractionSearchResult& Other) const;
};

// Experimental, this feature might be removed without warning, not for production use
// FInteractionIsland contains ticks functions injected between the interacting actors TickActorComponents (UCharacterMovementComponent, or UCharacterMoverComponent)
// and PostTickComponent (USkeletalMeshComponent, or UAnimNextComponent)
// to create a execution threading fence to be able to perform motion matching searches between the involved characters in a thread safe manner.
// Look at UPoseSearchInteractionSubsystem "Execution model and threading details" for additional information
struct FInteractionIsland
{
	UE_NONCOPYABLE(FInteractionIsland);
	
	FInteractionIsland(ULevel* Level, UPoseSearchInteractionSubsystem* Subsystem);
	~FInteractionIsland();

	bool DoSearch_AnyThread(const UObject* AnimContext, const TConstArrayView<FValidInteractionSearch> ValidInteractionSearches, FPoseSearchBlueprintResult& Result);
	bool GetResult_AnyThread(const UObject* AnimContext, FPoseSearchBlueprintResult& Result, bool bCompareOwningActors = false);

	const TArray<TWeakObjectPtr<UActorComponent>>& GetTickActorComponents() const { return TickActorComponents; }
	const TArray<TWeakObjectPtr<const UObject>>& GetIslandAnimContexts() const { return IslandAnimContexts; }
	const TArray<FInteractionSearchContext>& GetSearchContexts() const { return SearchContexts; }
	const TArray<FInteractionSearchResult>& GetSearchResults() const { return SearchResults; }

	const FInteractionSearchResult* FindSearchResult(const FInteractionSearchContext& SearchContext) const;

	bool IsInitialized() const;
	void AddSearchContext(const FInteractionSearchContext& SearchContext);
	void Uninitialize(bool bValidateTickDependencies);

	bool HasTickDependencies() const;
	void InjectToActor(const UObject* AnimContext, bool bAddTickDependencies);
	
#if ENABLE_ANIM_DEBUG
	void LogTickDependencies() const;
#endif // ENABLE_ANIM_DEBUG

private:
#if ENABLE_ANIM_DEBUG
	static void LogTickDependencies(const TConstArrayView<TWeakObjectPtr<UActorComponent>> TickActorComponents, int32 InteractionIslandIndex);
#endif // ENABLE_ANIM_DEBUG

	void AddTickDependencies(UActorComponent* TickActorComponent, bool bInIsMainActor);
	void RemoveTickDependencies(bool bValidateTickDependencies);
	static IInteractionIslandDependency* FindCustomDependency(UActorComponent* InTickComponent);

	const UObject* GetMainAnimContext() const;
	const AActor* GetMainActor() const;

	struct FPreTickFunction : public FTickFunction
	{
		virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
		virtual FString DiagnosticMessage() override { return TEXT("UE::PoseSearch::FInteractionIsland::FPreTickFunction"); }
		FInteractionIsland* Island = nullptr;
	};
	FPreTickFunction PreTickFunction;

	struct FPostTickFunction : public FTickFunction
	{
		virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
		virtual FString DiagnosticMessage() override { return TEXT("UE::PoseSearch::FInteractionIsland::FPostTickFunction"); }
		FInteractionIsland* Island = nullptr;
	};
	FPostTickFunction PostTickFunction;
	bool bHasTickDependencies = false;

	TArray<TWeakObjectPtr<UActorComponent>> TickActorComponents;
	// all the AnimContext in this island. Each SearchContexts will contain a subset of IslandAnimContexts 
	TArray<TWeakObjectPtr<const UObject>> IslandAnimContexts;

	// there's one FSearchContext for each search we need to perform (including all the possible roles combinations). Added by UPoseSearchInteractionSubsystem::Tick
	// islands don't get deallocated, so this array once warmed up will not hit the allocator and waste cycles
	TArray<FInteractionSearchContext> SearchContexts;

	// SearchResults contains only the best results, and it has not necessarly the same cardinality as SearchContexts. usually SearchResults.Num() < SearchContexts.Num()
	// islands don't get deallocated, so this array once warmed up will not hit the allocator and waste cycles
	TArray<FInteractionSearchResult> SearchResults;
	bool bSearchPerfomed = false;

	UPoseSearchInteractionSubsystem* InteractionSubsystem = nullptr;

#if ENABLE_ANIM_DEBUG
	// used to analyze thread safety
	friend struct UE::PoseSearch::FInteractionValidator;
	FThreadSafeCounter InteractionIslandThreadSafeCounter = 0;
	FThreadSafeCounter TickFunctionsThreadSafeCounter = 0;
	bool bPreTickFunctionExecuted = false;
	bool bPostTickFunctionExecuted = false;
#endif // ENABLE_ANIM_DEBUG
};

// Experimental, this feature might be removed without warning, not for production use
// Allows systems other than regular actor components to hook into interaction island dependencies 
struct IInteractionIslandDependency : public IModularFeature
{
	static inline FName FeatureName = "IInteractionIslandDependency";

	virtual ~IInteractionIslandDependency() = default;

	virtual bool CanMakeDependency(const UObject* InIslandObject, const UObject* InObject) const = 0;
	virtual const FTickFunction* FindTickFunction(UObject* InObject) const = 0;

	virtual void AddPrerequisite(UObject* InIslandObject, FTickFunction& InIslandTickFunction, UObject* InObject) const = 0;
	virtual void AddSubsequent(UObject* InIslandObject, FTickFunction& InIslandTickFunction, UObject* InObject) const = 0;
	virtual void RemovePrerequisite(UObject* InIslandObject, FTickFunction& InIslandTickFunction, UObject* InObject) const = 0;
	virtual void RemoveSubsequent(UObject* InIslandObject, FTickFunction& InIslandTickFunction, UObject* InObject) const = 0;
};

} // namespace UE::PoseSearch
