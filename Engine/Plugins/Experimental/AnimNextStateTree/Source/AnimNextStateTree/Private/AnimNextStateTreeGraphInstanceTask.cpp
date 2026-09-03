// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/AnimNextStateTreeGraphInstanceTask.h"

#include "AnimNextAnimGraphSettings.h"
#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "AnimNextDataInterfacePayload.h"

FAnimNextStateTreeGraphInstanceTask::FAnimNextStateTreeGraphInstanceTask()
{
	// Re-selecting the same state should not cause a re-trigger of EnterState()
	bShouldStateChangeOnReselect = false;
}

bool FAnimNextStateTreeGraphInstanceTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TraitContextHandle);	
	return true;
}

EStateTreeRunStatus FAnimNextStateTreeGraphInstanceTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FAnimNextStateTreeTraitContext& ExecContext = Context.GetExternalData(TraitContextHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Reference task's default payload
	FAnimNextDataInterfacePayload Payload;
	FInstancedStruct PayloadStruct;
	PayloadStruct.InitializeAs(InstanceData.Payload.GetPropertyBagStruct(), InstanceData.Payload.GetValue().GetMemory());
	Payload.AddNative(MoveTemp(PayloadStruct));

	// Temporarily disabling parameter overriding because it is blocking data interface variables from being accessible in subgraphs
	// FInstancedStruct NewNativePayload;
	// NewNativePayload.InitializeAs(Context.GetMutableInstanceData()->GetMutableStorage().GetMutableGlobalParameters().GetScriptStruct(), Context.GetMutableInstanceData()->GetMutableStorage().GetMutableGlobalParameters().GetMemory());
	// Payload.AddNative(MoveTemp(NewNativePayload));

	if (ExecContext.PushAssetOntoBlendStack(InstanceData.Asset.Get(), InstanceData.BlendOptions, MoveTemp(Payload)))
	{
		return EStateTreeRunStatus::Running;
	}
	
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FAnimNextStateTreeGraphInstanceTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.bContinueTicking)
	{
		FAnimNextStateTreeTraitContext& ExecContext = Context.GetExternalData(TraitContextHandle);

		InstanceData.PlaybackRatio = ExecContext.QueryPlaybackRatio(InstanceData.Asset.Get());

		return EStateTreeRunStatus::Running;
	}
	
	return EStateTreeRunStatus::Succeeded;
}

void FAnimNextStateTreeGraphInstanceTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FAnimNextStateTreeTaskBase::ExitState(Context, Transition);
}

#if WITH_EDITOR
void FAnimNextStateTreeGraphInstanceTask::PostEditInstanceDataChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FStateTreeDataView InstanceDataView)
{
	const FProperty* Property = PropertyChangedEvent.Property;
	if(Property == nullptr)
	{
		return;
	}

	if(Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimNextGraphInstanceTaskInstanceData, Asset))
	{
		// Repopulate payload property bag if the selected asset changes
		FAnimNextGraphInstanceTaskInstanceData& InstanceData = InstanceDataView.GetMutable<FAnimNextGraphInstanceTaskInstanceData>();
		const UAnimNextAnimationGraph* AnimationGraph = GetDefault<UAnimNextAnimGraphSettings>()->GetGraphFromObject(InstanceData.Asset);
		GetDefault<UAnimNextAnimGraphSettings>()->GetNonNativePayloadFromGraph(InstanceData.Asset, AnimationGraph, InstanceData.Payload);
	}
}

void FAnimNextStateTreeGraphInstanceTask::GetObjectReferences(TArray<const UObject*>& OutReferencedObjects, FStateTreeDataView InstanceDataView) const
{
	if (InstanceDataView.IsValid())
	{
		OutReferencedObjects.Add(InstanceDataView.Get<FAnimNextGraphInstanceTaskInstanceData>().Asset.Get());
	}	
}
#endif
