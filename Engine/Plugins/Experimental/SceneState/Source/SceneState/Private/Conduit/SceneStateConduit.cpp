// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conduit/SceneStateConduit.h"
#include "Conduit/SceneStateConduitLink.h"
#include "SceneStateExecutionContext.h"
#include "SceneStateGeneratedClass.h"
#include "SceneStateObject.h"
#include "Transition/SceneStateTransitionEvaluation.h"
#include "Transition/SceneStateTransitionResult.h"

void FSceneStateConduit::Link(const FSceneStateConduitLink& InConduitLink, USceneStateGeneratedClass* InGeneratedClass)
{
	if (!InConduitLink.ResultPropertyName.IsNone())
	{
		ResultProperty = CastField<FStructProperty>(InGeneratedClass->FindPropertyByName(InConduitLink.ResultPropertyName));
		check(ResultProperty);
	}
	else
	{
		ResultProperty = nullptr;
	}

	if (!InConduitLink.EventName.IsNone())
	{
		EvaluationEvent = InGeneratedClass->FindFunctionByName(InConduitLink.EventName);
		check(EvaluationEvent);
	}
	else
	{
		EvaluationEvent = nullptr;
	}
}

bool FSceneStateConduit::Evaluate(const UE::SceneState::FTransitionEvaluationParams& InParams) const
{
	// Early return if waiting for tasks to finish and there are still active tasks yet to finish
	if (EnumHasAnyFlags(EvaluationFlags, ESceneStateTransitionEvaluationFlags::WaitForTasksToFinish)
		&& InParams.SourceState.HasPendingTasks(InParams.ExecutionContext))
	{
		return false;
	}

	return ProcessEvaluationEvent(InParams);
}

bool FSceneStateConduit::ProcessEvaluationEvent(const UE::SceneState::FTransitionEvaluationParams& InParams) const
{
	if (EnumHasAllFlags(EvaluationFlags, ESceneStateTransitionEvaluationFlags::EvaluationEventAlwaysTrue))
	{
		return true;
	}

	if (!EvaluationEvent || !ResultProperty)
	{
		return false;
	}

	void* FunctionParams = nullptr;

	USceneStateObject* RootState = InParams.ExecutionContext.GetRootState();
	check(RootState);
	RootState->ProcessEvent(EvaluationEvent, FunctionParams);

	const FSceneStateTransitionResult* Result = ResultProperty->ContainerPtrToValuePtr<FSceneStateTransitionResult>(RootState);
	check(Result);
	return Result->bCanTransition;
}
