// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "StateTreeSchema.h"
#include "StateTreePropertyBindings.h"
#include "StateTreeInstanceData.h"
#include "StateTreeTypes.h"
#include "Misc/TransactionallySafeRWLock.h"
#include "UObject/ObjectKey.h"
#include "StateTree.generated.h"

#define UE_API STATETREEMODULE_API

class UUserDefinedStruct;

template<bool>
struct TStateTreeStrongExecutionContext;

/** Custom serialization version for StateTree Asset */
struct FStateTreeCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,
		// Separated conditions to shared instance data.
		SharedInstanceData,
		// Moved evaluators to be global.
		GlobalEvaluators,
		// Moved instance data to arrays.
		InstanceDataArrays,
		// Added index types.
		IndexTypes,
		// Added events.
		AddedEvents,
		// Testing mishap
		AddedFoo,
		// Changed transition delay
		TransitionDelay,
		// Added external transitions
		AddedExternalTransitions,
		// Changed how bindings are represented
		ChangedBindingsRepresentation,
		// Added guid to transitions
		AddedTransitionIds,
		// Added data handles
		AddedDataHandlesIds,
		// Added linked asset state
		AddedLinkedAssetState,
		// Change how external data is accessed
		ChangedExternalDataAccess,
		// Added override option for parameters
		OverridableParameters,
		// Added override option for state parameters
		OverridableStateParameters,
		// Added storing global parameters in instance storage
		StoringGlobalParametersInInstanceStorage,
		// Added binding to events
		AddedBindingToEvents,
		// Added checking parent states' prerequisites when activating child state directly.
		AddedCheckingParentsPrerequisites,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	/** The GUID for this custom version number */
	UE_API const static FGuid GUID;

private:
	FStateTreeCustomVersion() {}
};


#if WITH_EDITOR
/** Struct containing information about the StateTree runtime memory usage. */
struct FStateTreeMemoryUsage
{
	FStateTreeMemoryUsage() = default;
	FStateTreeMemoryUsage(const FString InName, const FStateTreeStateHandle InHandle = FStateTreeStateHandle::Invalid)
		: Name(InName)
		, Handle(InHandle)
	{
	}
	
	void AddUsage(FConstStructView View);
	void AddUsage(const UObject* Object);

	FString Name;
	FStateTreeStateHandle Handle;
	int32 NodeCount = 0;
	int32 EstimatedMemoryUsage = 0;
	int32 ChildNodeCount = 0;
	int32 EstimatedChildMemoryUsage = 0;
};
#endif


/**
 * StateTree asset. Contains the StateTree definition in both editor and runtime (baked) formats.
 */
UCLASS(MinimalAPI, BlueprintType)
class UStateTree : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @return Default instance data. */
	const FStateTreeInstanceData& GetDefaultInstanceData() const
	{
		return DefaultInstanceData;
	}

	/** @return Shared instance data. */
	UE_API TSharedPtr<FStateTreeInstanceData> GetSharedInstanceData() const;

	/** @return Number of context data views required for StateTree execution (Tree params, context data, External data). */
	int32 GetNumContextDataViews() const
	{
		return NumContextData;
	}

	/** @return List of external data required by the state tree */
	TConstArrayView<FStateTreeExternalDataDesc> GetExternalDataDescs() const
	{
		return ExternalDataDescs;
	}

	/** @return List of context data enforced by the schema that must be provided through the execution context. */
	TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const
	{
		return ContextDataDescs;
	}

	/** @return true if the other StateTree has compatible context data. */
	UE_API bool HasCompatibleContextData(const UStateTree& Other) const;
	
	/** @return List of default parameters of the state tree. Default parameter values can be overridden at runtime by the execution context. */
	const FInstancedPropertyBag& GetDefaultParameters() const
	{
		return Parameters;
	}

	/** @return true if the tree asset can be used at runtime. */
	UE_API bool IsReadyToRun() const;

	/** @return schema that was used to compile the StateTree. */
	const UStateTreeSchema* GetSchema() const
	{
		return Schema;
	}

	/** @return Pointer to a frame or null if frame not found */
	UE_API const FCompactStateTreeFrame* GetFrameFromHandle(const FStateTreeStateHandle StateHandle) const;

	/** @return Pointer to a state or null if state not found */ 
	UE_API const FCompactStateTreeState* GetStateFromHandle(const FStateTreeStateHandle StateHandle) const;

	/** @return State handle matching a given Id; invalid handle if state not found. */
	UE_API FStateTreeStateHandle GetStateHandleFromId(const FGuid Id) const;

	/** @return Id of the state matching a given state handle; invalid Id if state not found. */
	UE_API FGuid GetStateIdFromHandle(const FStateTreeStateHandle Handle) const;

	/** @return Struct view of the node matching a given node index; invalid view if state not found. */
	UE_API FConstStructView GetNode(const int32 NodeIndex) const;

	/** @return Struct views of all nodes */
	const FInstancedStructContainer& GetNodes() const { return Nodes; }

	/** @return Node index matching a given Id; invalid index if node not found. */
	UE_API FStateTreeIndex16 GetNodeIndexFromId(const FGuid Id) const;

	/** @return Id of the node matching a given node index; invalid Id if node not found. */
	UE_API FGuid GetNodeIdFromIndex(const FStateTreeIndex16 NodeIndex) const;

	/** @return View of all states. */
	TConstArrayView<FCompactStateTreeState> GetStates() const { return States; }

	/** @return Pointer to the transition at a given index; null if not found. */ 
	UE_API const FCompactStateTransition* GetTransitionFromIndex(const FStateTreeIndex16 TransitionIndex) const;
	
	/** @return Runtime transition index matching a given Id; invalid index if node not found. */
	UE_API FStateTreeIndex16 GetTransitionIndexFromId(const FGuid Id) const;

	/** @return Id of the transition matching a given runtime transition index; invalid Id if transition not found. */
	UE_API FGuid GetTransitionIdFromIndex(const FStateTreeIndex16 Index) const;

	/** @return Property bindings */
	const FStateTreePropertyBindings& GetPropertyBindings() const { return PropertyBindings; }

	/** @return True if there is any global tasks need ticking. */
	bool DoesRequestTickGlobalTasks(bool bHasEvents) const
	{
		return bCachedRequestGlobalTick || (bHasEvents && bCachedRequestGlobalTickOnlyOnEvents);
	}
	
	/** @return True if there is any global tasks that ticks. */
	bool ShouldTickGlobalTasks(bool bHasEvents) const
	{
		return bHasGlobalTickTasks || (bHasEvents && bHasGlobalTickTasksOnlyOnEvents);
	}

	/** @return true if the tree can use the scheduled tick feature. */
	bool IsScheduledTickAllowed() const
	{
		return bScheduledTickAllowed;
	}

#if WITH_EDITOR
	/** Resets the compiled data to empty. */
	UE_API void ResetCompiled();

	/** Calculates runtime memory usage for different sections of the tree. */
	UE_API TArray<FStateTreeMemoryUsage> CalculateEstimatedMemoryUsage() const;

	/** Called when the editor is preparing to start a pie session. */
	UE_API void OnPreBeginPIE(const bool bIsSimulating);

	/** Compile the state trees if the editor hash data as changed since the last compilation. */
	UE_API void CompileIfChanged();

#endif

#if WITH_EDITOR || WITH_STATETREE_DEBUG
	/** @return the internal content of the state tree compiled asset. */
	[[nodiscard]] UE_API FString DebugInternalLayoutAsString() const;
#endif

#if WITH_EDITORONLY_DATA
	/** Edit time data for the StateTree, instance of UStateTreeEditorData */
	UPROPERTY()
	TObjectPtr<UObject> EditorData;

	FDelegateHandle OnObjectsReinstancedHandle;
	FDelegateHandle OnUserDefinedStructReinstancedHandle;
	FDelegateHandle OnPreBeginPIEHandle;
#endif

	/** Hash of the editor data from last compile. Also used to detect mismatching events from recorded traces. */
	UPROPERTY()
	uint32 LastCompiledEditorDataHash = 0;

protected:
	
	/**
	 * Resolves references between data in the StateTree.
	 * @return true if all references to internal and external data are resolved properly, false otherwise.
	 */
	[[nodiscard]] UE_API bool Link();

	UE_API virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	static UE_API void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif
	UE_API virtual void Serialize(FStructuredArchiveRecord Record) override;
	
#if WITH_EDITOR
	using FReplacementObjectMap = TMap<UObject*, UObject*>;
	UE_API void OnObjectsReinstanced(const FReplacementObjectMap& ObjectMap);
	UE_API void OnUserDefinedStructReinstanced(const UUserDefinedStruct& UserDefinedStruct);
	UE_API virtual void PostInitProperties() override;
	UE_API virtual void BeginDestroy() override;
	UE_API virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	UE_API virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
protected:
	UE_API virtual void ThreadedPostLoadAssetRegistryTagsOverride(FPostLoadAssetRegistryTagsContext& Context) const override;
public:
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	static UE_API void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	
private:
	/** Compile the state trees. */
	UE_API void Compile();

	/**
	 * Reset the data generated by Link(), this in turn will cause IsReadyToRun() to return false.
	 * Used during linking, or to invalidate the linked data when data version is old (requires recompile). 
	 */
	UE_API void ResetLinked();

	/** @return true if all the source instance data types match with the node's instance data types */
	UE_API bool ValidateInstanceData();

	/** Set StateTree's flag that can't be set when compiled. */
	UE_API void UpdateRuntimeFlags();

	UE_API bool PatchBindings();

	// Data created during compilation, source data in EditorData.
	
	/** Schema used to compile the StateTree. */
	UPROPERTY(Instanced)
	TObjectPtr<UStateTreeSchema> Schema = nullptr;

	/** Runtime frames */
	UPROPERTY()
	TArray<FCompactStateTreeFrame> Frames;

	/** Runtime states, root state at index 0 */
	UPROPERTY()
	TArray<FCompactStateTreeState> States;

	/** Runtime transitions. */
	UPROPERTY()
	TArray<FCompactStateTransition> Transitions;

	/** Evaluators, Tasks, Condition and Consideration nodes. */
	UPROPERTY()
	FInstancedStructContainer Nodes;

	/** Default node instance data (e.g. evaluators, tasks). */
	UPROPERTY()
	FStateTreeInstanceData DefaultInstanceData;

	/** Shared node instance data (e.g. conditions, considerations). */
	UPROPERTY()
	FStateTreeInstanceData SharedInstanceData;

	mutable FTransactionallySafeRWLock PerThreadSharedInstanceDataLock;
	mutable TArray<TSharedPtr<FStateTreeInstanceData>> PerThreadSharedInstanceData;
	
	/** List of names external data enforced by the schema, created at compilation. */
	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;

	UPROPERTY()
	FStateTreePropertyBindings PropertyBindings;

	/** Mapping of state guid for the Editor and state handles, created at compilation. */
	UPROPERTY()
	TArray<FStateTreeStateIdToHandle> IDToStateMappings;

	/** Mapping of node guid for the Editor and node index, created at compilation. */
	UPROPERTY()
	TArray<FStateTreeNodeIdToIndex> IDToNodeMappings;
	
	/** Mapping of state transition identifiers and runtime compact transition index, created at compilation. */
	UPROPERTY()
	TArray<FStateTreeTransitionIdToIndex> IDToTransitionMappings;

	/**
	 * Parameters that could be used for bindings within the Tree.
	 * Default values are stored within the asset but StateTreeReference can be used to parameterized the tree.
	 * @see FStateTreeReference
	 */
	UPROPERTY()
	FInstancedPropertyBag Parameters;

	//~ Data created during linking.
	/** List of external data required by the state tree, created during linking. */
	UPROPERTY(Transient)
	TArray<FStateTreeExternalDataDesc> ExternalDataDescs;

	/** Mask used to test the global tasks completion. */
	UPROPERTY()
	uint32 CompletionGlobalTasksMask = 0;

	/** Number of context data, include parameters and all context data. */
	UPROPERTY()
	uint16 NumContextData = 0;

	/** Number of global instance data. */
	UPROPERTY()
	uint16 NumGlobalInstanceData = 0;
	
	/** Index of first evaluator in Nodes. */
	UPROPERTY()
	uint16 EvaluatorsBegin = 0;

	/** Number of evaluators. */
	UPROPERTY()
	uint16 EvaluatorsNum = 0;

	/** Index of first global task in Nodes. */
	UPROPERTY()
	uint16 GlobalTasksBegin = 0;

	/** Number of global tasks. */
	UPROPERTY()
	uint16 GlobalTasksNum = 0;

	/** How the global tasks control the completion of the frame. */
	UPROPERTY()
	EStateTreeTaskCompletionType CompletionGlobalTasksControl = EStateTreeTaskCompletionType::Any;

	/** The parameter data type used by the schema. */
	UPROPERTY()
	EStateTreeParameterDataType ParameterDataType = EStateTreeParameterDataType::GlobalParameterData;

	/** True if any global task is a transition task. */
	UPROPERTY()
	uint8 bHasGlobalTransitionTasks : 1 = false;

	/**
	 * True if any global task has bShouldCallTick.
	 * Not ticking implies no property copy.
	 */
	uint8 bHasGlobalTickTasks : 1 = false;

	/**
	 * True if any global task has bShouldCallTickOnlyOnEvents.
	 * No effect if bHasGlobalTickTasks is true.
	 * Not ticking implies no property copy.
	 */
	uint8 bHasGlobalTickTasksOnlyOnEvents : 1 = false;
	
	/** True if any global tasks request a tick every frame. */
	uint8 bCachedRequestGlobalTick : 1 = false;

	/**
	 * True if any global tasks request a tick every frame but only if there are events.
	 * No effect if bCachedRequestGlobalTick is true.
	 */
	uint8 bCachedRequestGlobalTickOnlyOnEvents : 1 = false;

	/** True when the scheduled tick is allowed by the schema. */
	uint8 bScheduledTickAllowed : 1 = false;

	/** True if the StateTree was linked successfully. */
	uint8 bIsLinked : 1 = false;

#if WITH_EDITORONLY_DATA
	/** List of Struct that are out of date and waiting to be replaced with the new instance. */
	TSet<FObjectKey> OutOfDateStructs;
#endif

	friend struct FStateTreeInstance;
	friend struct FStateTreeExecutionContext;
	friend struct FStateTreeTasksCompletionStatus;
	friend struct FStateTreeMinimalExecutionContext;
	friend struct FStateTreeReadOnlyExecutionContext;
	friend struct FStateTreeWeakExecutionContext;
	friend struct TStateTreeStrongExecutionContext<true>;

#if WITH_EDITORONLY_DATA
	friend struct FStateTreeCompiler;
#endif
};

#undef UE_API
