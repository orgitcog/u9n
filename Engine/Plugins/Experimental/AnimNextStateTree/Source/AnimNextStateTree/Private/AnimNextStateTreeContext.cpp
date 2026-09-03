// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextStateTreeContext.h"

#include "AnimNextAnimGraphSettings.h"
#include "TraitCore/TraitStackBinding.h"
#include "TraitInterfaces/IBlendStack.h"
#include "TraitInterfaces/IGraphFactory.h"
#include "TraitInterfaces/ISmoothBlend.h"
#include "TraitInterfaces/ITimeline.h"

bool FAnimNextStateTreeTraitContext::PushAssetOntoBlendStack(TNonNullPtr<UObject> InAsset, const FAlphaBlendArgs& InBlendArguments, FAnimNextDataInterfacePayload&& InPayload) const
{
	using namespace UE::AnimNext;

	if (Binding == nullptr || Context == nullptr)
	{
		return false;
	}

	TTraitBinding<IBlendStack> BlendStackBinding;
	if(!Binding->GetStackInterface<IBlendStack>(BlendStackBinding))
	{
		return false;
	}

	// Get a graph to instantiate
	const UAnimNextAnimationGraph* AnimationGraph = IGraphFactory::GetGraphFromObjectWithFallback(*Context, *Binding, InAsset, InPayload);
	if(AnimationGraph == nullptr)
	{
		return false;
	}

	IBlendStack::FGraphRequest NewGraphRequest;
	NewGraphRequest.BlendArgs = InBlendArguments;
	NewGraphRequest.FactoryObject = InAsset;
	NewGraphRequest.AnimationGraph = AnimationGraph;
	NewGraphRequest.GraphPayload = MoveTemp(InPayload);

	BlendStackBinding.PushGraph(*Context, MoveTemp(NewGraphRequest));

	return true;
}

float FAnimNextStateTreeTraitContext::QueryPlaybackRatio(TNonNullPtr<UObject> InAsset) const
{
	if (Binding && Context)
	{
		UE::AnimNext::TTraitBinding<UE::AnimNext::ITimeline> Timeline;
		if (Binding->GetStackInterface<UE::AnimNext::ITimeline>(Timeline))
		{
			return Timeline.GetState(*Context).GetPositionRatio();
		}
	}

	return 1.0f;
}
