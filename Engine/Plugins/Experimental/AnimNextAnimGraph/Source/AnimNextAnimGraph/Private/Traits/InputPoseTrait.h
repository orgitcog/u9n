// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraitCore/Trait.h"
#include "TraitInterfaces/IEvaluate.h"
#include "Graph/AnimNext_LODPose.h"

#include "InputPoseTrait.generated.h"

USTRUCT(meta = (DisplayName = "Input Pose"))
struct FAnimNextInputPoseTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	FAnimNextGraphLODPose Input;

	// Latent pin support boilerplate
#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(Input)

	GENERATE_TRAIT_LATENT_PROPERTIES(FAnimNextInputPoseTraitSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	struct FInputPoseTrait : FBaseTrait, IEvaluate
	{
		DECLARE_ANIM_TRAIT(FInputPoseTrait, FBaseTrait)

		using FSharedData = FAnimNextInputPoseTraitSharedData;

		// IEvaluate impl
		virtual void PreEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;
	};
}