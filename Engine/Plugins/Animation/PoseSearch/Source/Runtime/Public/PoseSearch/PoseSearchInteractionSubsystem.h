// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchInteractionLibrary.h"
#include "PoseSearch/PoseSearchInteractionValidator.h"
#include "Subsystems/WorldSubsystem.h"
#include "PoseSearchInteractionSubsystem.generated.h"

#define UE_API POSESEARCH_API

struct FPoseSearchContinuingProperties;
class UAnimInstance;
class UPoseSearchDatabase;

namespace UE::PoseSearch
{
struct FAnimContextInfos;
struct FTagToDatabases;
} // namespace UE::PoseSearch

// World subsystem accepting the publication of characters (via their AnimInstance(s)) FPoseSearchInteractionAvailability, representing the characters willingness to partecipate in an 
// interaction with other characters from the next frame forward via Query_AnyThread method.
//
// The same method will return the FPoseSearchBlueprintResult from the PREVIOUS Tick processing (categorization of FPoseSearchInteractionAvailability(s) in multiple FInteractionIsland(s)),
// to the requesting character, containing the animation to play at what time, and the assigned role to partecipate in the selected interaction within the assigned 
// FInteractionIsland

// Execution model and threading details:
/////////////////////////////////////////
// 
// - by calling UPoseSearchInteractionLibrary::MotionMatchInteraction_Pure(TArray<FPoseSearchInteractionAvailability> Availabilities, UObject* AnimInstance), characters publish their availabilities
//   to partecipate in interactions to the UPoseSearchInteractionSubsystem
// - UPoseSearchInteractionSubsystem::Tick processes those FPoseSearchInteractionAvailability(s) and creates/updates UE::PoseSearch::FInteractionIsland. For each FInteractionIsland it injects 
//   a tick prerequisite via FInteractionIsland::InjectToActor (that calls AddPrerequisite) to all the Actors in the same island.
//   NoTe: the next frame the execution will be:
//			for each island[k]
//			{
//				for each Actor[k][i]
//				{
//					Tick all the TickActorComponents prerequisites, such as CharacterMovementComponent[k][i] (or Mover) in parallel
//				}
// 
//				Tick Island[k].PreTickFunction (that eventually generates the trajectories with all the updated CMCs or Mover)
//
//				Tick Actor[k][0].SkeletalMeshComponent (or AnimNextComponent, that performs the MotionMatchInteraction queries for all the involved actors via DoSearch_AnyThread)
// 
//				Tick Island[k].PostTickFunction (currently just a threading fence for the execution of all the other SkeletalMeshComponent(s) or AnimNextComponent(s))
// 
//				for each Actor[k][i]
//				{
//					if (i != 0)
//						Tick SkeletalMeshComponent[k][i] (or AnimNextComponent(s) that DoSearch_AnyThread get the cached result calculated by Tick Actor[k][0].SkeletalMeshComponent) in parallel
//				}
//			}
// - next frame UPoseSearchInteractionLibrary::MotionMatchInteraction_Pure(TArray<FPoseSearchInteractionAvailability> Availabilities, UObject* AnimInstance), with the context of all the published 
//   availabilities and created islands, will find the associated FInteractionIsland to the AnimInstance and call FInteractionIsland::DoSearch_AnyThread 
//   (via UPoseSearchInteractionSubsystem::Query_AnyThread) that will perform ALL (YES, ALL, so the bigger the island the slower the execution) the motion matching searches
//   for all the possible Actors / databases / Roles combinations, and populate FInteractionIsland::SearchResults with ALL the results for the island.
//   Ultimately the MotionMatchInteraction_Pure will return the SearchResults associated to the requesting AnimInstance with information about what animation to play
//   at what time with wich Role.

UCLASS(MinimalAPI, Experimental, Category="Animation|Pose Search")
class UPoseSearchInteractionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UE_API UPoseSearchInteractionSubsystem* GetSubsystem_AnyThread(const UObject* AnimInstance);

	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;

	// it processes FPoseSearchInteractionAvailability(s) and creates/updates FInteractionIsland
	UE_API virtual void Tick(float DeltaSeconds) override;
	
	UE_API virtual TStatId GetStatId() const override;

	// publishing FPoseSearchInteractionAvailability(s) for the requesting character (via AnimContext as UAnimInstance or UAnimNextComponent) 
	// and getting the FPoseSearchBlueprintResult from the PREVIOUS Tick update
	// containing the animation to play at what time, and the assigned role to partecipate in the selected interaction.
	// Either a PoseHistoryName or a PoseHistory are required to perform the associated motion matching searches
	// if bValidateResultAgainstAvailabilities is true, the result will be validated against the currently published availabilities,
	// that could differ from the previous frame one for this AnimContext, and discarded if not valid within the context of the new availabilities
	UE_API void Query_AnyThread(const TArrayView<const FPoseSearchInteractionAvailability> Availabilities, const UObject* AnimContext, 
		FPoseSearchBlueprintResult& Result,	FName PoseHistoryName, const UE::PoseSearch::IPoseHistory* PoseHistory, bool bValidateResultAgainstAvailabilities);

	UE_API void GetResult_AnyThread(const UObject* AnimContext, FPoseSearchBlueprintResult& Result, bool bCompareOwningActors = false);

	TConstArrayView<UE::PoseSearch::FInteractionIsland*> GetInteractionIslands() const { return Islands; }

private:
#if ENABLE_ANIM_DEBUG
	friend struct UE::PoseSearch::FInteractionValidator;
#endif // ENABLE_ANIM_DEBUG

	UE_API UE::PoseSearch::FInteractionIsland& CreateIsland();
	UE_API UE::PoseSearch::FInteractionIsland& GetAvailableIsland();

	UE_API void DestroyIsland(int32 Index);
	UE_API void DestroyAllIslands();
	UE_API void RegenerateAllIslands(float DeltaSeconds);

#if DO_CHECK
	UE_API bool ValidateAllIslands() const;
#endif // DO_CHECK

	UE_API void PopulateContinuingProperties(float DeltaSeconds, TArrayView<UE::PoseSearch::FInteractionSearchContext> SearchContexts) const;
	UE_API UE::PoseSearch::FInteractionIsland* FindIsland(const UObject* AnimContext, bool bCompareOwningActors = false);
	
#if ENABLE_ANIM_DEBUG
	UE_API void DebugDrawIslands() const;
	UE_API void DebugLogTickDependencies() const;
#endif // ENABLE_ANIM_DEBUG

	UE_API void AddAvailabilities(const TArrayView<const FPoseSearchInteractionAvailability> Availabilities, const UObject* AnimContext, FName PoseHistoryName, const UE::PoseSearch::IPoseHistory* PoseHistory);
	UE_API void GenerateAnimContextInfosAndTagToDatabases(UE::PoseSearch::FAnimContextInfos& AnimContextInfos, UE::PoseSearch::FTagToDatabases& TagToDatabases) const;
	UE_API void GenerateSearchContexts(float DeltaSeconds, UE::PoseSearch::FInteractionSearchContexts& SearchContexts) const;

	UE_API void UpdateValidInteractionSearches();
	static UE_API void OnInteractionStart(UE::PoseSearch::FValidInteractionSearch& ValidInteractionSearch);
	static UE_API void OnInteractionContinuing(UE::PoseSearch::FValidInteractionSearch& ValidInteractionSearch);
	static UE_API void OnInteractionEnd(UE::PoseSearch::FValidInteractionSearch& ValidInteractionSearch);

	// @todo: use a thread local storage data structure to make publishing availabilities lockless to avoid the need of AnimContextsAvailabilitiesMutex
	// it could contain duplicated availabilities
	UPROPERTY(Transient)
	TArray<FPoseSearchInteractionAnimContextAvailabilities> AnimContextsAvailabilities;

	FCriticalSection AnimContextsAvailabilitiesMutex;

	// array of groups of characters that needs to be anaylzed together for possible interactions
	TArray<UE::PoseSearch::FInteractionIsland*> Islands;

	TArray<UE::PoseSearch::FValidInteractionSearch> ValidInteractionSearches;
};

#undef UE_API
