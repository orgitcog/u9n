// Copyright Epic Games, Inc. All Rights Reserved.

#include "Traits/MakeGraphFromAsset.h"

#include "AnimNextAnimGraphSettings.h"
#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT(FMakeGraphFromAssetTrait)

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IGraphFactory) \

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FMakeGraphFromAssetTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR

	const UAnimNextAnimationGraph* FMakeGraphFromAssetTrait::GetGraphFromObject(FExecutionContext& Context, const TTraitBinding<IGraphFactory>& Binding, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload) const
	{
		if(InObject == nullptr)
		{
			return nullptr;
		}

		return GetDefault<UAnimNextAnimGraphSettings>()->GetGraphFromObject(InObject);
	}

}
