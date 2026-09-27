// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EvaluationVM/Tasks/BlendKeyframesPerBone.h"
#include "HierarchyTable.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/IContinuousBlend.h"
#include "TraitInterfaces/IEvaluate.h"
#include "TraitInterfaces/IHierarchy.h"
#include "TraitInterfaces/IUpdate.h"

#include "BlendLayer.generated.h"

USTRUCT(meta = (DisplayName = "Blend Layer"))
struct FAnimNextBlendLayerTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	FAnimNextTraitHandle ChildBase;

	UPROPERTY()
	FAnimNextTraitHandle ChildBlend;

	UPROPERTY(EditAnywhere, Category = "Default")
	float BlendWeight = 1.0f;

	/** Blend profile that configures how fast to blend each bone. */
	// TODO: Can't show list of blend profiles, we need to find a skeleton to perform the lookup with
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	TObjectPtr<UHierarchyTable> BlendProfile = nullptr;

	#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(BlendWeight) \

	GENERATE_TRAIT_LATENT_PROPERTIES(FAnimNextBlendLayerTraitSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
	#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	/**
	 * FBlendLayerTrait
	 * 
	 * A trait that can blend a layer into an input.
	 */
	 struct FBlendLayerTrait : FBaseTrait, IEvaluate, IUpdate, IUpdateTraversal, IHierarchy, IContinuousBlend
	 {
		DECLARE_ANIM_TRAIT(FBlendLayerTrait, FBaseTrait)


		using FSharedData = FAnimNextBlendLayerTraitSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
			FTraitPtr ChildBase;
			FTraitPtr ChildBlend;

			bool bWasChildBaseRelevant = false;
			bool bWasChildBlendRelevant = false;

			bool bBoneMaskWeightsNeedEvaluating = true;
			bool bCurveMaskWeightsNeedEvaluating = true;
			bool bAttributeMaskWeightsNeedEvaluating = true;

			TArray<float> BoneMaskWeights;

			UE::Anim::TNamedValueArray<FDefaultAllocator, UE::Anim::FCurveElement> CurveMaskWeights;

			TArray<FAnimNextBlendKeyframePerBoneWithScaleTask::FMaskedAttributeWeight> AttributeMaskWeights;
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