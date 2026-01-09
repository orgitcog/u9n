// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModifyCurveTrait.h"

#include "EvaluationVM/EvaluationVM.h"

namespace UE::AnimNext
{
AUTO_REGISTER_ANIM_TRAIT(FModifyCurveTrait)

#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
GeneratorMacro(IEvaluate) \

// Trait implementation boilerplate
GENERATE_ANIM_TRAIT_IMPLEMENTATION(FModifyCurveTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
#undef TRAIT_INTERFACE_ENUMERATOR
}

void UE::AnimNext::FModifyCurveTrait::PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
{
	IEvaluate::PostEvaluate(Context, Binding);

	const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
	check(SharedData);

	FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
	check(InstanceData);

	// Copy Alpha value since it is 'latent'
	InstanceData->Alpha = SharedData->GetAlpha(Binding);

#if ENABLE_ANIM_DEBUG 
	InstanceData->HostObject = Context.GetHostObject();
#endif // ENABLE_ANIM_DEBUG 

	Context.AppendTask(FModifyCurveTask::Make(InstanceData, SharedData));
}

FModifyCurveTask FModifyCurveTask::Make(UE::AnimNext::FModifyCurveTrait::FInstanceData* InstanceData,
	const UE::AnimNext::FModifyCurveTrait::FSharedData* SharedData)
{
	FModifyCurveTask Task;
	Task.InstanceData = InstanceData;
	Task.SharedData = SharedData;
	return Task;
}

void FModifyCurveTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;
	
	if (const TUniquePtr<FKeyframeState>* Keyframe = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0))
	{
		for (const FModifyCurveParameters& CurveParameters : SharedData->ModifyCurveParameters)
		{
			float CurrentValue = Keyframe->Get()->Curves.Get(CurveParameters.CurveName);
			float UpdatedValue = ProcessCurveOperation(CurrentValue, CurveParameters.CurveValue, InstanceData->Alpha, SharedData->ApplyMode);
			Keyframe->Get()->Curves.Set(CurveParameters.CurveName, UpdatedValue);
		}
		
	}
}

float FModifyCurveTask::ProcessCurveOperation(float CurrentValue, float NewValue, float Alpha, EAnimNext_ModifyCurveApplyMode ApplyMode)
{
	float UseNewValue = CurrentValue;

	// Use ApplyMode enum to decide how to apply
	if (ApplyMode == EAnimNext_ModifyCurveApplyMode::Add)
	{
		UseNewValue = CurrentValue + NewValue;
	}
	else if (ApplyMode == EAnimNext_ModifyCurveApplyMode::Scale)
	{
		UseNewValue = CurrentValue * NewValue;
	}
	else if (ApplyMode == EAnimNext_ModifyCurveApplyMode::Blend)
	{
		UseNewValue = NewValue;
	}

	const float UseAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	return FMath::Lerp(CurrentValue, UseNewValue, UseAlpha);
}
