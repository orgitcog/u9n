// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transition/SceneStateTransition.h"
#include "SceneStateBindingUtils.h"
#include "SceneStateExecutionContext.h"
#include "SceneStateGeneratedClass.h"
#include "SceneStateObject.h"
#include "Transition/SceneStateTransitionEvaluation.h"
#include "Transition/SceneStateTransitionInstance.h"
#include "Transition/SceneStateTransitionLink.h"
#include "Transition/SceneStateTransitionResult.h"

void FSceneStateTransition::Link(const FSceneStateTransitionLink& InTransitionLink, USceneStateGeneratedClass* InGeneratedClass)
{
	if (!InTransitionLink.ResultPropertyName.IsNone())
	{
		ResultProperty = CastField<FStructProperty>(InGeneratedClass->FindPropertyByName(InTransitionLink.ResultPropertyName));
		check(ResultProperty);
	}
	else
	{
		ResultProperty = nullptr;
	}

	if (!InTransitionLink.EventName.IsNone())
	{
		EvaluationEvent = InGeneratedClass->FindFunctionByName(InTransitionLink.EventName);
		check(EvaluationEvent);
	}
	else
	{
		EvaluationEvent = nullptr;
	}
}

void FSceneStateTransition::Setup(const FSceneStateExecutionContext& InContext) const
{
	if (FSceneStateTransitionInstance* Instance = InContext.FindOrAddTransitionInstance(*this))
	{
		Instance->Parameters = InContext.GetTemplateTransitionParameter(*this);
	}
}

bool FSceneStateTransition::Evaluate(const UE::SceneState::FTransitionEvaluationParams& InParams) const
{
	// Check if the target state's required events are all present
	if (!ContainsAllRequiredEvents(InParams))
	{
		return false;
	}

	// Early return if waiting for tasks to finish and there are still active tasks yet to finish
	if (EnumHasAnyFlags(EvaluationFlags, ESceneStateTransitionEvaluationFlags::WaitForTasksToFinish)
		&& InParams.SourceState.HasPendingTasks(InParams.ExecutionContext))
	{
		return false;
	}

	return ProcessEvaluationEvent(InParams);
}

void FSceneStateTransition::Exit(const FSceneStateExecutionContext& InContext) const
{
	InContext.RemoveTransitionInstance(*this);
}

bool FSceneStateTransition::ApplyBindings(const FSceneStateExecutionContext& InContext, FSceneStateTransitionInstance& InInstance) const
{
	QUICK_SCOPE_CYCLE_COUNTER(SceneStateTransition_ApplyBindings);

	const FSceneStateBindingCollection* const BindingCollection = InContext.GetBindingCollection();
	if (!BindingCollection)
	{
		return false;
	}

	auto FindDataView =
		[&InContext](const FSceneStateBindingDataHandle& InDataHandle)
		{
			return InContext.FindDataView(InDataHandle);
		};

	UE::SceneState::FApplyBatchParams ApplyBatchParams
		{
			.BatchIndex = BindingsBatch.Get(),
			.TargetDataView = InInstance.Parameters.GetMutableValue(),
			.BindingCollection = *BindingCollection,
			.FindSourceDataViewFunctor = FindDataView,
		};

	return ApplyBatch(ApplyBatchParams);
}

bool FSceneStateTransition::ContainsAllRequiredEvents(const UE::SceneState::FTransitionEvaluationParams& InParams) const
{
	// No required events present for targets that aren't states
	if (Target.Type != ESceneStateTransitionTargetType::State)
	{
		return true;
	}

	USceneStateEventStream* EventStream = InParams.ExecutionContext.GetEventStream();
	if (!EventStream)
	{
		return true;
	}

	const FSceneState* TargetState = InParams.ExecutionContext.GetState(InParams.StateMachine, Target.Index);
	if (!TargetState)
	{
		return true;
	}

	for (const FSceneStateEventHandler& TargetEventHandler : InParams.ExecutionContext.GetEventHandlers(*TargetState))
	{
		// Fail transition condition if a Target Event Handler cannot find a matching Event in the current stream
		if (!EventStream->FindEventBySchema(TargetEventHandler.GetEventSchemaHandle()))
		{
			return false;
		}
	}

	return true;
}

bool FSceneStateTransition::ProcessEvaluationEvent(const UE::SceneState::FTransitionEvaluationParams& InParams) const
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

	// The evaluation event could either have 0 parameters (optimized when the transition parameters are not used in the event)
	// or it could be using these parameters, in which case EvalEvent->NumParams must match the number of parameters in the bag
	if (EvaluationEvent->NumParms != 0)
	{
		FSceneStateTransitionInstance* Instance = InParams.ExecutionContext.FindOrAddTransitionInstance(*this);
		if (Instance && Instance->Parameters.IsValid())
		{
			ApplyBindings(InParams.ExecutionContext, *Instance);

			checkfSlow(EvaluationEvent->NumParms == Instance->Parameters.GetNumPropertiesInBag()
				, TEXT("Unexpected parameter mismatch! Event Parameter Count: %d... Transition Parameter Count: %d")
				, EvaluationEvent->NumParms
				, Instance->Parameters.GetNumPropertiesInBag());

			FunctionParams = Instance->Parameters.GetMutableValue().GetMemory();
		}
	}

	USceneStateObject* RootState = InParams.ExecutionContext.GetRootState();
	check(RootState);
	RootState->ProcessEvent(EvaluationEvent, FunctionParams);

	const FSceneStateTransitionResult* Result = ResultProperty->ContainerPtrToValuePtr<FSceneStateTransitionResult>(RootState);
	check(Result);
	return Result->bCanTransition;
}
