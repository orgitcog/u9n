// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchInteractionIsland.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Features/IModularFeatures.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/MultiAnimAsset.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchInteractionLibrary.h"
#include "PoseSearch/PoseSearchInteractionSubsystem.h"
#include "PoseSearch/PoseSearchInteractionValidator.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "VisualLogger/VisualLogger.h"

namespace UE::PoseSearch
{

#if ENABLE_ANIM_DEBUG
static bool GVarPoseSearchInteractionDiagnoseTickDependencies = false;
static FAutoConsoleVariableRef CVarPoseSearchInteractionDiagnoseTickDependencies(TEXT("a.PoseSearchInteraction.DiagnoseTickDependencies"), GVarPoseSearchInteractionDiagnoseTickDependencies, TEXT("Enable Pose Search Interaction Tick Dependencies Diagnostic (SLOW!)"));

// recursion safe FTickFunction logging functions
static void ShowPrerequistes(const FTickFunction& NestedTick, int32 Indent, int32 MaxIndent)
{
	if (Indent >= MaxIndent)
	{
		UE_LOG(LogPoseSearch, Log, TEXT("%s==== REACHED MAX INDENT ===="), FCString::Spc(Indent * 2));
	}
	else
	{
		for (const FTickPrerequisite& Prereq : NestedTick.GetPrerequisites())
		{
			if (Prereq.PrerequisiteTickFunction)
			{
				UE_LOG(LogPoseSearch, Log, TEXT("%s prereq %s"), FCString::Spc(Indent * 2), *Prereq.PrerequisiteTickFunction->DiagnosticMessage());
				ShowPrerequistes(*Prereq.PrerequisiteTickFunction, Indent + 1, MaxIndent);
			}
		}
	}
}

static void LogTickFunction(FTickFunction& Tick, ENamedThreads::Type CurrentThread, bool bLogPrerequisites, int32 Indent, int32 MaxIndent = 50)
{
	if (Indent >= MaxIndent)
	{
		UE_LOG(LogPoseSearch, Log, TEXT("%s==== REACHED MAX INDENT ===="), FCString::Spc(Indent * 2));
	}
	else
	{
		// scoping brackets to save some heap for the recursion
		{
			UE_LOG(LogPoseSearch, Log, TEXT("%stick %s [%1d, %1d] %6llu %2d %s"), FCString::Spc(Indent * 2), Tick.bHighPriority ? TEXT("*") : TEXT(" "), (int32)Tick.GetActualTickGroup(), (int32)Tick.GetActualEndTickGroup(), (uint64)GFrameCounter, (int32)CurrentThread, *Tick.DiagnosticMessage());
			if (bLogPrerequisites)
			{
				ShowPrerequistes(Tick, Indent, MaxIndent);
			}
		}

		// Handle nested ticks
		Tick.ForEachNestedTick([CurrentThread, bLogPrerequisites, Indent, MaxIndent](FTickFunction& NestedTick)
			{
				LogTickFunction(NestedTick, CurrentThread, bLogPrerequisites, Indent + 1, MaxIndent);
			});
	}
}

// check if there's any cycle within the prerequisites of Tick
static bool ValidateTickDependenciesCycles(FTickFunction& Tick, TSet<FTickFunction*>& VisitedTickFunctions)
{
	bool bValidatedCorrectly = true;

	bool bAlreadyInSet = false;
	VisitedTickFunctions.Add(&Tick, &bAlreadyInSet);

	if (bAlreadyInSet)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("ValidateTickDependencies: TickFunction '%s' form a cycle"), *Tick.DiagnosticMessage());
		bValidatedCorrectly = false;
	}
	else
	{
		for (const FTickPrerequisite& Prereq : Tick.GetPrerequisites())
		{
			if (Prereq.PrerequisiteTickFunction)
			{
				if (!ValidateTickDependenciesCycles(*Prereq.PrerequisiteTickFunction, VisitedTickFunctions))
				{
					VisitedTickFunctions.Remove(&Tick);
					bValidatedCorrectly = false;
					break;
				}
			}
		}

		if (bValidatedCorrectly)
		{
			Tick.ForEachNestedTick([&VisitedTickFunctions, &bValidatedCorrectly](FTickFunction& NestedTick)
				{
					if (bValidatedCorrectly)
					{
						if (!ValidateTickDependenciesCycles(NestedTick, VisitedTickFunctions))
						{
							bValidatedCorrectly = false;
						}
					}
				});
		}
	}
	
	VisitedTickFunctions.Remove(&Tick);
	return bValidatedCorrectly;
}
#endif // ENABLE_ANIM_DEBUG

static FInteractionSearchResult InitSearchResult(const FSearchResult& SearchResult, const FInteractionSearchContext& SearchContext, int32 SearchIndex)
{
	FInteractionSearchResult InteractionSearchResult;
	static_cast<FSearchResult&>(InteractionSearchResult) = SearchResult;
	InteractionSearchResult.SearchIndex = SearchIndex;

	const int32 AnimContextsNum = SearchContext.Num();
	InteractionSearchResult.ActorRootTransforms.SetNum(AnimContextsNum);
	InteractionSearchResult.ActorRootBoneTransforms.SetNum(AnimContextsNum);

	for (int32 AnimContextIndex = 0; AnimContextIndex < AnimContextsNum; ++AnimContextIndex)
	{
		const UObject* AnimContext = SearchContext.GetAnimContext(AnimContextIndex);
		const USkeleton* Skeleton = AnimContext ? GetContextSkeleton(AnimContext) : nullptr;
		const IPoseHistory* PoseHistory = SearchContext.GetPoseHistory(AnimContextIndex);
		if (Skeleton && PoseHistory)
		{
			PoseHistory->GetTransformAtTime(0.f, InteractionSearchResult.ActorRootTransforms[AnimContextIndex], Skeleton, ComponentSpaceIndexType, WorldSpaceIndexType);
			PoseHistory->GetTransformAtTime(0.f, InteractionSearchResult.ActorRootBoneTransforms[AnimContextIndex], Skeleton, RootBoneIndexType, ComponentSpaceIndexType);
		}
		else
		{
			InteractionSearchResult.ActorRootTransforms[AnimContextIndex] = FTransform::Identity;
			InteractionSearchResult.ActorRootBoneTransforms[AnimContextIndex] = FTransform::Identity;
		}
	}

	return InteractionSearchResult;
}

typedef TSet<const UObject*, DefaultKeyFuncs<const UObject*>, TInlineSetAllocator<64, TMemStackSetAllocator<>>> FVisitedAnimContexts;

static bool IsPoseSearchResultUsable(int32 SearchIndex, TConstArrayView<FSearchResult> PoseSearchResults, TConstArrayView<FInteractionSearchContext> SearchContexts, const FVisitedAnimContexts& VisitedAnimContexts)
{
	if (!PoseSearchResults[SearchIndex].IsValid())
	{
		return false;
	}

	for (int32 AnimContextIndex = 0; AnimContextIndex < SearchContexts[SearchIndex].Num(); ++AnimContextIndex)
	{
		if (const UObject* ValidSearchAnimContext = SearchContexts[SearchIndex].GetAnimContext(AnimContextIndex))
		{
			if (VisitedAnimContexts.Find(ValidSearchAnimContext))
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}

static void InitSearchResults(TArray<FInteractionSearchResult>& SearchResults, TConstArrayView<FSearchResult> PoseSearchResults, TConstArrayView<FInteractionSearchContext> SearchContexts)
{
	SearchResults.Reset();

	if (!PoseSearchResults.IsEmpty())
	{
		TArray<int32, TMemStackAllocator<>> SortedPoseSearchResults;
		SortedPoseSearchResults.SetNum(PoseSearchResults.Num());
		for (int32 Index = 0; Index < PoseSearchResults.Num(); ++Index)
		{
			SortedPoseSearchResults[Index] = Index;
		}

		SortedPoseSearchResults.StableSort([&SearchContexts, &PoseSearchResults](int32 IndexA, int32 IndexB)
			{
				const bool IsValidA = PoseSearchResults[IndexA].IsValid();
				const bool IsValidB = PoseSearchResults[IndexB].IsValid();

				if (IsValidA && !IsValidB)
				{
					return true;
				}

				if (!IsValidA && IsValidB)
				{
					return false;
				}

				if (!IsValidA && !IsValidB)
				{
					return true;
				}

				const int32 NumRolesA = SearchContexts[IndexA].Num();
				const int32 NumRolesB = SearchContexts[IndexB].Num();

				if (NumRolesA > NumRolesB)
				{
					return true;
				}

				if (NumRolesA < NumRolesB)
				{
					return false;
				}

				return PoseSearchResults[IndexA].PoseCost < PoseSearchResults[IndexB].PoseCost;
			});

		// assign from best to worst result
		FVisitedAnimContexts VisitedAnimContexts;
		for (int32 SearchIndex : SortedPoseSearchResults)
		{
			if (IsPoseSearchResultUsable(SearchIndex, PoseSearchResults, SearchContexts, VisitedAnimContexts))
			{
				SearchResults.Add(InitSearchResult(PoseSearchResults[SearchIndex], SearchContexts[SearchIndex], SearchIndex));

				for (int32 AnimContextIndex = 0; AnimContextIndex < SearchContexts[SearchIndex].Num(); ++AnimContextIndex)
				{
					const UObject* SearchAnimContext = SearchContexts[SearchIndex].GetAnimContext(AnimContextIndex);
					check(SearchAnimContext);
					VisitedAnimContexts.Add(SearchAnimContext);
				}
			}
		}
	}
}

static UActorComponent* FindComponentForTickDependencies(const UObject* AnimContext)
{
	check(AnimContext);
	
	if (const UAnimInstance* AnimInstance = Cast<UAnimInstance>(AnimContext))
	{
		return AnimInstance->GetSkelMeshComponent();
	}
	
	// this is the AnimNext case
	return const_cast<UActorComponent*>(Cast<UActorComponent>(AnimContext));
}

static void AddPrerequisite(FTickFunction& TickFunction, UObject* TargetObject, FTickFunction& TargetTickFunction)
{
	if (TargetObject)
	{
#if ENABLE_ANIM_DEBUG
		if (GVarPoseSearchInteractionDiagnoseTickDependencies)
		{
			const ENamedThreads::Type Type = IsInGameThread() ? ENamedThreads::GameThread : ENamedThreads::AnyThread;
			TSet<FTickFunction*> VisitedTickFunctions;
			if (!ValidateTickDependenciesCycles(TickFunction, VisitedTickFunctions))
			{
				LogTickFunction(TickFunction, Type, true, 1);
			}
			check(VisitedTickFunctions.IsEmpty());
			if (!ValidateTickDependenciesCycles(TargetTickFunction, VisitedTickFunctions))
			{
				LogTickFunction(TargetTickFunction, Type, true, 1);
			}
			check(VisitedTickFunctions.IsEmpty());
		}
#endif // ENABLE_ANIM_DEBUG

		TickFunction.AddPrerequisite(TargetObject, TargetTickFunction);

#if ENABLE_ANIM_DEBUG
		if (!TickFunction.GetPrerequisites().Contains(FTickPrerequisite(TargetObject, TargetTickFunction)))
		{
			UE_LOG(LogPoseSearch, Error, TEXT("UE::PoseSearch::AddPrerequisite, Failed to add prerequisite from [%s] to [%s, %s]!"), *TickFunction.DiagnosticMessage(), *TargetObject->GetName(), *TargetTickFunction.DiagnosticMessage());
		}

		if (GVarPoseSearchInteractionDiagnoseTickDependencies)
		{
			const ENamedThreads::Type Type = IsInGameThread() ? ENamedThreads::GameThread : ENamedThreads::AnyThread;
			TSet<FTickFunction*> VisitedTickFunctions;
			if (!ValidateTickDependenciesCycles(TickFunction, VisitedTickFunctions))
			{
				LogTickFunction(TickFunction, Type, true, 1);
			}
			check(VisitedTickFunctions.IsEmpty());
		}
#endif // ENABLE_ANIM_DEBUG
	}
}

// FInteractionSearchContextBase
///////////////////////////////////////////////////////////
const UE::PoseSearch::IPoseHistory* FInteractionSearchContextBase::GetPoseHistory(int32 Index) const
{
	check(PoseHistories.IsValidIndex(Index));
	if (PoseHistories[Index].IsValid())
	{
		return PoseHistories[Index].Pin().Get();
	}
	return nullptr;
}

bool FInteractionSearchContextBase::IsEquivalent(const FInteractionSearchContextBase& Other) const
{
	// skipping bDisableCollisions for equality
	return Database == Other.Database &&
		AnimContexts == Other.AnimContexts &&
		PoseHistories == Other.PoseHistories &&
		Roles == Other.Roles;
}

#if ENABLE_VISUAL_LOG
void FInteractionSearchContextBase::VLogContext(const FColor& Color) const
{
	using namespace UE::PoseSearch;

	if (FVisualLogger::IsRecording())
	{
		static const TCHAR* LogName = TEXT("PoseSearchInteraction");

		const int32 AnimContextsNum = AnimContexts.Num();
		TArray<FVector, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> Locations;
		Locations.SetNum(AnimContextsNum);
		for (int32 Index = 0; Index < AnimContextsNum; ++Index)
		{
			if (const UObject* AnimContext = AnimContexts[Index].Get())
			{
				Locations[Index] = GetContextLocation(AnimContext);
			}
		}

		for (int32 IndexA = 0; IndexA < AnimContextsNum; ++IndexA)
		{
			for (int32 IndexB = IndexA + 1; IndexB < AnimContextsNum; ++IndexB)
			{
				for (int32 IndexAll = 0; IndexAll < AnimContextsNum; ++IndexAll)
				{
					if (const UObject* AnimContext = AnimContexts[IndexAll].Get())
					{
						UE_VLOG_SEGMENT(AnimContext, LogName, Display, Locations[IndexA], Locations[IndexB], Color, TEXT(""));
					}
				}
			}
		}
	}
}
#endif // ENABLE_VISUAL_LOG

#if DO_CHECK
bool FInteractionSearchContextBase::CheckForConsistency() const
{
	if (Database == nullptr)
	{
		return false;
	}

	const int32 Num = AnimContexts.Num();
	if (Num < 1)
	{
		return false;
	}

	if (Num != PoseHistories.Num())
	{
		return false;
	}

	if (Num != Roles.Num())
	{
		return false;
	}

	for (int32 IndexA = 0; IndexA < Num; ++IndexA)
	{
		if (AnimContexts[IndexA] == nullptr)
		{
			return false;
		}

		for (int32 IndexB = IndexA + 1; IndexB < Num; ++IndexB)
		{
			if (AnimContexts[IndexA] == AnimContexts[IndexB])
			{
				return false;
			}
		}
	}

	for (int32 IndexA = 1; IndexA < Num; ++IndexA)
	{
		// AnimContexts must be sorted to have deterministic searches across multiple frames
		if (AnimContexts[IndexA - 1].Get() >= AnimContexts[IndexA].Get())
		{
			return false;
		}
	}

	for (int32 IndexA = 0; IndexA < Num; ++IndexA)
	{
		for (int32 IndexB = IndexA + 1; IndexB < Num; ++IndexB)
		{
			if (Roles[IndexA] == Roles[IndexB])
			{
				return false;
			}
		}
	}

	for (int32 IndexA = 0; IndexA < Num; ++IndexA)
	{
		if (PoseHistories[IndexA] == nullptr)
		{
			return false;
		}

		for (int32 IndexB = IndexA + 1; IndexB < Num; ++IndexB)
		{
			if (PoseHistories[IndexA] == PoseHistories[IndexB])
			{
				return false;
			}
		}
	}

	return true;
}

void FInteractionSearchContextBase::TestHistoryCollectorsThreadingAccess() const
{
	for (int32 Index = 0; Index < AnimContexts.Num(); ++Index)
	{
		if (const UAnimInstance* AnimInstance = Cast<UAnimInstance>(AnimContexts[Index].Get()))
		{
			if (const IPoseHistory* PoseHistory = GetPoseHistory(Index))
			{		
				const USkeleton* Skeleton = AnimInstance->GetRequiredBonesOnAnyThread().GetSkeletonAsset();
				check(Skeleton);

				FTransform ActorRootBoneTransform;
				PoseHistory->GetTransformAtTime(0.f, ActorRootBoneTransform, Skeleton, RootBoneIndexType, ComponentSpaceIndexType);
			}
		}
	}
}
#endif // DO_CHECK

// FInteractionSearchResult
///////////////////////////////////////////////////////////
bool FInteractionSearchResult::operator==(const FInteractionSearchResult& Other) const
{
	// not checking SearchIndex, nor ActorRootTransforms, nor ActorRootBoneTransforms for equality
	return static_cast<const FSearchResult&>(*this) == static_cast<const FSearchResult&>(Other);
}

// FIslandPreTickFunction
///////////////////////////////////////////////////////////
void FInteractionIsland::FPreTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	// Called before any skeletal mesh component tick, when there aren't animation jobs flying. No need to FScopeLock Lock(&Mutex);
	// generating trajectories before running any of the skeletal mesh component ticks
	check(Island);

	if (Island->HasTickDependencies())
	{
		CheckInteractionThreadSafety(Island);

		for (const FInteractionSearchContext& SearchContext : Island->SearchContexts)
		{
			for (int32 Index = 0; Index < SearchContext.Num(); ++Index)
			{
				if (const UObject* AnimContext = SearchContext.GetAnimContext(Index))
				{
					if (const IPoseHistory* PoseHistory = SearchContext.GetPoseHistory(Index))
					{
						// since FInteractionIsland has a tick dependency with the USkeletalMeshComponent it's safe modify the IPoseHistory
						const_cast<IPoseHistory*>(PoseHistory)->GenerateTrajectory(AnimContext, DeltaTime);
					}
				}
			}
		}

#if ENABLE_ANIM_DEBUG
		if (Island->bPreTickFunctionExecuted)
		{
			// @todo: need to figure out why when creating a new island FPreTickFunction gets called twice (it's not a real issue rather than a performance hit)
			//		  use GVarPoseSearchInteractionCacheIslands = false to debug the issue (it destroys the islands every frame)
			UE_LOG(LogPoseSearch, Warning, TEXT("FInteractionIsland::FPreTickFunction::ExecuteTick, called twice before UPoseSearchInteractionSubsystem::Tick!"));
		}
		else
		{
			if (Island->bPostTickFunctionExecuted)
			{
				UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::FPreTickFunction::ExecuteTick, FPostTickFunction::ExecuteTick alreay run?!"));
				Island->LogTickDependencies();
			}

			Island->bPreTickFunctionExecuted = true;
		}
#endif // ENABLE_ANIM_DEBUG
	}
}

// FPostTickFunction
///////////////////////////////////////////////////////////
void FInteractionIsland::FPostTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(Island);

#if ENABLE_ANIM_DEBUG
	if (Island->HasTickDependencies())
	{
		CheckInteractionThreadSafety(Island);

		if (Island->bPostTickFunctionExecuted)
		{
			// @todo: need to figure out why when creating a new island FPostTickFunction gets called twice (it's not a real issue rather than a performance hit)
			//		  use GVarPoseSearchInteractionCacheIslands = false to debug the issue (it destroys the islands every frame)
			UE_LOG(LogPoseSearch, Warning, TEXT("FInteractionIsland::FPostTickFunction::ExecuteTick, called twice before UPoseSearchInteractionSubsystem::Tick!"));
		}
		else
		{
			if (!Island->bPreTickFunctionExecuted)
			{
				UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::FPostTickFunction::ExecuteTick, FPreTickFunction::ExecuteTick didn't run!"));
				Island->LogTickDependencies();
			}

			Island->bPostTickFunctionExecuted = true;
		}
	}
#endif // ENABLE_ANIM_DEBUG
}

// FInteractionIsland
///////////////////////////////////////////////////////////
FInteractionIsland::FInteractionIsland(ULevel* Level, UPoseSearchInteractionSubsystem* Subsystem)
{
	PreTickFunction.bAllowTickBatching = true;
	PreTickFunction.bRunOnAnyThread = true;
	PreTickFunction.Island = this;
	PreTickFunction.RegisterTickFunction(Level);

	PostTickFunction.bAllowTickBatching = true;
	PostTickFunction.bRunOnAnyThread = true;
	PostTickFunction.Island = this;
	PostTickFunction.RegisterTickFunction(Level);

	InteractionSubsystem = Subsystem;
}

FInteractionIsland::~FInteractionIsland()
{
	Uninitialize(false);

	PreTickFunction.UnRegisterTickFunction();
	PostTickFunction.UnRegisterTickFunction();

	InteractionSubsystem = nullptr;
}

IInteractionIslandDependency* FInteractionIsland::FindCustomDependency(UActorComponent* InTickComponent)
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	const int32 NumFeatures = ModularFeatures.GetModularFeatureImplementationCount(IInteractionIslandDependency::FeatureName);

	// Add pre-tick function dependencies
	for (int32 FeatureIndex = 0; FeatureIndex < NumFeatures; ++FeatureIndex)
	{
		if (IInteractionIslandDependency* IslandDependency = static_cast<IInteractionIslandDependency*>(ModularFeatures.GetModularFeatureImplementation(IInteractionIslandDependency::FeatureName, FeatureIndex)))
		{
			if (IslandDependency->CanMakeDependency(nullptr, InTickComponent))
			{
				return IslandDependency;
			}
		}
	}
	return nullptr;
}

void FInteractionIsland::AddTickDependencies(UActorComponent* TickActorComponent, bool bInIsMainActor)
{
	check(TickActorComponent);

	if (IInteractionIslandDependency* IslandDependency = FindCustomDependency(TickActorComponent))
	{
		if (const FTickFunction* TickActorComponentTickFunction = IslandDependency->FindTickFunction(TickActorComponent))
		{
			if(bInIsMainActor)
			{
				// PostTickFunction prerequisites should be empty since we haven't add the main actor tick function yet
				check(PostTickFunction.GetPrerequisites().IsEmpty());

				// adding to PreTickFunction all the tick dependencies TickActorComponent has, so it runs after all the tick dependencies of ALL the TickActorComponents in this FInteractionIsland
				for (const FTickPrerequisite& TickActorComponentPrerequisite : TickActorComponentTickFunction->GetPrerequisites())
				{
					AddPrerequisite(PreTickFunction, TickActorComponentPrerequisite.PrerequisiteObject.Get(), *TickActorComponentPrerequisite.PrerequisiteTickFunction);
				}

				// Add post-tick function dependencies
				IslandDependency->AddSubsequent(InteractionSubsystem, PreTickFunction, TickActorComponent);
				IslandDependency->AddPrerequisite(InteractionSubsystem, PostTickFunction, TickActorComponent);

				check(!bHasTickDependencies);
				bHasTickDependencies = true;
			}
			else
			{
				// PostTickFunction should contain only the tick function to the main actor's one
				check(PostTickFunction.GetPrerequisites().Num());

				// adding to PreTickFunction all the tick dependencies TickActorComponent has, so it runs after all the tick dependencies of ALL the TickActorComponents in this FInteractionIsland
				// BUT excluding the main actor tick fuction that is PostTickFunction.GetPrerequisites()[0]
				const FTickFunction* MainActorTickFunction = PostTickFunction.GetPrerequisites()[0].Get();
				for (const FTickPrerequisite& TickActorComponentPrerequisite : TickActorComponentTickFunction->GetPrerequisites())
				{
					if (TickActorComponentPrerequisite.PrerequisiteTickFunction != MainActorTickFunction)
					{
						AddPrerequisite(PreTickFunction, TickActorComponentPrerequisite.PrerequisiteObject.Get(), *TickActorComponentPrerequisite.PrerequisiteTickFunction);
					}
				}

				// Add post-tick function dependencies
				IslandDependency->AddSubsequent(InteractionSubsystem, PostTickFunction, TickActorComponent);
				check(bHasTickDependencies);
			}
		}
		else
		{
			UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::AddTickDependencies, error while retrieving the tick function for %s"), *TickActorComponent->GetName());
		}
	}
	else
	{
		if (bInIsMainActor)
		{
			// PostTickFunction prerequisites should be empty since we haven't add the main actor tick function yet
			check(PostTickFunction.GetPrerequisites().IsEmpty());

			// adding to PreTickFunction all the tick dependencies TickActorComponent has, so it runs after all the tick dependencies of ALL the TickActorComponents in this FInteractionIsland
			for (FTickPrerequisite& TickActorComponentPrerequisite : TickActorComponent->PrimaryComponentTick.GetPrerequisites())
			{
				AddPrerequisite(PreTickFunction, TickActorComponentPrerequisite.PrerequisiteObject.Get(), *TickActorComponentPrerequisite.PrerequisiteTickFunction);
			}

			// Add post-tick function dependencies
			// it should be this island, but it's not a UObject, so we use the InteractionSubsystem
			AddPrerequisite(TickActorComponent->PrimaryComponentTick, InteractionSubsystem, PreTickFunction);
			AddPrerequisite(PostTickFunction, TickActorComponent, TickActorComponent->PrimaryComponentTick);

			check(!bHasTickDependencies);
			bHasTickDependencies = true;
		}
		else
		{
			// PostTickFunction should contain only the tick function to the main actor's one
			check(PostTickFunction.GetPrerequisites().Num());

			// adding to PreTickFunction all the tick dependencies TickActorComponent has, so it runs after all the tick dependencies of ALL the TickActorComponents in this FInteractionIsland
			// BUT excluding the main actor tick fuction that is PostTickFunction.GetPrerequisites()[0]
			const FTickFunction* MainActorTickFunction = PostTickFunction.GetPrerequisites()[0].Get();
			for (FTickPrerequisite& TickActorComponentPrerequisite : TickActorComponent->PrimaryComponentTick.GetPrerequisites())
			{
				if (TickActorComponentPrerequisite.PrerequisiteTickFunction != MainActorTickFunction)
				{
					AddPrerequisite(PreTickFunction, TickActorComponentPrerequisite.PrerequisiteObject.Get(), *TickActorComponentPrerequisite.PrerequisiteTickFunction);
				}
			}

			// Add post-tick function dependencies
			// it should be this island, but it's not a UObject, so we use the InteractionSubsystem
			AddPrerequisite(TickActorComponent->PrimaryComponentTick, InteractionSubsystem, PostTickFunction);
			check(bHasTickDependencies);
		}
	}

#if ENABLE_ANIM_DEBUG
	if (GVarPoseSearchInteractionDiagnoseTickDependencies)
	{
		TSet<FTickFunction*> VisitedTickFunctions;
		bool bIsPreTickFunctionValid = ValidateTickDependenciesCycles(PreTickFunction, VisitedTickFunctions);
		check(VisitedTickFunctions.IsEmpty());
		bool bIsPostTickFunctionValid = ValidateTickDependenciesCycles(PostTickFunction, VisitedTickFunctions);
		check(VisitedTickFunctions.IsEmpty());
		if (!bIsPreTickFunctionValid || !bIsPostTickFunctionValid)
		{
			// if this validation triggers here, FInteractionIsland is not respectint the already present dependencies, creating cycles
			UE_LOG(LogPoseSearch, Error, TEXT("============== FInteractionIsland::AddTickDependencies ValidateTickDependencies failed! Analyze the log and tune the FPoseSearchInteractionAvailability::TickPriority =============="));
			LogTickDependencies();
		}
	}
#endif // ENABLE_ANIM_DEBUG
}

void FInteractionIsland::RemoveTickDependencies(bool bValidateTickDependencies)
{
	// Called by UPoseSearchInteractionSubsystem::Tick when there aren't animation jobs flying.
	check(IsInGameThread());

	check(TickActorComponents.Num() == IslandAnimContexts.Num());

	if (!bHasTickDependencies)
	{
#if ENABLE_ANIM_DEBUG
		if (bValidateTickDependencies && (bPreTickFunctionExecuted || bPostTickFunctionExecuted))
		{
			if (bPreTickFunctionExecuted)
			{
				UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::RemoveTickDependencies, unexpected FPreTickFunction::ExecuteTick run!"));
			}
			else
			{
				UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::RemoveTickDependencies, unexpected FPostTickFunction::ExecuteTick run!"));
			}
				
			LogTickDependencies();
		}
#endif // ENABLE_ANIM_DEBUG
	}
	else
	{
#if ENABLE_ANIM_DEBUG
		if (bValidateTickDependencies && (!bPreTickFunctionExecuted || !bPostTickFunctionExecuted))
		{
			if (!bPreTickFunctionExecuted)
			{
				UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::RemoveTickDependencies, expected FPreTickFunction::ExecuteTick didn't run!"));
			}
			else
			{
				UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::RemoveTickDependencies, expected FPostTickFunction::ExecuteTick didn't run!"));
			}
			
			LogTickDependencies();
		}
#endif // ENABLE_ANIM_DEBUG

		// removing ALL the prerequisites form the PreTickFunction
		while (!PreTickFunction.GetPrerequisites().IsEmpty())
		{
			const FTickPrerequisite& PreTickFunctionPrerequisite = PreTickFunction.GetPrerequisites().Last();
			PreTickFunction.RemovePrerequisite(PreTickFunctionPrerequisite.PrerequisiteObject.Get(), *PreTickFunctionPrerequisite.PrerequisiteTickFunction);
		}

		bool bMainActor = true;
		for (TWeakObjectPtr<UActorComponent>& TickActorComponentPtr : TickActorComponents)
		{
			if (UActorComponent* TickActorComponent = TickActorComponentPtr.Get())
			{
				if(IInteractionIslandDependency* IslandDependency = FindCustomDependency(TickActorComponent))
				{
					if (bMainActor)
					{
						IslandDependency->RemoveSubsequent(InteractionSubsystem, PreTickFunction, TickActorComponent);
						IslandDependency->RemovePrerequisite(InteractionSubsystem, PostTickFunction, TickActorComponent);
					}
					else
					{
						IslandDependency->RemoveSubsequent(InteractionSubsystem, PostTickFunction, TickActorComponent);
					}
				}
				else
				{
					if (bMainActor)
					{
						TickActorComponent->PrimaryComponentTick.RemovePrerequisite(InteractionSubsystem, PreTickFunction);
						PostTickFunction.RemovePrerequisite(TickActorComponent, TickActorComponent->PrimaryComponentTick);
					}
					else
					{
						TickActorComponent->PrimaryComponentTick.RemovePrerequisite(InteractionSubsystem, PostTickFunction);
					}
				}
			}
			bMainActor = false;
		}

		check(PreTickFunction.GetPrerequisites().IsEmpty());
		check(PostTickFunction.GetPrerequisites().IsEmpty());

		bHasTickDependencies = false;
	}

#if ENABLE_ANIM_DEBUG
	bPreTickFunctionExecuted = false;
	bPostTickFunctionExecuted = false;
#endif // ENABLE_ANIM_DEBUG
}

void FInteractionIsland::InjectToActor(const UObject* AnimContext, bool bAddTickDependencies)
{
	check(IsInGameThread());

	// Called by UPoseSearchInteractionSubsystem::Tick when there aren't animation jobs flying. No need to FScopeLock Lock(&Mutex);
	if (AnimContext)
	{
#if ENABLE_ANIM_DEBUG
		if (bPreTickFunctionExecuted || bPostTickFunctionExecuted)
		{
			if (bPreTickFunctionExecuted)
			{
				UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::InjectToActor, unexpected FPreTickFunction::ExecuteTick run!"));
			}
			else
			{
				UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::InjectToActor, unexpected FPostTickFunction::ExecuteTick run!"));
			}
				
			LogTickDependencies();
		}
#endif // ENABLE_ANIM_DEBUG

		if (UActorComponent* TickActorComponent = FindComponentForTickDependencies(AnimContext))
		{
			const bool bIsMainActor = IslandAnimContexts.IsEmpty();

			//	tick order: 
			//		ALL TickActorComponents prerequisites (ultimately we're looking to have UCharacterMovementComponent or UCharacterMoverComponent ticked) ->
			//			Island.PreTickFunction ->
			//				first injected TickActorComponent (USkeletalMeshComponent, or UAnimNextComponent) ->
			//					Island.PostTickFunction ->
			//						other TickActorComponent(s)
			TickActorComponents.AddUnique(TickActorComponent);
			IslandAnimContexts.AddUnique(AnimContext);

			// making sure that if we add a unique TickActorComponent, we add as well a unique PostTickComponent
			// (so we can remove them later on in a consistent fashion)
			check(TickActorComponents.Num() == IslandAnimContexts.Num());

			if (bAddTickDependencies)
			{
				AddTickDependencies(TickActorComponent, bIsMainActor);
			}
			else
			{
				check(!bHasTickDependencies);
			}
		}
	}
}

void FInteractionIsland::AddSearchContext(const FInteractionSearchContext& SearchContext)
{
#if DO_CHECK
	check(SearchContext.CheckForConsistency());
#endif
	check(IsInGameThread());
	SearchContexts.Add(SearchContext);
}

void FInteractionIsland::Uninitialize(bool bValidateTickDependencies)
{
#if ENABLE_ANIM_DEBUG
	if (GVarPoseSearchInteractionDiagnoseTickDependencies)
	{
		TSet<FTickFunction*> VisitedTickFunctions;
		bool bIsPreTickFunctionValid = ValidateTickDependenciesCycles(PreTickFunction, VisitedTickFunctions);
		check(VisitedTickFunctions.IsEmpty());
		bool bIsPostTickFunctionValid = ValidateTickDependenciesCycles(PostTickFunction, VisitedTickFunctions);
		check(VisitedTickFunctions.IsEmpty());
		if (!bIsPreTickFunctionValid || !bIsPostTickFunctionValid)
		{
			// if this validation triggers here, some additional tick dependency outside FInteractionIsland has been injected witout respecting the already present dependencies, creating cycles
			UE_LOG(LogPoseSearch, Error, TEXT("============== FInteractionIsland::Uninitialize ValidateTickDependencies failed! =============="));
			LogTickDependencies();
		}
	}
#endif // ENABLE_ANIM_DEBUG

	RemoveTickDependencies(bValidateTickDependencies);

	if (IsInitialized())
	{
		TickActorComponents.Reset();
		IslandAnimContexts.Reset();

		SearchContexts.Reset();
		SearchResults.Reset();
		bSearchPerfomed = false;
	}
	else
	{
		check(TickActorComponents.IsEmpty() && IslandAnimContexts.IsEmpty() && SearchContexts.IsEmpty() && SearchResults.IsEmpty() && !bSearchPerfomed);
	}
}

bool FInteractionIsland::HasTickDependencies() const
{
	return bHasTickDependencies;
}

bool FInteractionIsland::IsInitialized() const
{
	return !SearchContexts.IsEmpty();
}

const UObject* FInteractionIsland::GetMainAnimContext() const
{
	return !IslandAnimContexts.IsEmpty() ? IslandAnimContexts[0].Get() : nullptr;
}

const AActor* FInteractionIsland::GetMainActor() const
{
	if (IsInitialized())
	{
		return GetContextOwningActor(GetMainAnimContext());
	}
	return nullptr;
}

#if ENABLE_ANIM_DEBUG
void FInteractionIsland::LogTickDependencies(const TConstArrayView<TWeakObjectPtr<UActorComponent>> TickActorComponents, int32 InteractionIslandIndex)
{
	check(IsInGameThread());

	for (const TWeakObjectPtr<UActorComponent>& TickActorComponentPtr : TickActorComponents)
	{
		if (UActorComponent* TickActorComponent = TickActorComponentPtr.Get())
		{
			UE_LOG(LogPoseSearch, Log, TEXT("============== %s (Island %d) =============="), *TickActorComponent->GetOwner()->GetName(), InteractionIslandIndex);
			if (IInteractionIslandDependency* IslandDependency = FindCustomDependency(TickActorComponent))
			{
				// AnimNextComponent case
				if (const FTickFunction* TickActorComponentTickFunction = IslandDependency->FindTickFunction(TickActorComponent))
				{
					LogTickFunction(*const_cast<FTickFunction*>(TickActorComponentTickFunction), ENamedThreads::GameThread, true, 1);
				}
				else
				{
					UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::LogTickDependencies, error while retrieving the tick function for to %s"), *TickActorComponent->GetName());
				}
			}
			else
			{
				// SkeletalMeshComponent / AnimInstance case
				LogTickFunction(TickActorComponent->PrimaryComponentTick, ENamedThreads::GameThread, true, 1);
			}
		}
		else
		{
			UE_LOG(LogPoseSearch, Log, TEXT("============== !!!Missing Actor!!! (Island %d) =============="), InteractionIslandIndex);
		}
	}
}

void FInteractionIsland::LogTickDependencies() const
{
	const int32 InteractionIslandIndex = InteractionSubsystem->GetInteractionIslands().IndexOfByKey(this);

	if (IsInGameThread())
	{
		LogTickDependencies(TickActorComponents, InteractionIslandIndex);
	}
	else
	{
		TArray<TWeakObjectPtr<UActorComponent>> TickActorComponentsCopy = TickActorComponents;
		FFunctionGraphTask::CreateAndDispatchWhenReady([TickActorComponentsCopy, InteractionIslandIndex]()
		{
			LogTickDependencies(TickActorComponentsCopy, InteractionIslandIndex);
		}, TStatId(), nullptr, ENamedThreads::GameThread);
	}
}
#endif // ENABLE_ANIM_DEBUG

bool FInteractionIsland::DoSearch_AnyThread(const UObject* AnimContext, const TConstArrayView<FValidInteractionSearch> ValidInteractionSearches, FPoseSearchBlueprintResult& Result)
{
	check(AnimContext);

	if (bSearchPerfomed)
	{
		// we now support multiple searches from the same AnimContext, and we return the already performed
		// and cached result, so check(AnimContext != GetMainAnimContext()) is no longer valid!
		return GetResult_AnyThread(AnimContext, Result);
	}

	// searches are performed only on the MainAnimContext / MainActor
	if (AnimContext != GetMainAnimContext())
	{
		// search failed, because AnimContext is not the context from the main actor. continuing the search would lead to threading issues,
		// since the threading model expect the main actor to perform the search! (this happens when recompiling animation blue prints..)
		Result = FPoseSearchBlueprintResult();
		return false;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_UPoseSearchInteractionInteractionIsland_Search);

	FMemMark Mark(FMemStack::Get());

	TArray<const UObject*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> AnimContexts;
	TArray<const UE::PoseSearch::IPoseHistory*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> PoseHistories;
	TArray<FSearchResult, TMemStackAllocator<>> PoseSearchResults;

	// SearchContexts are modified only by UPoseSearchInteractionSubsystem::Tick and constant otherwise, so it's safe to access them in a threaded enviroment without locks
	PoseSearchResults.SetNum(SearchContexts.Num());

	for (int32 SearchIndex = 0; SearchIndex < SearchContexts.Num(); ++SearchIndex)
	{
		FInteractionSearchContext& SearchContext = SearchContexts[SearchIndex];
		const UPoseSearchDatabase* Database = SearchContext.Database.Get();
		if (!Database)
		{
			UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::DoSearch_AnyThread invalid context database"));
			return false;
		}

		if (!Database->Schema)
		{
			UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::DoSearch_AnyThread invalid schema for context database %s"), *Database->GetName());
			return false;
		}

		const int32 NumRoles = SearchContext.Num();
		
		AnimContexts.Reset();
		PoseHistories.Reset();

		for (int32 RoleIndex = 0; RoleIndex < NumRoles; ++RoleIndex)
		{
			const UObject* SearchContextAnimContext = SearchContext.GetAnimContext(RoleIndex);
			if (!SearchContextAnimContext)
			{
				UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::DoSearch_AnyThread null anim context"));
				return false;
			}

			AnimContexts.Add(SearchContextAnimContext);

			const UE::PoseSearch::IPoseHistory* PoseHistory = SearchContext.GetPoseHistory(RoleIndex);
			if (!PoseHistory)
			{
				UE_LOG(LogPoseSearch, Error, TEXT("FInteractionIsland::DoSearch_AnyThread null PoseHistory"));
				return false;
			}

			PoseHistories.Add(PoseHistory);
		}

		const UObject* AssetsToSearch[] = { Database };
		FPoseSearchFutureProperties PoseSearchFutureProperties;

		// @todo: we could perform multiple UPoseSearchLibrary::MotionMatch in parallel!
		PoseSearchResults[SearchIndex] = UPoseSearchLibrary::MotionMatch(AnimContexts, SearchContext.GetRoles(), PoseHistories,
			AssetsToSearch, SearchContext.GetContinuingProperties(), PoseSearchFutureProperties, FPoseSearchEvent());
	}
		
	InitSearchResults(SearchResults, PoseSearchResults, SearchContexts);
	bSearchPerfomed = true;
	
	return GetResult_AnyThread(AnimContext, Result);
}

bool FInteractionIsland::GetResult_AnyThread(const UObject* AnimContext, FPoseSearchBlueprintResult& Result, bool bCompareOwningActors)
{
	check(AnimContext);

	const AActor* Actor = bCompareOwningActors ? GetContextOwningActor(AnimContext) : nullptr;

	// looking for AnimContext in SearchResults to fill up Result
	for (const FInteractionSearchResult& SearchResult : SearchResults)
	{
		const FInteractionSearchContext& SearchContext = SearchContexts[SearchResult.SearchIndex];
		for (int32 AnimContextIndex = 0; AnimContextIndex < SearchContext.Num(); ++AnimContextIndex)
		{
			bool bAnimContextFound = false;
			if (bCompareOwningActors)
			{
				bAnimContextFound = GetContextOwningActor(SearchContext.GetAnimContext(AnimContextIndex)) == Actor;
			}
			else
			{
				bAnimContextFound = SearchContext.GetAnimContext(AnimContextIndex) == AnimContext;
			}		

			if (bAnimContextFound)
			{
				// @todo: perhaps add a custom Result.InitFrom(SearchResult, 1.f) for MM interactions
				const UPoseSearchDatabase* Database = SearchResult.Database.Get();
				check(Database);

				const FSearchIndexAsset* SearchIndexAsset = SearchResult.GetSearchIndexAsset();
				check(SearchIndexAsset);

				const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(*SearchIndexAsset);
				check(DatabaseAnimationAssetBase);

				Result.SelectedAnim = DatabaseAnimationAssetBase->GetAnimationAsset();
				Result.SelectedTime = SearchResult.AssetTime;
				Result.bIsContinuingPoseSearch = SearchResult.bIsContinuingPoseSearch;
				Result.bLoop = SearchIndexAsset->IsLooping();
				Result.bIsMirrored = SearchIndexAsset->IsMirrored();
				Result.BlendParameters = SearchIndexAsset->GetBlendParameters();
				Result.SelectedDatabase = Database;
				Result.SearchCost = SearchResult.PoseCost;
				Result.bIsInteraction = true;
				Result.Role = SearchContext.GetRole(AnimContextIndex);

				// figuring out the WantedPlayRate
				Result.WantedPlayRate = 1.f;
				//if (Future.Animation && Future.IntervalTime > 0.f)
				//{
				//	if (const UPoseSearchFeatureChannel_PermutationTime* PermutationTimeChannel = Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_PermutationTime>())
				//	{
				//		const FSearchIndex& SearchIndex = Database->GetSearchIndex();
				//		if (!SearchIndex.IsValuesEmpty())
				//		{
				//			TConstArrayView<float> ResultData = Database->GetSearchIndex().GetPoseValues(SearchResult.PoseIdx);
				//			const float ActualIntervalTime = PermutationTimeChannel->GetPermutationTime(ResultData);
				//			ProviderResult.WantedPlayRate = ActualIntervalTime / Future.IntervalTime;
				//		}
				//	}
				//}

				if (const UMultiAnimAsset* MultiAnimAsset = Cast<UMultiAnimAsset>(Result.SelectedAnim))
				{
					const int32 NumRoles = MultiAnimAsset->GetNumRoles();
					Result.ActorRootTransforms.SetNum(NumRoles);
					Result.ActorRootBoneTransforms.SetNum(NumRoles);
					Result.AnimContexts.SetNum(NumRoles);

					const FRoleToIndex InteractionSearchContextRoleToIndex = MakeRoleToIndex(SearchContext.GetRoles());

					for (int32 MultiAnimAssetRoleIndex = 0; MultiAnimAssetRoleIndex < NumRoles; ++MultiAnimAssetRoleIndex)
					{
						if (const int32* InteractionSearchContextRoleIndex = InteractionSearchContextRoleToIndex.Find(MultiAnimAsset->GetRole(MultiAnimAssetRoleIndex)))
						{
							Result.ActorRootTransforms[MultiAnimAssetRoleIndex] = SearchResult.ActorRootTransforms[*InteractionSearchContextRoleIndex];
							Result.ActorRootBoneTransforms[MultiAnimAssetRoleIndex] = SearchResult.ActorRootBoneTransforms[*InteractionSearchContextRoleIndex];
							Result.AnimContexts[MultiAnimAssetRoleIndex] = SearchContext.GetAnimContext(*InteractionSearchContextRoleIndex);
						}
						else
						{
							Result.ActorRootTransforms[MultiAnimAssetRoleIndex] = FTransform::Identity;
							Result.ActorRootBoneTransforms[MultiAnimAssetRoleIndex] = FTransform::Identity;
							Result.AnimContexts[MultiAnimAssetRoleIndex] = nullptr;
						}
					}
				}
				else
				{
					// @todo: should we support trivial "interactions" with only a character defined using some other assets rather then UMultiAnimAsset?
					check(Result.AnimContexts.Num() == 1);

					Result.ActorRootTransforms = SearchResult.ActorRootTransforms;
					Result.ActorRootBoneTransforms = SearchResult.ActorRootBoneTransforms;
					
					Result.AnimContexts.SetNum(1);
					Result.AnimContexts[0] = SearchContext.GetAnimContext(0);
				}

				// we found our AnimContext: we can stop searching
				return true;
			}
		}
	}

	Result = FPoseSearchBlueprintResult();
	return false;
}

const FInteractionSearchResult* FInteractionIsland::FindSearchResult(const FInteractionSearchContext& SearchContext) const
{
	// called only by UPoseSearchInteractionSubsystem::Tick via UPoseSearchInteractionSubsystem::PopulateContinuingProperties so no need to lock SearchResultsMutex to protect the read of SearchResults
	check(IsInGameThread());

	// searching for InSearchContext in all the SearchContexts referenced by valid active SearchResults
	for (const FInteractionSearchResult& SearchResult : SearchResults)
	{
		const FInteractionSearchContext& LocalSearchContext = SearchContexts[SearchResult.SearchIndex];
		if (LocalSearchContext.IsEquivalent(SearchContext))
		{
			return &SearchResult;
		}
	}
	return nullptr;
}

} // namespace UE::PoseSearch
