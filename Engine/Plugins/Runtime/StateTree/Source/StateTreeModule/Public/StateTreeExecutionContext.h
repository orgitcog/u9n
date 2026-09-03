// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTree.h"
#include "StateTreeExecutionExtension.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeNodeBase.h"
#include "StateTreeNodeRef.h"
#include "StateTreeReference.h"
#include "Debugger/StateTreeTrace.h"
#include "Experimental/ConcurrentLinearAllocator.h"
#include "Templates/IsInvocable.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "StateTreeAsyncExecutionContext.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6

#define UE_API STATETREEMODULE_API

struct FGameplayTag;
struct FInstancedPropertyBag;
struct FStateTreeExecutionContext;
struct FStateTreeEvaluatorBase;
struct FStateTreeTaskBase;
struct FStateTreeConditionBase;
struct FStateTreeDelegateDispatcher;
struct FStateTreeEvent;
struct FStateTreeMinimalExecutionContext;
struct FStateTreeTransitionRequest;
struct FStateTreeInstanceDebugId;
struct FStateTreeWeakExecutionContext;

namespace UE::StateTree::ExecutionContext
{
	UE_API bool MarkDelegateAsBroadcasted(FStateTreeDelegateDispatcher Dispatcher, const FStateTreeExecutionFrame& CurrentFrame, FStateTreeInstanceStorage& Storage);
	UE_API EStateTreeRunStatus GetPriorityRunStatus(EStateTreeRunStatus A, EStateTreeRunStatus B);
	UE_API UE::StateTree::ETaskCompletionStatus CastToTaskStatus(EStateTreeFinishTaskType FinishTask);
	UE_API EStateTreeRunStatus CastToRunStatus(EStateTreeFinishTaskType FinishTask);
	UE_API UE::StateTree::ETaskCompletionStatus CastToTaskStatus(EStateTreeRunStatus InStatus);
	UE_API EStateTreeRunStatus CastToRunStatus(UE::StateTree::ETaskCompletionStatus InStatus);
}
/**
 * Delegate used by the execution context to collect external data views for a given StateTree asset.
 * The caller is expected to iterate over the ExternalDataDescs array, find the matching external data,
 * and store it in the OutDataViews at the same index:
 *
 *	for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
 *	{
 *		const FStateTreeExternalDataDesc& Desc = ExternalDataDescs[Index];
 *		// Find data requested by Desc
 *		OutDataViews[Index] = ...;
 *	}
 */
DECLARE_DELEGATE_RetVal_FourParams(bool, FOnCollectStateTreeExternalData, const FStateTreeExecutionContext& /*Context*/, const UStateTree* /*StateTree*/, TArrayView<const FStateTreeExternalDataDesc> /*ExternalDataDescs*/, TArrayView<FStateTreeDataView> /*OutDataViews*/);

/**
 * Read-only execution context to interact with the state tree instance data. Only const and read accesses are available.
 * Multiple FStateTreeReadOnlyExecutionContext can coexist on different threads as long no other (minimal, weak, regular) execution context exists.
 * The user is responsible for preventing invalid multithread access.
 */
struct FStateTreeReadOnlyExecutionContext
{
public:
	UE_API explicit FStateTreeReadOnlyExecutionContext(TNotNull<UObject*> Owner, TNotNull<const UStateTree*> StateTree, FStateTreeInstanceData& InInstanceData);
	UE_API explicit FStateTreeReadOnlyExecutionContext(TNotNull<UObject*> Owner, TNotNull<const UStateTree*> StateTree, FStateTreeInstanceStorage& Storage);
	UE_API virtual ~FStateTreeReadOnlyExecutionContext();

private:
	FStateTreeReadOnlyExecutionContext(const FStateTreeReadOnlyExecutionContext&) = delete;
	FStateTreeReadOnlyExecutionContext& operator=(const FStateTreeReadOnlyExecutionContext&) = delete;

public:
	/**
	 * Indicates if the instance is valid and would be able to run the instance of the associated StateTree asset with a regular execution context.
	 * @return True if the StateTree asset assigned to the execution context is valid
	 * (i.e., not empty) and successfully initialized (i.e., linked and all bindings resolved).
	 */
	bool IsValid() const
	{
		return RootStateTree.IsReadyToRun();
	}

	/** @return The owner of the context */
	UObject* GetOwner() const
	{
		return &Owner;
	}

	/** @return The world of the owner or nullptr if the owner is not set. */
	UWorld* GetWorld() const
	{
		return Owner.GetWorld();
	}

	/** @return the StateTree asset in use by the instance. It is the root asset. */
	const UStateTree* GetStateTree() const
	{
		return &RootStateTree;
	}

	/** @return true if there is a pending event with specified tag. */
	bool HasEventToProcess(const FGameplayTag Tag) const
	{
		return Storage.GetEventQueue().GetEventsView().ContainsByPredicate([Tag](const FStateTreeSharedEvent& Event)
			{
				check(Event.IsValid());
				return Event->Tag.MatchesTag(Tag);
			});
	}

	/** @return Pointer to a State or null if state not found */
	const FCompactStateTreeState* GetStateFromHandle(const FStateTreeStateHandle StateHandle) const
	{
		return RootStateTree.GetStateFromHandle(StateHandle);
	}

	/** @return the delta time for the next execution context tick. */
	UE_API FStateTreeScheduledTick GetNextScheduledTick() const;

	/** @return the tree run status. */
	UE_API EStateTreeRunStatus GetStateTreeRunStatus() const;

	/** @return the status of the last tick function */
	UE_API EStateTreeRunStatus GetLastTickStatus() const;

	/** @return reference to the list of currently active frames and states. */
	UE_API TConstArrayView<FStateTreeExecutionFrame> GetActiveFrames() const;

	/** @return the name of the active state. */
	UE_API FString GetActiveStateName() const;

	/** @return the names of all the active state. */
	UE_API TArray<FName> GetActiveStateNames() const;

#if WITH_GAMEPLAY_DEBUGGER
	/** @return Debug string describing the current state of the execution */
	UE_API FString GetDebugInfoString() const;
#endif // WITH_GAMEPLAY_DEBUGGER

#if WITH_STATETREE_DEBUG
	UE_API int32 GetStateChangeCount() const;

	UE_API void DebugPrintInternalLayout();
#endif

protected:
	/** @return Prefix that will be used by STATETREE_LOG and STATETREE_CLOG, Owner name by default. */
	UE_API FString GetInstanceDescriptionInternal() const;

#if WITH_STATETREE_TRACE
	/** A unique Id used by debugging tools to identify the instance. */
	UE_API FStateTreeInstanceDebugId GetInstanceDebugId() const;
#endif

protected:
	/** Owner of the instance data. */
	UObject& Owner;

	/** The StateTree asset the context is initialized for */
	const UStateTree& RootStateTree;

	/** Data storage of the instance data. */
	FStateTreeInstanceStorage& Storage;
};

/**
 * Minimal execution context to interact with the state tree instance data.
 * A regular execution context requires the context data and external data to be valid to execute all possible operations.
 * The minimal execution context doesn't requires those data but supports only a subset of operations.
 */
struct FStateTreeMinimalExecutionContext : public FStateTreeReadOnlyExecutionContext
{
public:
	UE_DEPRECATED(5.6, "Use FStateTreeMinimalExecutionContext with the not null pointer.")
	UE_API explicit FStateTreeMinimalExecutionContext(UObject& Owner, const UStateTree& StateTree, FStateTreeInstanceData& InInstanceData);
	UE_DEPRECATED(5.6, "Use FStateTreeMinimalExecutionContext with the not null pointer.")
	UE_API explicit FStateTreeMinimalExecutionContext(UObject& Owner, const UStateTree& StateTree, FStateTreeInstanceStorage& Storage);
	UE_API explicit FStateTreeMinimalExecutionContext(TNotNull<UObject*> Owner, TNotNull<const UStateTree*> StateTree, FStateTreeInstanceData& InInstanceData);
	UE_API explicit FStateTreeMinimalExecutionContext(TNotNull<UObject*> Owner, TNotNull<const UStateTree*> StateTree, FStateTreeInstanceStorage& Storage);
	UE_API virtual ~FStateTreeMinimalExecutionContext();

private:
	FStateTreeMinimalExecutionContext(const FStateTreeMinimalExecutionContext&) = delete;
	FStateTreeMinimalExecutionContext& operator=(const FStateTreeMinimalExecutionContext&) = delete;

public:
	/** 
	 * Adds a scheduled tick request.
	 * The result of GetNextScheduledTick is affected by the request.
	 * This allows a specific task to control when the tree ticks.
	 * @note A request with a higher priority will supersede all other requests.
	 * ex: Task A request a custom time of 1FPS and Task B request a custom time of 2FPS. Both tasks will tick at 1FPS.
	 */
	UE_API UE::StateTree::FScheduledTickHandle AddScheduledTickRequest(FStateTreeScheduledTick ScheduledTick);

	/** Updates the scheduled tick of a previous request. */
	UE_API void UpdateScheduledTickRequest(UE::StateTree::FScheduledTickHandle Handle, FStateTreeScheduledTick ScheduledTick);

	/** Removes a scheduled tick request. */
	UE_API void RemoveScheduledTickRequest(UE::StateTree::FScheduledTickHandle Handle);

	/** Sends event for the StateTree. */
	UE_API void SendEvent(const FGameplayTag Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName());

#if WITH_STATETREE_TRACE
	void SetNodeCustomDebugTraceData(UE::StateTreeTrace::FNodeCustomDebugData&& DebugData)
	{
		ensureMsgf(!NodeCustomDebugTraceData .IsSet()
			, TEXT("CustomData is not expected to be already set."
				" This might indicate nested calls to SetNodeCustomDebugTraceData without calls to a trace macro"));
		NodeCustomDebugTraceData = MoveTemp(DebugData);
	}
#endif //WITH_STATETREE_TRACE

protected:
#if WITH_STATETREE_TRACE
	UE::StateTreeTrace::FNodeCustomDebugData StealNodeCustomDebugTraceData()
	{
		return MoveTemp(NodeCustomDebugTraceData);
	}

	UE::StateTreeTrace::FNodeCustomDebugData NodeCustomDebugTraceData;
#endif // WITH_STATETREE_TRACE

	/** Informs the owner when the instance of the tree must woke up from a scheduled tick sleep. */
	UE_API void ScheduleNextTick();

protected:
	/** The context is processing the tree. We do not need to inform the owner that something changed. */
	bool bAllowedToScheduleNextTick = true;
};

/**
 * StateTree Execution Context is a helper that is used to update StateTree instance data.
 *
 * The context is meant to be temporary, you should not store a context across multiple frames.
 *
 * The owner is used as the owner of the instantiated UObjects in the instance data and logging,
 * it should have same or greater lifetime as the InstanceData. 
 *
 * In common case you can use the constructor to initialize the context, and us a helper struct
 * to set up the context data and external data getter:
 *
 *		FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
 *		if (SetContextRequirements(Context))
 *		{
 *			Context.Tick(DeltaTime);
 * 		}
 *
 * 
 *		bool UMyComponent::SetContextRequirements(FStateTreeExecutionContext& Context)
 *		{
 *			if (!Context.IsValid())
 *			{
 *				return false;
 *			}
 *			// Setup context data
 *			Context.SetContextDataByName(...);
 *			...
 *
 *			Context.SetCollectExternalDataCallback(FOnCollectStateTreeExternalData::CreateUObject(this, &UMyComponent::CollectExternalData);
 *
 *			return Context.AreContextDataViewsValid();
 *		}
 *
 *		bool UMyComponent::CollectExternalData(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs, TArrayView<FStateTreeDataView> OutDataViews)
 *		{
 *			...
 *			for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
 *			{
 *				const FStateTreeExternalDataDesc& Desc = ExternalDataDescs[Index];
 *				if (Desc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
 *				{
 *					UWorldSubsystem* Subsystem = World->GetSubsystemBase(Cast<UClass>(const_cast<UStruct*>(Desc.Struct.Get())));
 *					OutDataViews[Index] = FStateTreeDataView(Subsystem);
 *				}
 *				...
 *			}
 *			return true;
 *		}
 *
 * In this example the SetContextRequirements() method is used to set the context defined in the schema,
 * and the delegate FOnCollectStateTreeExternalData is used to query the external data required by the tasks and conditions.
 *
 * In case the State Tree links to other state tree assets, the collect external data might get called
 * multiple times, once for each asset.
 */
struct FStateTreeExecutionContext : public FStateTreeMinimalExecutionContext
{
public:
	UE_API FStateTreeExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData, const FOnCollectStateTreeExternalData& CollectExternalDataCallback = {}, const EStateTreeRecordTransitions RecordTransitions = EStateTreeRecordTransitions::No);
	/** Construct an execution context from a parent context and another tree. Useful to run a subtree from the parent context with the same schema. */
	UE_API FStateTreeExecutionContext(const FStateTreeExecutionContext& InContextToCopy, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData);
	UE_API FStateTreeExecutionContext(TNotNull<UObject*> Owner, TNotNull<const UStateTree*> StateTree, FStateTreeInstanceData& InInstanceData, const FOnCollectStateTreeExternalData& CollectExternalDataCallback = {}, const EStateTreeRecordTransitions RecordTransitions = EStateTreeRecordTransitions::No);
	/** Construct an execution context from a parent context and another tree. Useful to run a subtree from the parent context with the same schema. */
	UE_API FStateTreeExecutionContext(const FStateTreeExecutionContext& InContextToCopy, TNotNull<const UStateTree*> StateTree, FStateTreeInstanceData& InInstanceData);
	UE_API virtual ~FStateTreeExecutionContext();

private:
	FStateTreeExecutionContext(const FStateTreeExecutionContext&) = delete;
	FStateTreeExecutionContext& operator=(const FStateTreeExecutionContext&) = delete;

public:
	/** Sets callback used to collect external data views during State Tree execution. */
	UE_API void SetCollectExternalDataCallback(const FOnCollectStateTreeExternalData& Callback);

	UE_DEPRECATED(5.6, "Use SetLinkedStateTreeOverrides that creates a copy.")
	/**
	 * Overrides for linked State Trees. This table is used to override State Tree references on linked states.
	 * If a linked state's tag is exact match of the tag specified on the table, the reference from the table is used instead.
	 */
	UE_API void SetLinkedStateTreeOverrides(const FStateTreeReferenceOverrides* InLinkedStateTreeOverrides);

	/**
	 * Overrides for linked State Trees. This table is used to override State Tree references on linked states.
	 * If a linked state's tag is exact match of the tag specified on the table, the reference from the table is used instead.
	 */
	UE_API void SetLinkedStateTreeOverrides(FStateTreeReferenceOverrides InLinkedStateTreeOverrides);

	/** @return the first state tree reference set by SetLinkedStateTreeOverrides that matches the StateTag. Or null if not found. */
	UE_API const FStateTreeReference* GetLinkedStateTreeOverrideForTag(const FGameplayTag StateTag) const;

	/** Structure to-be-populated and set for any StateTree using any EStateTreeDataSourceType::ExternalGlobalParameterData bindings */
	struct FExternalGlobalParameters
	{
		/* Add memory mapping, this expects InParameterMemory to resolve correctly for the SourceLeafProperty and SourceIndirection */
		UE_API bool Add(const FPropertyBindingCopyInfo& Copy, uint8* InParameterMemory);
		UE_API uint8* Find(const FPropertyBindingCopyInfo& Copy) const;
		UE_API void Reset();
	private:
		TMap<uint32, uint8*> Mappings;
	};
	UE_API void SetExternalGlobalParameters(const FExternalGlobalParameters* Parameters);

	/** @return const references to the instance data in use, or nullptr if the context is not valid. */
	const FStateTreeInstanceData* GetInstanceData() const
	{
		return &InstanceData;
	}

	/** @return mutable references to the instance data in use, or nullptr if the context is not valid. */
	FStateTreeInstanceData* GetMutableInstanceData() const
	{
		return &InstanceData;
	}

	/** @return mutable references to the instance data in use. */
	const FStateTreeEventQueue& GetEventQueue() const
	{
		return InstanceData.GetEventQueue();
	}

	/** @return mutable references to the instance data in use. */
	FStateTreeEventQueue& GetMutableEventQueue() const
	{
		return InstanceData.GetMutableEventQueue();
	}

	/** @return a weak context to interact with the state tree instance data that can be stored for later uses. */
	UE_API FStateTreeWeakExecutionContext MakeWeakExecutionContext() const;
	
	/**
	 * @return a weak reference for a task that can be stored for later uses.
	 * @note similar to GetInstanceData, the node needs to be the current processing node.
	 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "FStateTreeWeakTaskRef is no longer used.")
	UE_API FStateTreeWeakTaskRef MakeWeakTaskRef(const FStateTreeTaskBase& Node) const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * @return a weak reference for a task that can be stored for later uses.
	 * @note similar to GetInstanceData, the instance data needs to be the current processing node.
	 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	template <typename T>
	UE_DEPRECATED(5.6, "FStateTreeWeakTaskRef is no longer used.")
	FStateTreeWeakTaskRef MakeWeakTaskRefFromInstanceData(const T& InInstanceData) const
	{
		check(&CurrentNodeInstanceData.template GetMutable<T>() == &InInstanceData);
		return MakeWeakTaskRefInternal();
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * @return a weak reference for a task that can be stored for later uses.
	 * @note similar to GetInstanceData, the instance data needs to be the current processing node.
	 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	template <typename T>
	UE_DEPRECATED(5.6, "FStateTreeWeakTaskRef is no longer used.")
	FStateTreeWeakTaskRef MakeWeakTaskRefFromInstanceDataPtr(const T* InInstanceData) const
	{
		check(CurrentNodeInstanceData.template GetMutablePtr<T>() == InInstanceData);
		return MakeWeakTaskRefInternal();
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	struct FStartParameters
	{
		/** Optional override of parameters initial values. */
		const FInstancedPropertyBag* GlobalParameters = nullptr;

		/** Optional extension for the execution context. */
		TInstancedStruct<FStateTreeExecutionExtension> ExecutionExtension;

		/** Optional event queue from another instance data. Marks the event queue not owned. */
		const TSharedPtr<FStateTreeEventQueue> SharedEventQueue;

		/** Optional override of initial seed for RandomStream. By default FPlatformTime::Cycles() will be used. */
		TOptional<int32> RandomSeed;
	};

	/**
	 * Start executing.
	 * @param InitialParameters Optional override of parameters initial values
	 * @param RandomSeed Optional override of initial seed for RandomStream. By default FPlatformTime::Cycles() will be used.
	 * @return Tree execution status after the start.
	 */
	UE_API EStateTreeRunStatus Start(const FInstancedPropertyBag* InitialParameters = nullptr, int32 RandomSeed = -1);
	
	/**
	 * Start executing.
	 * @return Tree execution status after the start.
	 */
	UE_API EStateTreeRunStatus Start(FStartParameters Parameter);
	
	/**
	 * Stop executing if the tree is running.
	 * @param CompletionStatus Status (and terminal state) reported in the transition when the tree is stopped.
	 * @return Tree execution status at stop, can be CompletionStatus, or earlier status if the tree is not running. 
	 */
	UE_API EStateTreeRunStatus Stop(const EStateTreeRunStatus CompletionStatus = EStateTreeRunStatus::Stopped);

	/**
	 * Tick the state tree logic, updates the tasks and triggers transitions.
	 * @param DeltaTime time to advance the logic.
	 * @returns tree run status after the tick.
	 */
	UE_API EStateTreeRunStatus Tick(const float DeltaTime);

	/**
	 * Tick the state tree logic partially, updates the tasks.
	 * For full update TickTriggerTransitions() should be called after.
	 * @param DeltaTime time to advance the logic.
	 * @returns tree run status after the partial tick.
	 */
	UE_API EStateTreeRunStatus TickUpdateTasks(const float DeltaTime);
	
	/**
	 * Tick the state tree logic partially, triggers the transitions.
	 * For full update TickUpdateTasks() should be called before.
	 * @returns tree run status after the partial tick.
	 */
	UE_API EStateTreeRunStatus TickTriggerTransitions();

	/**
	 * Broadcasts the delegate.
	 * It executes bound delegates immediately and triggers bound transitions (when transitions are evaluated).
	 */
	UE_API void BroadcastDelegate(const FStateTreeDelegateDispatcher& Dispatcher);

	UE_DEPRECATED(5.6, "Use BindDelegate")
	/** Adds delegate listener. */
	UE_API bool AddDelegateListener(const FStateTreeDelegateListener& Listener, FSimpleDelegate Delegate);

	/**
	 * Registers the delegate to the listener.
	 * If the listener was previously registered, then unregister it first before registering it again with the new delegate callback.
	 * The listener is bound to a dispatcher in the editor.
	 */
	UE_API void BindDelegate(const FStateTreeDelegateListener& Listener, FSimpleDelegate Delegate);

	UE_DEPRECATED(5.6, "Use UnbindDelegate")
	/** Removes delegate listener. */
	UE_API void RemoveDelegateListener(const FStateTreeDelegateListener& Listener);

	/** Unregisters the callback bound to the listener. */
	UE_API void UnbindDelegate(const FStateTreeDelegateListener& Listener);

	/**
	 * Iterates over all events.
	 * @param Function a lambda which takes const FStateTreeSharedEvent& Event, and returns EStateTreeLoopEvents.
	 */
	template<typename TFunc>
	typename TEnableIf<TIsInvocable<TFunc, FStateTreeSharedEvent>::Value, void>::Type ForEachEvent(TFunc&& Function) const
	{
		if (!EventQueue)
		{
			return;
		}
		EventQueue->ForEachEvent(Function);
	}

	/**
	 * Iterates over all events.
	 * @param Function a lambda which takes const FStateTreeEvent& Event, and returns EStateTreeLoopEvents.
	 * Less preferable than FStateTreeSharedEvent version.
	 */
	template<typename TFunc>
	typename TEnableIf<TIsInvocable<TFunc, FStateTreeEvent>::Value, void>::Type ForEachEvent(TFunc&& Function) const
	{
		if (!EventQueue)
		{
			return;
		}
		EventQueue->ForEachEvent([Function](const FStateTreeSharedEvent& Event)
		{
			return Function(*Event);
		});
	}

	/** @return events to process this tick. */
	TArrayView<FStateTreeSharedEvent> GetMutableEventsToProcessView()
	{
		return EventQueue ? EventQueue->GetMutableEventsView() : TArrayView<FStateTreeSharedEvent>();
	}

	/** @return events to process this tick. */
	TConstArrayView<FStateTreeSharedEvent> GetEventsToProcessView() const
	{
		return EventQueue ? EventQueue->GetMutableEventsView() : TArrayView<FStateTreeSharedEvent>();
	}

	/** Consumes and removes the specified event from the event queue. */
	void ConsumeEvent(const FStateTreeSharedEvent& Event)
	{
		if (EventQueue)
		{
			EventQueue->ConsumeEvent(Event);
		}
	}

	UE_DEPRECATED(5.5, "Use GetEventsToProcessView() instead.")
	TConstArrayView<FStateTreeEvent> GetEventsToProcess() const { return {}; }

	FStateTreeIndex16 GetCurrentlyProcessedNodeIndex() const
	{
		return FStateTreeIndex16(CurrentNodeIndex);
	}

	FStateTreeDataHandle GetCurrentlyProcessedNodeInstanceData() const
	{
		return CurrentNodeDataHandle;
	}

	UE_DEPRECATED(5.6, "Use GetCurrentlyProcessedNodeInstanceData() instead.")
	/** @return the currently processed node if applicable. */
	FStateTreeDataHandle GetCurrentlyProcessedNode() const
	{
		return CurrentNodeDataHandle;
	}

	/** @return the currently processed state if applicable. */
	FStateTreeStateHandle GetCurrentlyProcessedState() const
	{
		return CurrentlyProcessedState;
	}

	/** @return the currently processed execution frame if applicable. */
	const FStateTreeExecutionFrame* GetCurrentlyProcessedFrame() const
	{
		return CurrentlyProcessedFrame;
	}

	/** @return the currently processed execution parent frame if applicable. */
	const FStateTreeExecutionFrame* GetCurrentlyProcessedParentFrame() const
	{
		return CurrentlyProcessedParentFrame;
	}

	/** @return Array view to named external data descriptors associated with this context. Note: Init() must be called before calling this method. */
	TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const
	{
		return RootStateTree.GetContextDataDescs();
	}

	/** Sets context data view value for specific item. */
	void SetContextData(const FStateTreeExternalDataHandle Handle, FStateTreeDataView DataView)
	{
		check(Handle.IsValid());
		check(Handle.DataHandle.GetSource() == EStateTreeDataSourceType::ContextData);
		ContextAndExternalDataViews[Handle.DataHandle.GetIndex()] = DataView;
	}

	/** Sets the context data based on name (name is defined in the schema), returns true if data was found */
	UE_API bool SetContextDataByName(const FName Name, FStateTreeDataView DataView);

	/** @return the context data based on name (name is defined in the schema) */
	UE_API FStateTreeDataView GetContextDataByName(const FName Name) const;

	/** @return True if all context data pointers are set. */ 
	UE_API bool AreContextDataViewsValid() const;

	/**
	 * Returns reference to external data based on provided handle. The return type is deduced from the handle's template type.
     * @param Handle Valid TStateTreeExternalDataHandle<> handle. 
	 * @return reference to external data based on handle or null if data is not set.
	 */ 
	template <typename T>
	typename T::DataType& GetExternalData(const T Handle) const
	{
		check(Handle.IsValid());
		check(Handle.DataHandle.GetSource() == EStateTreeDataSourceType::ExternalData);
		check(CurrentlyProcessedFrame);
		check(CurrentlyProcessedFrame->StateTree->ExternalDataDescs[Handle.DataHandle.GetIndex()].Requirement != EStateTreeExternalDataRequirement::Optional); // Optionals should query pointer instead.
		return ContextAndExternalDataViews[CurrentlyProcessedFrame->ExternalDataBaseIndex.Get() + Handle.DataHandle.GetIndex()].template GetMutable<typename T::DataType>();
	}

	/**
	 * Returns pointer to external data based on provided item handle. The return type is deduced from the handle's template type.
     * @param Handle Valid TStateTreeExternalDataHandle<> handle.
	 * @return pointer to external data based on handle or null if item is not set or handle is invalid.
	 */ 
	template <typename T>
	typename T::DataType* GetExternalDataPtr(const T Handle) const
	{
		if (Handle.IsValid())
		{
			check(CurrentlyProcessedFrame);
			check(Handle.DataHandle.GetSource() == EStateTreeDataSourceType::ExternalData);
			return ContextAndExternalDataViews[CurrentlyProcessedFrame->ExternalDataBaseIndex.Get() + Handle.DataHandle.GetIndex()].template GetMutablePtr<typename T::DataType>();
		}
		return nullptr;
	}

	FStateTreeDataView GetExternalDataView(const FStateTreeExternalDataHandle Handle)
	{
		if (Handle.IsValid())
		{
			check(CurrentlyProcessedFrame);
			check(Handle.DataHandle.GetSource() == EStateTreeDataSourceType::ExternalData);
			return ContextAndExternalDataViews[CurrentlyProcessedFrame->ExternalDataBaseIndex.Get() + Handle.DataHandle.GetIndex()];
		}
		return FStateTreeDataView();
	}

	/** @returns pointer to the instance data of specified node. */
	template <typename T>
	T* GetInstanceDataPtr(const FStateTreeNodeBase& Node) const
	{
		check(CurrentNodeDataHandle == Node.InstanceDataHandle);
		return CurrentNodeInstanceData.template GetMutablePtr<T>();
	}

	/** @returns reference to the instance data of specified node. */
	template <typename T>
	T& GetInstanceData(const FStateTreeNodeBase& Node) const
	{
		check(CurrentNodeDataHandle == Node.InstanceDataHandle);
		return CurrentNodeInstanceData.template GetMutable<T>();
	}

	/** @returns reference to the instance data of specified node. Infers the instance data type from the node's FInstanceDataType. */
	template <typename T>
	typename T::FInstanceDataType& GetInstanceData(const T& Node) const
	{
		static_assert(TIsDerivedFrom<T, FStateTreeNodeBase>::IsDerived, "Expecting Node to derive from FStateTreeNodeBase.");
		check(CurrentNodeDataHandle == Node.InstanceDataHandle);
		return CurrentNodeInstanceData.template GetMutable<typename T::FInstanceDataType>();
	}

	/** @returns reference to instance data struct that can be passed to lambdas. See TStateTreeInstanceDataStructRef for usage. */
	template <typename T>
	TStateTreeInstanceDataStructRef<typename T::FInstanceDataType> GetInstanceDataStructRef(const T& Node) const
	{
		static_assert(TIsDerivedFrom<T, FStateTreeNodeBase>::IsDerived, "Expecting Node to derive from FStateTreeNodeBase.");
		check(CurrentlyProcessedFrame);
		return TStateTreeInstanceDataStructRef<typename T::FInstanceDataType>(InstanceData, *CurrentlyProcessedFrame, Node.InstanceDataHandle);
	}

	/**
	 * Requests transition to a state.
	 * If called during during transition processing (e.g. from FStateTreeTaskBase::TriggerTransitions()) the transition
	 * is attempted to be activate immediately (it can fail e.g. because of preconditions on a target state).
	 * If called outside the transition handling, the request is buffered and handled at the beginning of next transition processing.
	 * @param Request The state to transition to.
	 */
	UE_API void RequestTransition(const FStateTreeTransitionRequest& Request);

	/**
	 * Requests transition to a state.
	 * If called during during transition processing (e.g. from FStateTreeTaskBase::TriggerTransitions()) the transition
	 * is attempted to be activate immediately (it can fail e.g. because of preconditions on a target state).
	 * If called outside the transition handling, the request is buffered and handled at the beginning of next transition processing.
	 * @param TargetState The state to transition to.
	 * @param Priority The priority of the transition.
	 * @param Fallback of the transition if it fails to select the target state.
	 */
	UE_API void RequestTransition(FStateTreeStateHandle TargetState, EStateTreeTransitionPriority Priority = EStateTreeTransitionPriority::Normal, EStateTreeSelectionFallback Fallback = EStateTreeSelectionFallback::None);

	/**
	 * Finishes a task. This fails if the Task is not currently the processed node.
	 * ie. Must be called from inside a FStateTreeTaskBase EnterState, ExitState, StateCompleted, Tick, TriggerTransitions.
	 * If called during tick processing, then the state completes immediately.
	 * If called outside of the tick processing, then the request is buffered and handled on the next tick.
	 */
	UE_API void FinishTask(const FStateTreeTaskBase& Task, EStateTreeFinishTaskType FinishType);
	
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use the weak context to finish a task async or FinishTask(FStateTreeTaskBase, EStateTreeFinishTaskType) to finish the current task.")
	/**
	 * Finishes a task.
	 * If called during tick processing, then the state completes immediately.
	 * If called outside of the tick processing, then the request is buffered and handled on the next tick.
	 */
	UE_API void FinishTask(const UE::StateTree::FFinishedTask& Task, EStateTreeFinishTaskType FinishType);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** @return data view of the specified handle relative to given frame. */
	UE_DEPRECATED(5.6, "Use UE::StateTree::InstanceData::GetDataView instead.")
	static FStateTreeDataView GetDataViewFromInstanceStorage(FStateTreeInstanceStorage& InstanceDataStorage, FStateTreeInstanceStorage* CurrentlyProcessedSharedInstanceStorage, const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle)
	{
		return UE::StateTree::InstanceData::GetDataView(InstanceDataStorage, CurrentlyProcessedSharedInstanceStorage, ParentFrame, CurrentFrame, Handle);
	}

	/** Looks for a frame in provided list of frames. */
	static UE_API const FStateTreeExecutionFrame* FindFrame(const UStateTree* StateTree, FStateTreeStateHandle RootState, TConstArrayView<FStateTreeExecutionFrame> Frames, const FStateTreeExecutionFrame*& OutParentFrame);

	UE_DEPRECATED(5.5, "Use UE::StateTree::InstanceData::GetDataView instead.")
	static FStateTreeDataView GetDataView(FStateTreeInstanceStorage& InstanceDataStorage, FStateTreeInstanceStorage* CurrentlyProcessedSharedInstanceStorage, const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, TConstArrayView<FStateTreeDataView> ContextAndExternalDataViews, const FStateTreeDataHandle Handle)
	{
		return UE::StateTree::InstanceData::GetDataView(InstanceDataStorage, CurrentlyProcessedSharedInstanceStorage, ParentFrame, CurrentFrame, Handle);
	}

	/**
	* Forces transition to a state from a previously recorded state tree transition result.
	* Primarily used for replication purposes so that a client state tree stay in sync with its server counterpart.
	* @param Recorded state transition to run on the state tree.
	* @return The new run status for the state tree.
	*/
	UE_API EStateTreeRunStatus ForceTransition(const FRecordedStateTreeTransitionResult& Transition);

	/** Returns the recorded transitions for this context. */
	TConstArrayView<FRecordedStateTreeTransitionResult> GetRecordedTransitions() const { return RecordedTransitions; }

protected:
	/**
	 * Describes a result of States Selection.
	 */
	struct FStateSelectionResult
	{
		/** Max number of execution frames handled during state selection. */
		static constexpr int32 MaxExecutionFrames = 8;

		FStateSelectionResult() = default;
		explicit FStateSelectionResult(TConstArrayView<FStateTreeExecutionFrame> InFrames)
		{
			SelectedFrames = InFrames;
			FramesStateSelectionEvents.SetNum(SelectedFrames.Num());
		}

		bool IsFull() const
		{
			return SelectedFrames.Num() == MaxExecutionFrames;
		}

		void PushFrame(FStateTreeExecutionFrame Frame)
		{
			SelectedFrames.Add(Frame);
			FramesStateSelectionEvents.AddDefaulted();
		}

		void PopFrame()
		{
			SelectedFrames.Pop();
			FramesStateSelectionEvents.Pop();
		}

		bool ContainsFrames() const
		{
			return !SelectedFrames.IsEmpty();
		}

		int32 FramesNum() const
		{
			return SelectedFrames.Num();
		}

		TArrayView<FStateTreeExecutionFrame> GetSelectedFrames()
		{
			return SelectedFrames;
		}

		TConstArrayView<FStateTreeExecutionFrame> GetSelectedFrames() const
		{
			return SelectedFrames;
		}

		TArrayView<FStateTreeFrameStateSelectionEvents> GetFramesStateSelectionEvents()
		{
			return FramesStateSelectionEvents;
		}

	protected:
		TArray<FStateTreeExecutionFrame, TFixedAllocator<MaxExecutionFrames>> SelectedFrames;
		TArray<FStateTreeFrameStateSelectionEvents, TFixedAllocator<MaxExecutionFrames>> FramesStateSelectionEvents;
	};

	/** @return Prefix that will be used by STATETREE_LOG and STATETREE_CLOG, Owner name by default. */
	UE_DEPRECATED(5.6, "Use FStateTreeExecutionExtension::GetInstanceDescription instead")
	UE_API virtual FString GetInstanceDescription() const final;

	/** Callback when delayed transition is triggered. Contexts that are event based can use this to trigger a future event. */
	virtual void BeginDelayedTransition(const FStateTreeTransitionDelayedState& DelayedState) {};

	UE_API void UpdateInstanceData(TConstArrayView<FStateTreeExecutionFrame> CurrentActiveFrames, TArrayView<FStateTreeExecutionFrame> NextActiveFrames);

	/**
	 * Handles logic for entering State. EnterState is called on new active Evaluators and Tasks that are part of the re-planned tree.
	 * Re-planned tree is from the transition target up to the leaf state. States that are parent to the transition target state
	 * and still active after the transition will remain intact.
	 * @return Run status returned by the tasks.
	 */
	UE_API EStateTreeRunStatus EnterState(FStateTreeTransitionResult& Transition);

	/**
	 * Handles logic for exiting State. ExitState is called on current active Evaluators and Tasks that are part of the re-planned tree.
	 * Re-planned tree is from the transition target up to the leaf state. States that are parent to the transition target state
	 * and still active after the transition will remain intact.
	 */
	UE_API void ExitState(const FStateTreeTransitionResult& Transition);

	/**
	 * Removes all delegate listeners.
	 */
	UE_API void RemoveAllDelegateListeners();

	/**
	 * Handles logic for signaling State completed. StateCompleted is called on current active Evaluators and Tasks in reverse order (from leaf to root).
	 */
	UE_API void StateCompleted();

	/**
	 * Tick evaluators and global tasks by delta time.
	 */
	UE_API EStateTreeRunStatus TickEvaluatorsAndGlobalTasks(const float DeltaTime, bool bTickGlobalTasks = true);

	/**
	 * Tick evaluators and global tasks of a specific frame.
	 */
	UE_API EStateTreeRunStatus TickEvaluatorsAndGlobalTasksForFrame(const float DeltaTime, const bool bTickGlobalTasks, const int32 FrameIndex, const FStateTreeExecutionFrame* CurrentParentFrame, const TNotNull<FStateTreeExecutionFrame*> CurrentFrame);

	/**
	 * Starts evaluators and global tasks.
	 * @return run status returned by the global tasks.
	 */
	UE_API EStateTreeRunStatus StartEvaluatorsAndGlobalTasks(FStateTreeIndex16& OutLastInitializedTaskIndex);

	/**
	 * Stops evaluators and global tasks.
	 */
	UE_API void StopEvaluatorsAndGlobalTasks(const EStateTreeRunStatus CompletionStatus, const FStateTreeIndex16 LastInitializedTaskIndex = FStateTreeIndex16());

	/**
	 * Stops evaluators and global tasks of the given frame. Expects nodes data to be already bound.
	 */
	UE_API void CallStopOnEvaluatorsAndGlobalTasks(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& Frame, const FStateTreeTransitionResult& Transition, const FStateTreeIndex16 LastInitializedTaskIndex = FStateTreeIndex16());

	/** Starts temporary instances of global evaluators and tasks for a given frame. */
	UE_API EStateTreeRunStatus StartTemporaryEvaluatorsAndGlobalTasks(const FStateTreeExecutionFrame* CurrentParentFrame, FStateTreeExecutionFrame& CurrentFrame);

	UE_DEPRECATED(5.6, "Use the non const version of StartTemporaryEvaluatorsAndGlobalTasks")
	EStateTreeRunStatus StartTemporaryEvaluatorsAndGlobalTasks(const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame)
	{
		return StartTemporaryEvaluatorsAndGlobalTasks(CurrentParentFrame, const_cast<FStateTreeExecutionFrame&>(CurrentFrame));
	}

	/** Stops temporary global evaluators and tasks for the provided frame. */
	UE_API void StopTemporaryEvaluatorsAndGlobalTasks(const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame);

	/**
	 * Ticks tasks of all active states starting from current state by delta time.
	 * @return Run status returned by the tasks.
	 */
	UE_API EStateTreeRunStatus TickTasks(const float DeltaTime);

	struct FTickTaskResult
	{
		bool bShouldTickTasks = true;
	};
	struct FTickTaskArguments
	{
		FTickTaskArguments() = default;
		float DeltaTime = 0.f;
		int32 TasksBegin = 0;
		int32 TasksNum = 0;
		int32 Indent = 0;
		const FStateTreeExecutionFrame* ParentFrame = nullptr;
		FStateTreeExecutionFrame* Frame = nullptr;
		UE::StateTree::FActiveStateID StateID;
		UE::StateTree::FTasksCompletionStatus* TasksCompletionStatus = nullptr;
		bool bIsGlobalTasks = false;
		bool bShouldTickTasks = true;
	};
	/** Ticks tasks and updates the bindings for a specific state or frame. */
	UE_API FTickTaskResult TickTasks(const FTickTaskArguments& Args);

	/** Common functionality shared by the tick methods. */
	UE_API EStateTreeRunStatus TickPrelude();
	UE_API EStateTreeRunStatus TickPostlude();

	/** Handles task ticking part of the tick. */
	UE_API void TickUpdateTasksInternal(float DeltaTime);
	
	/** Handles transition triggering part of the tick. */
	UE_API void TickTriggerTransitionsInternal();

	/**
	 * Checks all conditions at given range
	 * @return True if all conditions pass.
	 */
	UE_API bool TestAllConditions(const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const int32 ConditionsOffset, const int32 ConditionsNum);

	/**
	 * Calculate the final score of all considerations at given range
	 * @return the final score
	 */
	UE_API float EvaluateUtility(const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const int32 ConsiderationsOffset, const int32 ConsiderationsNum, const float StateWeight);

	/* Evaluate all function at given range. Should be used only on active instances, assumes valid handles and does not consider temporary instances. */
	UE_API void EvaluatePropertyFunctionsOnActiveInstances(const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame, FStateTreeIndex16 FuncsBegin, uint16 FuncsNum);


	/* Evaluate all function at given range. This version validates the data handles and looks up temporary instances. */
	UE_API void EvaluatePropertyFunctionsWithValidation(const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame, FStateTreeIndex16 FuncsBegin, uint16 FuncsNum);

	/**
	 * Requests transition to a specified state with specified priority.
	 */
	UE_API bool RequestTransition(
		const FStateTreeExecutionFrame& CurrentFrame,
		const FStateTreeStateHandle NextState,
		const EStateTreeTransitionPriority Priority,
		const FStateTreeSharedEvent* TransitionEvent = nullptr,
		const EStateTreeSelectionFallback Fallback = EStateTreeSelectionFallback::None);

	/**
	 * Sets up NextTransition based on the provided parameters and the current execution status. 
	 */
	UE_API void SetupNextTransition(const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeStateHandle NextState, const EStateTreeTransitionPriority Priority);

	/**
	 * Triggers transitions based on current run status. CurrentStatus is used to select which transitions events are triggered.
	 * If CurrentStatus is "Running", "Conditional" transitions pass, "Completed/Failed" will trigger "OnCompleted/OnSucceeded/OnFailed" transitions.
	 * Transition target state can point to a selector state. For that reason the result contains both the target state, as well ass
	 * the actual next state returned by the selector.
	 * @return Transition result describing the source state, state transitioned to, and next selected state.
	 */
	UE_API bool TriggerTransitions();

	/** Create a new transition result from a recorded transition result. It will fail if the recorded transition is malformed. */
	UE_API TOptional<FStateTreeTransitionResult> MakeTransitionResult(const FRecordedStateTreeTransitionResult& Transition) const;
	/** Create a new recorded transition from a transition result. */
	UE_API FRecordedStateTreeTransitionResult MakeRecordedTransitionResult(const FStateTreeTransitionResult& Transition) const;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Replaced with FStateTreeTasksCompletionStatus")
	/** Confirms that the frame and state ID are valid and the task index is correct. */
	UE_API bool IsFinishedTaskValid(const UE::StateTree::FFinishedTask& Task) const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.6, "Replaced with FStateTreeTasksCompletionStatus")
	/**
	 * Removes stale entries and fills the completed states list.
	 * The tree can be in a different state from when a finished task was added to the pending list and the finished task may not be valid.
	 * @param bMarkTaskProcessed when true, marks the tasks as processed. When false, only used the already marked tasks.
	 */
	UE_API void UpdateCompletedStateList(bool bMarkTaskProcessed);

	UE_DEPRECATED(5.6, "Replaced with FStateTreeTasksCompletionStatus")
	UE_API void UpdateCompletedStateList();

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Replaced with FStateTreeTasksCompletionStatus")
	/** Adds the state to the completed list from a finished task. */
	UE_API void MarkStateCompleted(UE::StateTree::FFinishedTask& FinisedTask);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.6, "Replaced with FStateTreeTasksCompletionStatus")
	/** In the list of completed states, returns the run status when it's completed by a global task. */
	UE_API EStateTreeRunStatus GetGlobalTasksCompletedStatesStatus() const;

	/**
	 * Runs state selection logic starting at the specified state, walking towards the leaf states.
	 * If a state cannot be selected, false is returned. 
	 * If NextState is a selector state, SelectStateInternal is called recursively (depth-first) to all child states (where NextState will be one of child states).
	 * If NextState is a leaf state, the active states leading from root to the leaf are returned.
	 * @param CurrentFrame The frame where the NextState is valid. 
	 * @param NextState The state which we try to select next.
	 * @param OutSelectionResult The result of selection.
	 * @param TransitionEvent Event used by transition to trigger the state selection.
	 * @param Fallback selection behavior to execute if it fails to select the desired state
	 * @return True if succeeded to select new active states.
	 */
	UE_API bool SelectState(
		const FStateTreeExecutionFrame& CurrentFrame,
		const FStateTreeStateHandle NextState,
		FStateSelectionResult& OutSelectionResult,
		const FStateTreeSharedEvent* TransitionEvent = nullptr,
		const EStateTreeSelectionFallback Fallback = EStateTreeSelectionFallback::None);

	/**
	 * Used internally to do the recursive part of the SelectState().
	 */
	UE_API bool SelectStateInternal(
		const FStateTreeExecutionFrame* CurrentParentFrame,
		FStateTreeExecutionFrame& CurrentFrame,
		const FStateTreeExecutionFrame* CurrentFrameInActiveFrames,
		TConstArrayView<FStateTreeStateHandle> PathToNextState,
		FStateSelectionResult& OutSelectionResult,
		const FStateTreeSharedEvent* TransitionEvent = nullptr);

	/** @return StateTree execution state from the instance storage. */
	FStateTreeExecutionState& GetExecState()
	{
		return Storage.GetMutableExecutionState();
	}

	/** @return const StateTree execution state from the instance storage. */
	const FStateTreeExecutionState& GetExecState() const
	{
		return Storage.GetExecutionState();
	}

	/** Updates the update phase of the statetree execution state. */
	UE_API void SetUpdatePhaseInExecutionState(FStateTreeExecutionState& ExecutionState, EStateTreeUpdatePhase UpdatePhase) const;

	/** @return String describing state status for logging and debug. */
	UE_API FString GetStateStatusString(const FStateTreeExecutionState& ExecState) const;

	/** @return String describing state name for logging and debug. */
	UE_API FString GetSafeStateName(const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeStateHandle State) const;

	/** @return String describing full path of an activate state for logging and debug. */
	UE_API FString DebugGetStatePath(TConstArrayView<FStateTreeExecutionFrame> ActiveFrames, const FStateTreeExecutionFrame* CurrentFrame = nullptr, const int32 ActiveStateIndex = INDEX_NONE) const;

	/** @return String describing all events that are currently being processed  for logging and debug. */
	UE_API FString DebugGetEventsAsString() const;

	/** @return data view of the specified handle relative to given frame. */
	UE_API FStateTreeDataView GetDataView(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle);
	UE_API FStateTreeDataView GetDataView(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo);

	/** @return true if handle source is valid cified handle relative to given frame. */
	UE_API bool IsHandleSourceValid(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle) const;
	UE_API bool IsHandleSourceValid(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo) const;

	/** @return data view of the specified handle relative to the given frame, or tries to find a matching temporary instance. */
	UE_API FStateTreeDataView GetDataViewOrTemporary(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle);
	UE_API FStateTreeDataView GetDataViewOrTemporary(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo);

	/** @return data view of the specified handle from temporary instance. */
	UE_API FStateTreeDataView GetTemporaryDataView(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle);

	/**
	 * Adds a temporary instance that can be located using frame and data handle later.
	 * @returns view to the newly added instance. If NewInstanceData is Object wrapper, the new object is returned.
	 */
	UE_API FStateTreeDataView AddTemporaryInstance(const FStateTreeExecutionFrame& Frame, const FStateTreeIndex16 OwnerNodeIndex, const FStateTreeDataHandle DataHandle, FConstStructView NewInstanceData);

	/** Copies a batch of properties to the data in TargetView. Should be used only on active instances, assumes valid handles and does not consider temporary instances. */
	UE_API bool CopyBatchOnActiveInstances(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataView TargetView, const FStateTreeIndex16 BindingsBatch);

	/** Copies a batch of properties to the data in TargetView. This version validates the data handles and looks up temporary instances. */
	UE_API bool CopyBatchWithValidation(const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataView TargetView, const FStateTreeIndex16 BindingsBatch);

	/**
	 * Collects external data for all StateTrees in active frames.
	 * @returns true if all external data are set successfully. */
	UE_API bool CollectActiveExternalData();

	/**
	 * Collects external data for specific State Tree asset. If the data is already collected, cached index is returned.
	 * @returns index in ContextAndExternalDataViews for the first external data.
	 */
	UE_API FStateTreeIndex16 CollectExternalData(const UStateTree* StateTree);

	/**
	 * Stores copy of provided parameters as State Tree global parameters.
	 * @param Parameters parameters to copy
	 * @returns true if successfully set the parameters
	 */
	UE_API bool SetGlobalParameters(const FInstancedPropertyBag& Parameters);

	/**
	 * Captures StateTree events used during the state selection.
	 */
	UE_API void CaptureNewStateEvents(TConstArrayView<FStateTreeExecutionFrame> PrevFrames, TConstArrayView<FStateTreeExecutionFrame> NewFrames, TArrayView<FStateTreeFrameStateSelectionEvents> FramesStateSelectionEvents);

	/** @return a weak reference for a task that can be stored for later uses. */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "FStateTreeWeakTaskRef is no longer used.")
	UE_API FStateTreeWeakTaskRef MakeWeakTaskRefInternal() const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Instance data used during current tick. */
	FStateTreeInstanceData& InstanceData;

	UE_DEPRECATED(5.6, "Use Storage instead.")
	/** Data storage of the instance data, cached for less indirections. */
	FStateTreeInstanceStorage* InstanceDataStorage = nullptr;

	/** Events queue to use, cached for less indirections. */
	TSharedPtr<FStateTreeEventQueue> EventQueue;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.6, "Use the LinkedStateTreeOverrides copy")
	/** Pointer to linked state tree overrides. */
	const FStateTreeReferenceOverrides* LinkedStateTreeOverrides = nullptr;
#endif
	/** Current linked state tree overrides. */
	FStateTreeReferenceOverrides LinkedAssetStateTreeOverrides;
	
	/** Data view of the context data. */
	TArray<FStateTreeDataView, FNonconcurrentLinearArrayAllocator> ContextAndExternalDataViews;

	FOnCollectStateTreeExternalData CollectExternalDataDelegate;

	struct FCollectedExternalDataCache
	{
		const UStateTree* StateTree = nullptr;
		FStateTreeIndex16 BaseIndex;
	};
	TArray<FCollectedExternalDataCache, FNonconcurrentLinearArrayAllocator> CollectedExternalCache;

	bool bActiveExternalDataCollected = false;

#if WITH_EDITORONLY_DATA
	/** Run status set in FinishTask and that used after the task ticks. */
	UE_DEPRECATED(5.6, "CompletedStateRunStatus is not used.")
	TOptional<EStateTreeRunStatus> CompletedStateRunStatus;
#endif

	/** Next transition, used by RequestTransition(). */
	FStateTreeTransitionResult NextTransition;

	/** Structure describing the origin of the state transition that caused the state change. */
	FStateTreeTransitionSource NextTransitionSource;

	/** When set, start the transitions loop from TriggerTransitionsFromFrameIndex. */
	TOptional<int32> TriggerTransitionsFromFrameIndex;

	/** Current frame we're processing. */
	const FStateTreeExecutionFrame* CurrentlyProcessedParentFrame = nullptr; 
	const FStateTreeExecutionFrame* CurrentlyProcessedFrame = nullptr; 

	/** Pointer to the shared instance data of the current frame we're processing.  */
	FStateTreeInstanceStorage* CurrentlyProcessedSharedInstanceStorage = nullptr;

	/** Helper struct to track currently processed frame. */
	struct FCurrentlyProcessedFrameScope
	{
		FCurrentlyProcessedFrameScope(FStateTreeExecutionContext& InContext, const FStateTreeExecutionFrame* CurrentParentFrame, const FStateTreeExecutionFrame& CurrentFrame);
		~FCurrentlyProcessedFrameScope();

	private:
		FStateTreeExecutionContext& Context;
		int32 SavedFrameIndex = 0;
		FStateTreeInstanceStorage* SavedSharedInstanceDataStorage = nullptr;
		const FStateTreeExecutionFrame* SavedFrame = nullptr;
		const FStateTreeExecutionFrame* SavedParentFrame = nullptr;
	};

	/** Current state selection result when performing recursive state selection, or nullptr if not applicable. */
	const FStateSelectionResult* CurrentSelectionResult = nullptr;

	/** Current state we're processing, or invalid if not applicable. */
	FStateTreeStateHandle CurrentlyProcessedState;

	/** Helper struct to track currently processed state. */
	struct FCurrentlyProcessedStateScope
	{
		FCurrentlyProcessedStateScope(FStateTreeExecutionContext& InContext, const FStateTreeStateHandle State)
			: Context(InContext)
		{
			SavedState = Context.CurrentlyProcessedState;
			Context.CurrentlyProcessedState = State;
		}

		~FCurrentlyProcessedStateScope()
		{
			Context.CurrentlyProcessedState = SavedState;
		}

	private:
		FStateTreeExecutionContext& Context;
		FStateTreeStateHandle SavedState = FStateTreeStateHandle::Invalid;
	};

	/** Current event we're processing in transition, or invalid if not applicable. */
	const FStateTreeEvent* CurrentlyProcessedTransitionEvent = nullptr;

	/** Helper struct to track currently processed transition event. */
	struct FCurrentlyProcessedTransitionEventScope
	{
		FCurrentlyProcessedTransitionEventScope(FStateTreeExecutionContext& InContext, const FStateTreeEvent* Event)
			: Context(InContext)
		{
			check(Context.CurrentlyProcessedTransitionEvent == nullptr);
			Context.CurrentlyProcessedTransitionEvent = Event;
		}

		~FCurrentlyProcessedTransitionEventScope()
		{
			Context.CurrentlyProcessedTransitionEvent = nullptr;
		}

	private:
		FStateTreeExecutionContext& Context;
	};

	/** Current frames events we're processing during the state selection, or invalid if not applicable. */
	FStateTreeFrameStateSelectionEvents* CurrentlyProcessedStateSelectionEvents = nullptr;

	/** Helper struct to track currently processed state selection events. */
	struct FCurrentFrameStateSelectionEventsScope
	{
		FCurrentFrameStateSelectionEventsScope(FStateTreeExecutionContext& InContext, FStateTreeFrameStateSelectionEvents& InCurrentlyProcessedStateSelectionEvents)
			: Context(InContext)
		{
			SavedStateSelectionEvents = Context.CurrentlyProcessedStateSelectionEvents; 
			Context.CurrentlyProcessedStateSelectionEvents = &InCurrentlyProcessedStateSelectionEvents;
		}

		~FCurrentFrameStateSelectionEventsScope()
		{
			Context.CurrentlyProcessedStateSelectionEvents = SavedStateSelectionEvents;
		}

	private:
		FStateTreeExecutionContext& Context;
		FStateTreeFrameStateSelectionEvents* SavedStateSelectionEvents = nullptr;
	};

	/** True if transitions are allowed to be requested directly instead of buffering. */
	bool bAllowDirectTransitions = false;

	/** Helper struct to track when it is allowed to request transitions. */
	struct FAllowDirectTransitionsScope
	{
		FAllowDirectTransitionsScope(FStateTreeExecutionContext& InContext)
			: Context(InContext)
		{
			bSavedAllowDirectTransitions = Context.bAllowDirectTransitions; 
			Context.bAllowDirectTransitions = true;
		}

		~FAllowDirectTransitionsScope()
		{
			Context.bAllowDirectTransitions = bSavedAllowDirectTransitions;
		}

	private:
		FStateTreeExecutionContext& Context;
		bool bSavedAllowDirectTransitions = false;
	};

	/** Currently processed nodes instance data. Ideally we would pass these to the nodes directly, but do not want to change the API currently. */
	const FStateTreeNodeBase* CurrentNode = nullptr;
	int32 CurrentNodeIndex = FStateTreeIndex16::InvalidValue;
	FStateTreeDataHandle CurrentNodeDataHandle;
	FStateTreeDataView CurrentNodeInstanceData;

	/** Helper struct to set current node data. */
	struct FNodeInstanceDataScope
	{
		FNodeInstanceDataScope(FStateTreeExecutionContext& InContext, const FStateTreeNodeBase* InNode, const int32 InNodeIndex, const FStateTreeDataHandle InNodeDataHandle, const FStateTreeDataView InNodeInstanceData);
		~FNodeInstanceDataScope();

	private:
		FStateTreeExecutionContext& Context;
		const FStateTreeNodeBase* SavedNode = nullptr;
		int32 SavedNodeIndex = FStateTreeIndex16::InvalidValue;
		FStateTreeDataHandle SavedNodeDataHandle;
		FStateTreeDataView SavedNodeInstanceData;
	};

	/** If true, the state tree context will create snapshots of transition events and capture them within RecordedTransitions for later use. */
	bool bRecordTransitions = false;

	/** Captured snapshots for transition results that can be used to recreate transitions. This array is only populated if bRecordTransitions is true. */
	TArray<FRecordedStateTreeTransitionResult> RecordedTransitions;

	/** Memory mapping structure used for redirecting property-bag copies to external (raw) memory pointers */
	const FExternalGlobalParameters* ExternalGlobalParameters = nullptr;
};

/**
 * The const version of a StateTree Execution Context that prevents using the FStateTreeInstanceData with non-const member function.
 */
struct FConstStateTreeExecutionContextView
{
public:
	FConstStateTreeExecutionContextView(UObject& InOwner, const UStateTree& InStateTree, const FStateTreeInstanceData& InInstanceData)
		: ExecutionContext(InOwner, InStateTree, const_cast<FStateTreeInstanceData&>(InInstanceData))
	{}

	operator const FStateTreeExecutionContext& ()
	{
		return ExecutionContext;
	}

	const FStateTreeExecutionContext& Get() const
	{
		return ExecutionContext;
	}

private:
	FConstStateTreeExecutionContextView(const FConstStateTreeExecutionContextView&) = delete;
	FConstStateTreeExecutionContextView& operator=(const FConstStateTreeExecutionContextView&) = delete;

private:
	FStateTreeExecutionContext ExecutionContext;
};

#undef UE_API
