// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStateEnums.h"
#include "SceneStateRange.h"
#include "SceneStateReentryGuard.h"
#include "SceneState.generated.h"

struct FConstStructView;
struct FSceneStateExecutionContext;
struct FSceneStateInstance;
struct FStructView;

namespace UE::SceneState::Editor
{
	class FBlueprintCompilerContext;
	class FStateMachineCompiler;

} // UE::SceneState

/** Metadata information about the State. Available only in editor */
USTRUCT()
struct FSceneStateMetadata
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString StateName;
#endif
};

/**
 * Runtime information of a given State.
 * Holds the range of tasks, state machines and transitions belonging to the state.
 * These states are stored in the Scene State Generated Class.
 */
USTRUCT()
struct FSceneState
{
	GENERATED_BODY()

	SCENESTATE_API void Enter(const FSceneStateExecutionContext& InContext) const;

	SCENESTATE_API void Tick(const FSceneStateExecutionContext& InContext, float InDeltaSeconds) const;

	SCENESTATE_API void Exit(const FSceneStateExecutionContext& InContext) const;

	FSceneStateRange GetTaskRange() const
	{
		return TaskRange;
	}

	FSceneStateRange GetStateMachineRange() const
	{
		return StateMachineRange;
	}

	FSceneStateRange GetTransitionRange() const
	{
		return TransitionRange;
	}

	FSceneStateRange GetEventHandlerRange() const
	{
		return EventHandlerRange;
	}

	/** Starts pending tasks whose prerequisites are satisfied */
	void UpdateActiveTasks(const FSceneStateExecutionContext& InContext, FSceneStateInstance& InInstance) const;

	/** Returns the State Name used for logging. Returns an empty string view if metadata is not available */
	FStringView GetStateName(const FSceneStateExecutionContext& InContext) const;

	/** Returns whether there's any Task pending to finish */
	SCENESTATE_API bool HasPendingTasks(const FSceneStateExecutionContext& InContext) const;

	/** Called on start to create the required task instances that will run for this state */
	SCENESTATE_API void AllocateTaskInstances(const FSceneStateExecutionContext& InContext, TConstArrayView<FConstStructView> InTemplateTaskInstances) const;

private:
	/** Instances the instanced references in source data into the target data using the provided duplication functor */
	SCENESTATE_API void InstanceTaskObjects(UObject* InOuter
		, TConstArrayView<FStructView> InTargets
		, TConstArrayView<FConstStructView> InSources
		, TFunctionRef<UObject*(FObjectDuplicationParameters&)> InDuplicationFunc) const;

	/** Captures all the events of interest from the handlers this state owns */
	void CaptureEvents(const FSceneStateExecutionContext& InContext) const;

	/** Removes all the event data that was captured by the handlers this state owns */
	void ResetCapturedEvents(const FSceneStateExecutionContext& InContext) const;

	/** Index and Count of the tasks that belong to this state. */
	UPROPERTY()
	FSceneStateRange TaskRange;

	/** Index and Count of the sub state machines that belong to this state. */
	UPROPERTY()
	FSceneStateRange StateMachineRange;

	/** Index and count of the exit transitions that go out of this state and into other targets (states, conduits, exit). */
	UPROPERTY()
	FSceneStateRange TransitionRange;

	/** Index and Count of the events that this state and its tasks and its sub state machines will be handling */
	UPROPERTY()
	FSceneStateRange EventHandlerRange;

	/**
	 * Handle to ensure reentry is not hit
	 * @see FSceneStateReentryGuard
	 */
	UE::SceneState::FReentryHandle ReentryHandle;

	friend UE::SceneState::Editor::FBlueprintCompilerContext;
	friend UE::SceneState::Editor::FStateMachineCompiler;
};
