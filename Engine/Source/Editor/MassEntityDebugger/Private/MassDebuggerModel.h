// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MassArchetypeTypes.h"
#include "MassEntityQuery.h"
#include "MassProcessorDependencySolver.h"
#include "MassProcessingTypes.h"
#include "MassProcessor.h"
#include "Types/SlateEnums.h"
#include "MassDebugger.h"


class UMassProcessor;
struct FMassArchetypeHandle;
struct FMassDebuggerModel;
struct FMassEntityHandle;
struct FMassEntityManager;
struct FMassEntityQuery;
class UWorld;
class SMassEntitiesView;
class SMassDebugger;

enum class EMassDebuggerSelectionMode : uint8
{
	None,
	Processor,
	Archetype,
	// @todo future:
	// Fragment
	MAX
};

enum class EMassDebuggerProcessorSelection : uint8
{
	None,
	Selected,
	MAX
};

enum class EMassDebuggerProcessingGraphNodeSelection : uint8
{
	None,
	WaitFor,
	Block,
	MAX
};


struct FMassDebuggerQueryData
{
	FMassDebuggerQueryData(const FMassEntityQuery& Query, const FText& InLabel);
	FMassDebuggerQueryData(const FMassSubsystemRequirements& SubsystemRequirements, const FText& InLabel);

	FMassExecutionRequirements ExecutionRequirements;
	FText Label;
	FText AdditionalInformation;

	int32 GetTotalBitsUsedCount();
	bool IsEmpty() const;
}; 

struct FMassDebuggerArchetypeData
{
	FMassDebuggerArchetypeData(const FMassArchetypeHandle& ArchetypeHandle);

	FMassArchetypeHandle Handle;

	FMassArchetypeCompositionDescriptor Composition;

	/** Hash of the Compositions. */
	uint32 CompositionHash = 0;
	/** Combined hash of composition and shared fragments. */
	uint32 FullHash = 0;

	/** Archetype statistics */
	UE::Mass::Debug::FArchetypeStats ArchetypeStats;

	/** Child debugger data (same as parent, but changed in some way) */
	TArray<TSharedPtr<FMassDebuggerArchetypeData>> Children;
	/** Parent debugger data. */
	TWeakPtr<FMassDebuggerArchetypeData> Parent;


	/** Index in FMassDebuggerModel::CachedArchetypes */
	int32 Index = INDEX_NONE;
	/** Display label */
	FText Label;
	/** Display label */
	FText LabelLong;
	/** Display label tooltip */
	FText LabelTooltip;
	/** FullHash as a display string */
	FText HashLabel;
	/** Primary debug name, used for grouping derived archetypes. */
	FString PrimaryDebugName;

	/** True if the archetype is selected. */
	bool bIsSelected = false;

	int32 GetTotalBitsUsedCount() const;
};

struct FMassDebuggerProcessorData
{
	FMassDebuggerProcessorData(const UMassProcessor& InProcessor);
	FMassDebuggerProcessorData(const FMassEntityManager& InEntityManager, const UMassProcessor& InProcessor, const TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& InTransientArchetypesMap);

private:
	void SetProcessor(const UMassProcessor& InProcessor);

public:
	FString Name;
	FString Label;
	uint32 ProcessorHash = 0;
	bool bIsActive = true;
	TWeakPtr<const FMassEntityManager> EntityManager;
	TWeakObjectPtr<const UMassProcessor> Processor;
	EMassDebuggerProcessorSelection Selection = EMassDebuggerProcessorSelection::None;
	TSharedPtr<FMassDebuggerQueryData> ProcessorRequirements;
	TArray<TSharedPtr<FMassDebuggerQueryData>> Queries;
	TArray<TSharedPtr<FMassDebuggerArchetypeData>> ValidArchetypes;
#if WITH_MASSENTITY_DEBUG
	FString Description;
#endif // WITH_MASSENTITY_DEBUG	
};

struct FMassDebuggerProcessingGraphNode
{
	explicit FMassDebuggerProcessingGraphNode(const TSharedPtr<FMassDebuggerProcessorData>& InProcessorData, const UMassCompositeProcessor::FDependencyNode& InProcessorNode = UMassCompositeProcessor::FDependencyNode());
	
	FText GetLabel() const;

	TSharedPtr<FMassDebuggerProcessorData> ProcessorData;
	TArray<int32> WaitForNodes;
	TArray<int32> BlockNodes;
	EMassDebuggerProcessingGraphNodeSelection GraphNodeSelection = EMassDebuggerProcessingGraphNodeSelection::None;
};

struct FMassDebuggerProcessingGraph
{
	FMassDebuggerProcessingGraph(const FMassDebuggerModel& DebuggerModel, TNotNull<const UMassCompositeProcessor*> InGraphOwner);

	FString Label;
	TArray<FMassDebuggerProcessingGraphNode> GraphNodes;
	bool bSingleTheadGraph = !bool(MASS_DO_PARALLEL);
};


struct FMassDebuggerEnvironment
{
	explicit FMassDebuggerEnvironment(const TSharedRef<const FMassEntityManager>& InEntityManager);

	bool operator==(const FMassDebuggerEnvironment& Other) const { return EntityManager == Other.EntityManager; }

	FString GetDisplayName() const;
	TSharedPtr<const FMassEntityManager> GetEntityManager() const;
	bool IsWorldValid() const { return World.IsValid(); }
	bool NeedsValidWorld() const { return bNeedsValidWorld; }
	
	TWeakPtr<const FMassEntityManager> EntityManager;
	TMap<FName, UE::Mass::Debug::FProcessorProviderFunction> ProcessorProviders;
	TWeakObjectPtr<UWorld> World;
	bool bNeedsValidWorld = false;
};


struct FMassDebuggerModel
{
	DECLARE_MULTICAST_DELEGATE(FOnRefresh);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProcessorsSelected, TConstArrayView<TSharedPtr<FMassDebuggerProcessorData>>, ESelectInfo::Type);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnArchetypesSelected, TConstArrayView<TSharedPtr<FMassDebuggerArchetypeData>>, ESelectInfo::Type);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFragmentSelected, FName FragmentName);

	FMassDebuggerModel();
	~FMassDebuggerModel();

	void SetEnvironment(const TSharedPtr<FMassDebuggerEnvironment>& Item);

	void RefreshAll();

	void SelectProcessor(TSharedPtr<FMassDebuggerProcessorData>& Processor);
	void SelectProcessors(TArrayView<TSharedPtr<FMassDebuggerProcessorData>> Processors, ESelectInfo::Type SelectInfo);
	void ClearProcessorSelection();

	void SelectArchetypes(TArrayView<TSharedPtr<FMassDebuggerArchetypeData>> SelectedArchetypes, ESelectInfo::Type SelectInfo);
	void ClearArchetypeSelection();

	bool IsCurrentEnvironment(const FMassDebuggerEnvironment& InEnvironment) const { return Environment && *Environment.Get() == InEnvironment; }
	bool IsCurrentEnvironmentValid() const { return Environment && Environment->EntityManager.IsValid(); }
	bool HasEnvironmentSelected() const { return static_cast<bool>(Environment); }

	void CacheArchetypesData(TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& OutTransientArchetypesMap); 
	void CacheProcessorsData(const TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& InTransientArchetypesMap, TArray<TNotNull<const UMassCompositeProcessor*>>& OutCompositeProcessors);
	void CacheProcessingGraphs(TConstArrayView<TNotNull<const UMassCompositeProcessor*>> InCompositeProcessors);

	float MinDistanceToSelectedArchetypes(const TSharedPtr<FMassDebuggerArchetypeData>& InArchetypeData) const;

	FText GetDisplayName() const;

	void MarkAsStale();
	bool IsStale() const;

	const TSharedPtr<FMassDebuggerProcessorData>& GetProcessorDataChecked(const UMassProcessor& Processor) const;

	void RegisterEntitiesView(TSharedRef<SMassEntitiesView> EntitiesView, int32 Index);

	static const int32 MaxEntityViewCount = 1;
	void ShowEntitiesView(int Index, FMassArchetypeHandle ArchetypeHandle);
	void ShowEntitiesView(int Index, TArray<FMassEntityHandle> EntitieHandles);
	void ShowEntitiesView(int Index, FMassEntityQuery& Query);
	void ShowEntitiesView(int Index, TConstArrayView<FMassEntityQuery*> InQueries);
	void ResetEntitiesViews();
	TWeakPtr<SMassEntitiesView> ShowEntitiesView(int32 Index);

protected:
	void StoreArchetypes(const FMassEntityManager& EntityManager, TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& OutTransientArchetypesMap);

	void ResetSelectedArchetypes();
	void ResetSelectedProcessors();

	void OnEntitySelected(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle);

public:
	TWeakPtr<SMassDebugger> DebuggerWindow;

	FOnRefresh OnRefreshDelegate;
	FOnProcessorsSelected OnProcessorsSelectedDelegate;
	FOnArchetypesSelected OnArchetypesSelectedDelegate;
	FOnFragmentSelected OnFragmentSelectedDelegate;

	EMassDebuggerSelectionMode SelectionMode = EMassDebuggerSelectionMode::None;

	TSharedPtr<FMassDebuggerEnvironment> Environment;
	struct FProcessorCollection
	{
		FProcessorCollection(FName InLabel = FName())
			: Label(InLabel)
		{	
		}
		FName Label;
		TArray<TSharedPtr<FMassDebuggerProcessorData>> Container;
	};

	TArray<TSharedPtr<FProcessorCollection>> CachedProcessorCollections;
	TArray<TSharedPtr<FMassDebuggerProcessorData>> SelectedProcessors;
	TArray<TSharedPtr<FMassDebuggerArchetypeData>> CachedAllArchetypes;
	TArray<TSharedPtr<FMassDebuggerArchetypeData>> CachedArchetypeRepresentatives;
	TArray<TSharedPtr<FMassDebuggerArchetypeData>> SelectedArchetypes;
	TArray<TSharedPtr<FMassDebuggerProcessingGraph>> CachedProcessingGraphs;

	TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>> HandleToArchetypeMap;

	TArray<TArray<float>> ArchetypeDistances;

	FString EnvironmentDisplayName;

	FDelegateHandle OnEntitySelectedHandle;

	void SelectFragment(FName InFragementName);
	FName GetSelectedFragment();

protected:	
	TArray<TSharedPtr<FMassDebuggerProcessorData>> AllCachedProcessors;
	TArray<TWeakPtr<SMassEntitiesView>> EntityViews;
	FName SelectedFragmentName;
public:
	UE_DEPRECATED(5.6, "CachedProcessors property is now deprecated. Use CachedProcessorCollections instead. ")
	TArray<TSharedPtr<FMassDebuggerProcessorData>> CachedProcessors;
};

