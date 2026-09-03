// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStateEnums.h"
#include "SceneStateRange.h"
#include "UObject/ObjectPtr.h"
#include "SceneStateConduit.generated.h"

class USceneStateGeneratedClass;
struct FSceneStateConduitLink;

namespace UE::SceneState
{
	struct FTransitionEvaluationParams;

	namespace Editor
	{
		class FStateMachineCompiler;
		class FStateMachineConduitCompiler;
	}
}

/** Defines a conduit, a transition indirection */
USTRUCT()
struct FSceneStateConduit
{
	GENERATED_BODY()

	/** Called when the Generated Class is linking to cache the Event function and Result property */
	void Link(const FSceneStateConduitLink& InConduitLink, USceneStateGeneratedClass* InGeneratedClass);

	/** Called to evaluate this transition. Returns true if the transition should take place */
	bool Evaluate(const UE::SceneState::FTransitionEvaluationParams& InParams) const;

	FSceneStateRange GetTransitionRange() const
	{
		return TransitionRange;
	}

private:
	/** Calls the evaluation event (can return early if optimized away via the evaluation flags) */
	bool ProcessEvaluationEvent(const UE::SceneState::FTransitionEvaluationParams& InParams) const;

	/** Index and count of the exit transitions that go out of this conduit and into other targets (states, conduits, exit). */
	UPROPERTY()
	FSceneStateRange TransitionRange;

	/** Transition flags indicating how a transition should be evaluated */
	UPROPERTY()
	ESceneStateTransitionEvaluationFlags EvaluationFlags = ESceneStateTransitionEvaluationFlags::None;

	/** Conduit evaluation event to execute */ 
	UPROPERTY(Transient)
	TObjectPtr<UFunction> EvaluationEvent;

	/** Pointer to the result property of this conduit evaluation */
	FStructProperty* ResultProperty = nullptr;

	friend UE::SceneState::Editor::FStateMachineCompiler;
	friend UE::SceneState::Editor::FStateMachineConduitCompiler;
};
