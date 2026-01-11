// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Conduit/SceneStateConduit.h"
#include "Conduit/SceneStateConduitLink.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "SceneState.h"
#include "SceneStateBindingCollection.h"
#include "SceneStateEventHandler.h"
#include "SceneStateMachine.h"
#include "SceneStateRange.h"
#include "StructUtils/InstancedStructContainer.h"
#include "Tasks/SceneStateTask.h"
#include "Tasks/SceneStateTaskMetadata.h"
#include "Transition/SceneStateTransition.h"
#include "Transition/SceneStateTransitionLink.h"
#include "Transition/SceneStateTransitionMetadata.h"
#include "UObject/ObjectKey.h"
#include "UObject/Package.h"
#include "SceneStateGeneratedClass.generated.h"

class USceneStateObject;
class UUserDefinedStruct;

namespace UE::SceneState::Editor
{
	class FBindingCompiler;
	class FBlueprintCompilerContext;
	class FStateMachineCompiler;
}

/**
 * Object Class for the Scene State Object
 * Holds all the data about States, State Machines, Tasks, etc.
 * All this data is immutable in execution, and as such, it is not instanced to the Scene State Object instances.
 * @see FSceneStateExecutionContext
 */
UCLASS(MinimalAPI)
class USceneStateGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_BODY()

public:
	SCENESTATE_API USceneStateGeneratedClass();

	const FSceneState* GetRootState() const;

	/** Finds the Top-Level State Machine that is linked to the given Guid */
	const FSceneStateMachine* FindStateMachine(const FGuid& InStateMachineId) const;

	const FSceneStateBindingCollection& GetBindingCollection() const
	{
		return BindingCollection;
	}

	TConstArrayView<FSceneState> GetStates() const
	{
		return States;
	}

	TConstArrayView<FSceneStateConduit> GetConduits() const
	{
		return Conduits;
	}

	const FInstancedStructContainer& GetTasks() const
	{
		return Tasks;
	}

	TConstArrayView<uint16> GetTaskPrerequisites() const
	{
		return TaskPrerequisites;
	}

	const FInstancedStructContainer& GetTaskInstances() const
	{
		return TaskInstances;
	}

	TConstArrayView<FSceneStateEventHandler> GetEventHandlers() const
	{
		return EventHandlers;
	}

	TConstArrayView<FSceneStateTransition> GetTransitions() const
	{
		return Transitions;
	}

	FInstancedPropertyBag GetTransitionParameter(uint16 InAbsoluteIndex) const;

	TConstArrayView<FSceneStateMachine> GetStateMachines() const
	{
		return StateMachines;
	}

#if WITH_EDITOR
	TConstArrayView<FSceneStateMetadata> GetStateMetadata() const
	{
		return StateMetadata;
	}

	TConstArrayView<FSceneStateTaskMetadata> GetTaskMetadata() const
	{
		return TaskMetadata;
	}
#endif

	//~ Begin UClass
	SCENESTATE_API virtual void Link(FArchive& Ar, bool bInRelinkExistingProperties) override;
	//~ End UClass

	//~ Begin UObject
	SCENESTATE_API virtual void PostLoad() override;
	SCENESTATE_API virtual void BeginDestroy() override;
	//~ End UObject

#if WITH_EDITOR
	/** Finds the mapped compiled state for a given state node */
	SCENESTATE_API const FSceneState* FindStateFromNode(FObjectKey InStateNode) const;

	/** Finds the mapped compiled task for a given task node */
	SCENESTATE_API const FSceneStateTask* FindTaskFromNode(FObjectKey InTaskNode) const;

	/**
	 * For a given root state and state node retrieves the mapped state instances
	 * @param InRootState the root state that is outered to a player
	 * @param InStateNode the state node to look for 
	 * @param InFunctor the functor to run for each state instance
	 */
	SCENESTATE_API void ForEachStateInstance(USceneStateObject& InRootState
		, FObjectKey InStateNode
		, TFunctionRef<void(const FSceneStateInstance&)> InFunctor) const;

	/**
	 * For a given root state and task node retrieves the mapped task instances
	 * @param InRootState the root state that is outered to a player
	 * @param InTaskNode the task node to look for
	 * @param InFunctor the functor to run for each task instance
	 */
	SCENESTATE_API void ForEachTaskInstance(USceneStateObject& InRootState
		, FObjectKey InTaskNode
		, TFunctionRef<void(const FSceneStateTaskInstance&)> InFunctor) const;
#endif

private:
	/** Finds the struct type for the given data handle */
	const UStruct* FindDataStruct(const FSceneStateBindingDataHandle& InDataHandle);

	/** Patches bindings and resolves the binding paths for the owning binding collection */
	SCENESTATE_API void ResolveBindings();

	/** Resets all the elements that get compiled for this generated class (e.g. Tasks, State Machines, etc) */
	SCENESTATE_API void Reset();

	bool IsFullClass() const;

#if WITH_EDITOR
	void OnObjectsReinstanced(const TMap<UObject*, UObject*>& InReplacementMap);
	void OnStructsReinstanced(const UUserDefinedStruct& InStruct);
#endif

	UPROPERTY()
	FSceneStateBindingCollection BindingCollection;

	UPROPERTY()
	uint16 RootStateIndex = FSceneStateRange::InvalidIndex;

	UPROPERTY()
	TArray<FSceneState> States;

	UPROPERTY()
	FInstancedStructContainer Tasks;

	/** Array of the task prerequisites in their relative index. Each task has a unique range indicating their pre-requisites */
	UPROPERTY()
	TArray<uint16> TaskPrerequisites;

	/** Templates used to instantiate the Task Instances */
	UPROPERTY()
	FInstancedStructContainer TaskInstances;

	UPROPERTY()
	TArray<FSceneStateEventHandler> EventHandlers;

	UPROPERTY()
	TArray<FSceneStateTransition> Transitions;

	/** Compiled transition information only used in Link time */
	UPROPERTY()
	TArray<FSceneStateTransitionLink> TransitionLinks;

	/** Map of the Transition Index (absolute) to the template transition parameters map for evaluation function call. */
	UPROPERTY()
	TMap<uint16, FInstancedPropertyBag> TransitionParameters;

	/** All the compiled conduits */
	UPROPERTY()
	TArray<FSceneStateConduit> Conduits;

	/** Compiled conduit information used only in Link time */
	UPROPERTY()
	TArray<FSceneStateConduitLink> ConduitLinks;

	UPROPERTY()
	TArray<FSceneStateMachine> StateMachines;

	/** Map of the Top-Level State machine Parameters id to the index in the state machine array */
	UPROPERTY()
	TMap<FGuid, uint16> StateMachineIdToIndex;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FSceneStateMetadata> StateMetadata;

	UPROPERTY()
	TArray<FSceneStateTaskMetadata> TaskMetadata;

	UPROPERTY()
	TArray<FSceneStateTransitionMetadata> TransitionMetadata;

	/** Map of a State Node to its State Index */
	TMap<FObjectKey, uint16> StateNodeToIndex;

	/** Map of a State Machine Graph to its State Machine Index */
	TMap<FObjectKey, uint16> StateMachineGraphToIndex;

	/** Map of a Task Node to its Task Index */
	TMap<FObjectKey, uint16> TaskNodeToIndex;

	FDelegateHandle OnObjectsReinstancedHandle;
	FDelegateHandle OnStructsReinstancedHandle;
#endif

	friend UE::SceneState::Editor::FBindingCompiler;
	friend UE::SceneState::Editor::FBlueprintCompilerContext;
	friend UE::SceneState::Editor::FStateMachineCompiler;
	friend class USceneStateBlueprint;
};
