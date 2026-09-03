// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "SceneStateInstance.h"
#include "SceneStateMachineInstance.h"
#include "StructUtils/InstancedStructContainer.h"
#include "Transition/SceneStateTransitionInstance.h"
#include "UObject/ObjectPtr.h"
#include "SceneStateExecutionContext.generated.h"

class UClass;
class USceneStateEventStream;
class USceneStateGeneratedClass;
class USceneStateObject;
class USceneStatePlayer;
struct FPropertyBindingDataView;
struct FPropertyBindingIndex16;
struct FSceneState;
struct FSceneStateBindingCollection;
struct FSceneStateBindingDataHandle;
struct FSceneStateConduit;
struct FSceneStateEventHandler;
struct FSceneStateMachine;
struct FSceneStateMetadata;
struct FSceneStateTask;
struct FSceneStateTransition;

namespace UE::SceneState
{
	class FExecutionContextRegistry;
}

/**
 * Struct representing an execution. It is used mainly to run a Scene State Object,
 * but is also used in to run the same state machines, state, etc. multiple times.
 * @see USceneStateObject
 * @see FSceneStateMachineTask
 *
 * This struct is the place holding the mutable data during execution, as the data residing in the Scene State Generated Class
 * is immutable during execution.
 *
 * This struct also offers functionality to both get class (immutable) objects (states, state machines, etc)
 * and their instance (mutable) data (state instances, state machine instances, etc)
 */
USTRUCT()
struct FSceneStateExecutionContext
{
	GENERATED_BODY()

	SCENESTATE_API static const FSceneStateExecutionContext InvalidContext;

	SCENESTATE_API ~FSceneStateExecutionContext();

	const USceneStateGeneratedClass* GetGeneratedClass() const
	{
		return GeneratedClass;
	}

	USceneStateObject* GetRootState() const
	{
		return RootState;
	}

	/** Gets the registry that this context is registered to */
	TWeakPtr<const UE::SceneState::FExecutionContextRegistry> GetContextRegistry() const;

	const FSceneStateBindingCollection* GetBindingCollection() const;

	/** Returns the Player Debug Name for logging purposes */
	SCENESTATE_API FString GetExecutionContextName() const;

	/** Retrieves the Context Object for this Execution */
	SCENESTATE_API UObject* GetContextObject() const;

	/** Gets the Event Stream from the Root State if available */
	SCENESTATE_API USceneStateEventStream* GetEventStream() const;

	/**
	 * Initialization for the Context.
	 * Through the given root state it pre-allocates the instance data that will be used
	 * @param InRootState the root state of this execution
	 */
	SCENESTATE_API void Setup(USceneStateObject* InRootState);

	/** Called when cleaning up the instances of this execution */
	SCENESTATE_API void Reset();

	/** Finds the data view that matches the given data handle */
	FPropertyBindingDataView FindDataView(const FSceneStateBindingDataHandle& InDataHandle) const;

	/** Invokes a given callable for each task in the given state */
	void ForEachTask(const FSceneState& InState, TFunctionRef<UE::SceneState::EIterationResult(const FSceneStateTask&, FStructView)> InCallable) const;

	/** Returns an array of const views of the Template Task Instances of the given State */
	TArray<FConstStructView> GetTemplateTaskInstances(const FSceneState& InState) const;

	/** Returns a sliced view of the prerequisite task indices for a given task, in relative index based on the owning state's task range */
	TConstArrayView<uint16> GetTaskPrerequisites(const FSceneStateTask& InTask) const;

	/** Returns a sliced view of all the Exit Transitions going out of the given State */
	TConstArrayView<FSceneStateTransition> GetTransitions(const FSceneState& InState) const;

	/** Returns a sliced view of all the Exit Transitions going out of the given Conduit */
	TConstArrayView<FSceneStateTransition> GetTransitions(const FSceneStateConduit& InConduit) const;

	/** Gets the template transition parameter for the given transition */
	FInstancedPropertyBag GetTemplateTransitionParameter(const FSceneStateTransition& InTransition) const;

	/** Returns a sliced view of all the Sub State Machines belonging to the given State */
	TConstArrayView<FSceneStateMachine> GetStateMachines(const FSceneState& InState) const;

	/** Returns a sliced view of all the Event Handlers in the given State */
	TConstArrayView<FSceneStateEventHandler> GetEventHandlers(const FSceneState& InState) const;

#if WITH_EDITOR
	/** Returns the State Metadata of a given state */
	const FSceneStateMetadata* GetStateMetadata(const FSceneState& InState) const;
#endif

	/** Returns the State Machine linked to the given id */
	const FSceneStateMachine* GetStateMachine(const FGuid& InStateMachineId) const;

	/** Returns the currently Active State within this context for the given State Machine */
	const FSceneState* GetActiveState(const FSceneStateMachine& InStateMachine) const;

	/** Returns the State at the given absolute index */
	const FSceneState* GetState(uint16 InAbsoluteIndex) const;

	/** Returns the Event Handler at the given absolute Index */
	const FSceneStateEventHandler* GetEventHandler(uint16 InAbsoluteIndex) const;

	/** Returns the State at the given relative index for the given State Machine */
	const FSceneState* GetState(const FSceneStateMachine& InStateMachine, uint16 InRelativeIndex) const;

	/** Returns the Conduit at the given relative index for the given State Machine */
	const FSceneStateConduit* GetConduit(const FSceneStateMachine& InStateMachine, uint16 InRelativeIndex) const;

	/** Returns the State Instance of this context for the given state, adding a new one if it's not found */
	FSceneStateInstance* FindOrAddStateInstance(const FSceneState& InState) const;

	/** Returns the existing State Instance of this context for the given state absolute index. Null if not found */
	FSceneStateInstance* FindStateInstance(uint16 InAbsoluteIndex) const;

	/** Returns the existing State Instance of this context for the given state. Null if not found */
	FSceneStateInstance* FindStateInstance(const FSceneState& InState) const;

	/** Removes the State Instance of this context for the given state */
	void RemoveStateInstance(const FSceneState& InState) const;

	/** Returns the Task of this context for the given task absolute index. Returns an invalid view if not found */
	FConstStructView FindTask(uint16 InAbsoluteIndex) const;

	/** Returns the Task Instance container of this context for the given state, adding a new one if not found */
	FInstancedStructContainer* FindOrAddTaskInstanceContainer(const FSceneState& InState) const;

	/** Returns the Task Instance container of this context for the given state, returning null if not found */
	FInstancedStructContainer* FindTaskInstanceContainer(const FSceneState& InState) const;

	/** Returns the Task Instance container of this context for the given state absolute index, returning null if not found */
	FInstancedStructContainer* FindTaskInstanceContainer(uint16 InAbsoluteIndex) const;

	/** Returns the Task Instance of this context for the given task absolute index. Returns an invalid view if not found */
	FStructView FindTaskInstance(uint16 InAbsoluteIndex) const;

	/** Removes the Task instance container of this context for the given state*/
	void RemoveTaskInstanceContainer(const FSceneState& InState) const;

	/** Returns the State Machine Instance of this context for the given state machine, adding a new one if it's not found */
	FSceneStateMachineInstance* FindOrAddStateMachineInstance(const FSceneStateMachine& InStateMachine) const;

	/** Returns the existing State Machine Instance of this context for the given state machine absolute index. Null if not found */
	FSceneStateMachineInstance* FindStateMachineInstance(uint16 InAbsoluteIndex) const;

	/** Returns the existing State Machine Instance of this context for the given state machine. Null if not found */
	FSceneStateMachineInstance* FindStateMachineInstance(const FSceneStateMachine& InStateMachine) const;

	/** Removes the State Machine Instance of this context for the given state machine */
	void RemoveStateMachineInstance(const FSceneStateMachine& InStateMachine) const;

	/** Returns the transition instance of this context for the given transition, adding a new one if it's not found */
	FSceneStateTransitionInstance* FindOrAddTransitionInstance(const FSceneStateTransition& InTransition) const;

	/** Returns the transition instance of this context for the given transition absolute index, or null if not found */
	FSceneStateTransitionInstance* FindTransitionInstance(uint16 InAbsoluteIndex) const;

	/** Returns the transition instance of this context for the given transition, or null if not found */
	FSceneStateTransitionInstance* FindTransitionInstance(const FSceneStateTransition& InTransition) const;

	/** Removes the transition instance of this context for the given transition */
	void RemoveTransitionInstance(const FSceneStateTransition& InTransition) const;

private:
	/** Retrieves the Absolute Index of a State in the State array */
	bool GetStateIndex(const FSceneState& InState, uint16& OutIndex) const;

	/** Retrieves the Absolute Index of a State Machine in the State Machine array */
	bool GetStateMachineIndex(const FSceneStateMachine& InStateMachine, uint16& OutIndex) const;

	/** Retrieves the Absolute Index of a Transition in the Transitions array */
	bool GetTransitionIndex(const FSceneStateTransition& InTransition, uint16& OutIndex) const;

	/** Root state object owning the Scene State Execution */
	UPROPERTY()
	TObjectPtr<USceneStateObject> RootState = nullptr;

	/** Class of the root state */
	UPROPERTY()
	TObjectPtr<USceneStateGeneratedClass> GeneratedClass = nullptr;

	/**
	 * Map of State Index to its Instance Data
	 * It's allocated when the state starts and removed on exit.
	 */
	UPROPERTY()
	mutable TMap<uint16, FSceneStateInstance> StateInstances;

	/**
	 * Map of State Index to the Task Instance Container
	 * It's allocated when the state starts and removed on exit.
	 */
	UPROPERTY()
	mutable TMap<uint16, FInstancedStructContainer> TaskInstanceContainers;

	/**
 	 * Map of State Machine Index to its Instance Data
 	 * It's allocated when the State Machine starts and removed on exit.
 	 */
	UPROPERTY()
	mutable TMap<uint16, FSceneStateMachineInstance> StateMachineInstances;

	/**
	 * Map of the transition absolute index to its instance data
	 * It's allocated when the state starts (along with the other exit transitions) and removed on state exit
	 */
	UPROPERTY()
	mutable TMap<uint16, FSceneStateTransitionInstance> TransitionInstances;

	/** Weak reference to the registry containing this context */
	TWeakPtr<UE::SceneState::FExecutionContextRegistry> ContextRegistryWeak;
};
