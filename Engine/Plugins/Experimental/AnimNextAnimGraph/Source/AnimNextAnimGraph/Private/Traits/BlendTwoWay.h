// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraitCore/Trait.h"
#include "TraitInterfaces/IContinuousBlend.h"
#include "TraitInterfaces/IEvaluate.h"
#include "TraitInterfaces/IHierarchy.h"
#include "TraitInterfaces/IUpdate.h"

#include "BlendTwoWay.generated.h"

USTRUCT(meta = (DisplayName = "Blend Two Way"))
struct FAnimNextBlendTwoWayTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	/** First output to be blended (full weight is 0.0). */
	UPROPERTY()
	FAnimNextTraitHandle ChildA;

	/** Second output to be blended (full weight is 1.0). */
	UPROPERTY()
	FAnimNextTraitHandle ChildB;

	/** How much to blend our two children: 0.0 is fully child A while 1.0 is fully child B. */
	UPROPERTY(EditAnywhere, Category = "Default")
	float BlendWeight = 0.0f;

	/** This reinitializes child when re-activated */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bResetChildOnActivation = true;

	/** Always update children, regardless of whether or not that child has weight. */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bAlwaysUpdateChildren = false;

	// Latent pin support boilerplate
	#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(BlendWeight) \
		GeneratorMacro(bResetChildOnActivation) \
		GeneratorMacro(bAlwaysUpdateChildren) \

	GENERATE_TRAIT_LATENT_PROPERTIES(FAnimNextBlendTwoWayTraitSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
	#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	/**
	 * FBlendTwoWayTrait
	 * 
	 * A trait that can blend two inputs.
	 */
	struct FBlendTwoWayTrait : FBaseTrait, IEvaluate, IUpdate, IUpdateTraversal, IHierarchy, IContinuousBlend
	{
		DECLARE_ANIM_TRAIT(FBlendTwoWayTrait, FBaseTrait)

		using FSharedData = FAnimNextBlendTwoWayTraitSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
			FTraitPtr ChildA;
			FTraitPtr ChildB;

			bool bIsChildANewlyRelevant : 1 = false;
			bool bIsChildBNewlyRelevant : 1 = false;
			
			bool bWasChildARelevant : 1 = false;
			bool bWasChildBRelevant : 1 = false;
		};

		// IEvaluate impl
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;

		// IUpdateTraversal impl
		virtual void QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TTraitBinding<IUpdateTraversal>& Binding, const FTraitUpdateState& TraitState, FUpdateTraversalQueue& TraversalQueue) const override;

		// IHierarchy impl
		virtual uint32 GetNumChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding) const override;
		virtual void GetChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding, FChildrenArray& Children) const override;

		// IContinuousBlend impl
		virtual float GetBlendWeight(const FExecutionContext& Context, const TTraitBinding<IContinuousBlend>& Binding, int32 ChildIndex) const override;
	};
}
