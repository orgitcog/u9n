// Copyright Epic Games, Inc. All Rights Reserved.

#include "MakeGraphFromChooser.h"

#include "Chooser.h"
#include "IObjectChooser.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT(FMakeGraphFromChooserTrait)

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IGraphFactory) \

		GENERATE_ANIM_TRAIT_IMPLEMENTATION(FMakeGraphFromChooserTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR

	const UAnimNextAnimationGraph* FMakeGraphFromChooserTrait::GetGraphFromObject(FExecutionContext& Context, const TTraitBinding<IGraphFactory>& Binding, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload) const
	{
		if(InObject == nullptr)
		{
			return nullptr;
		}

		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		const UChooserTable* Chooser = SharedData->GetChooser(Binding);
		if (Chooser == nullptr)
		{
			return nullptr;
		}

		FChooserEvaluationContext ChooserContext;
		ChooserContext.AddObjectParam(const_cast<UObject*>(InObject));

		const UAnimNextAnimationGraph* AnimationGraph = nullptr;
		UChooserTable::EvaluateChooser(ChooserContext, Chooser, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([&AnimationGraph](UObject* InResult)
		{
			AnimationGraph = Cast<UAnimNextAnimationGraph>(InResult);
			return FObjectChooserBase::EIteratorStatus::Stop;
		}));

		return AnimationGraph;
	}

}
