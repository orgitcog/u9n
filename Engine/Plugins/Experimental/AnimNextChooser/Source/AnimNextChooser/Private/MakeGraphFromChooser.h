// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/IGraphFactory.h"

#include "MakeGraphFromChooser.generated.h"

class UChooserTable;

USTRUCT(meta = (DisplayName = "Make Graph From Chooser"))
struct FAnimNextMakeGraphFromChooserTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	// The chooser to use to pick an animation graph 
	UPROPERTY(EditAnywhere, Category = "Chooser", meta=(ExportAsReference="true"))
	TObjectPtr<UChooserTable> Chooser;

	// Latent pin support boilerplate
	#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(Chooser) \

	GENERATE_TRAIT_LATENT_PROPERTIES(FAnimNextMakeGraphFromChooserTraitSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
	#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	/**
	 * FMakeGraphFromChooserTrait
	 *
	 * A trait that uses a chooser to make an AnimNext animation graph
	 */
	struct FMakeGraphFromChooserTrait : FAdditiveTrait, IGraphFactory
	{
		DECLARE_ANIM_TRAIT(FMakeGraphFromChooserTrait, FAdditiveTrait)

		using FSharedData = FAnimNextMakeGraphFromChooserTraitSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
		};

		// IGraphFactory impl
		virtual const UAnimNextAnimationGraph* GetGraphFromObject(FExecutionContext& Context, const TTraitBinding<IGraphFactory>& Binding, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload) const override;
	};
}
