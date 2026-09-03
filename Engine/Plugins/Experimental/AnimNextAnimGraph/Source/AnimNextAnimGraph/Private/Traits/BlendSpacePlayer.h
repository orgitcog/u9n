// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/IEvaluate.h"
#include "TraitInterfaces/IGarbageCollection.h"
#include "TraitInterfaces/ITimeline.h"
#include "TraitInterfaces/IHierarchy.h"
#include "TraitInterfaces/IUpdate.h"
#include "TraitInterfaces/IContinuousBlend.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimNextDataInterfacePayload.h"

#include "BlendSpacePlayer.generated.h"

USTRUCT(meta = (DisplayName = "Blend Space Player"))
struct FAnimNextBlendSpacePlayerTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	/** The blend space to play. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (ExportAsReference = "true"))
	TObjectPtr<UBlendSpace> BlendSpace;

	/** The location on the x-axis to sample. */
	UPROPERTY(EditAnywhere, Category = "Default")
	float XAxisSamplePoint = 0.0f;

	/** The location on the y-axis to sample. */
	UPROPERTY(EditAnywhere, Category = "Default")
	float YAxisSamplePoint = 0.0f;

	/** The play rate multiplier at which this blend space plays. */
	UPROPERTY(EditAnywhere, Category = "Default")
	float PlayRate = 1.0f;

	/** The time at which we should start playing this blend space. This is normalized in the [0,1] range. */
	UPROPERTY(EditAnywhere, Category = "Default")
	float StartPosition = 0.0f;

	/** Whether to loop the animation */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bLoop = false;

	// Latent pin support boilerplate
	#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(BlendSpace) \
		GeneratorMacro(XAxisSamplePoint) \
		GeneratorMacro(YAxisSamplePoint) \
		GeneratorMacro(PlayRate) \
		GeneratorMacro(StartPosition) \
		GeneratorMacro(bLoop) \


	GENERATE_TRAIT_LATENT_PROPERTIES(FAnimNextBlendSpacePlayerTraitSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
	#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	/**
	 * FBlendSpacePlayerTrait
	 *
	 * A trait that can play a blend space
	 */
	struct FBlendSpacePlayerTrait : FBaseTrait, IEvaluate, IUpdate, IUpdateTraversal, IHierarchy, ITimeline, IContinuousBlend, IGarbageCollection
	{
		DECLARE_ANIM_TRAIT(FBlendSpacePlayerTrait, FBaseTrait)

		using FSharedData = FAnimNextBlendSpacePlayerTraitSharedData;

		struct FSampleGraphState
		{
			FAnimNextDataInterfacePayload GraphPayload;

			// The graph instance
			TSharedPtr<FAnimNextGraphInstance> Instance;

			// Our child handle
			// Weak handle to the graph instance's root
			FTraitPtr ChildPtr;

			// The weight of this sample
			float Weight = 0.0f;

			// The scaled delta time for this sample
			float DeltaTime = 0.0f;

			bool bSampledLastFrame = false;

			bool bSampledThisFrame = false;

		};

		struct FInstanceData : FTrait::FInstanceData
		{
			// List of anim next graphs that represent each sample in the blend space
			TArray<FSampleGraphState> SampleGraphs;

			// Cached value of the blend space we are playing
			TObjectPtr<UBlendSpace> BlendSpace;

			// Cached blend samples updated by task and used to interpolate between points over time
			TArray<FBlendSampleData> BlendSamplesData;

			/** Previous position in the triangulation/segmentation */
			int32 CachedTriangulationIndex = INDEX_NONE;

			// Filter used to dampen coordinate changes
			FBlendFilter BlendFilter;

			int32 NumChildren = 0;

			void Construct(const FExecutionContext& Context, const FTraitBinding& Binding);
			void Destruct(const FExecutionContext& Context, const FTraitBinding& Binding);
		};

		// IEvaluate impl
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;

		// IHierarchy impl
		virtual uint32 GetNumChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding) const override;
		virtual void GetChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding, FChildrenArray& Children) const override;

		// IContinuousBlend impl
		virtual float GetBlendWeight(const FExecutionContext& Context, const TTraitBinding<IContinuousBlend>& Binding, int32 ChildIndex) const override;

		// IUpdate impl
		virtual void OnBecomeRelevant(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;

		// IUpdateTraversal impl
		virtual void QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TTraitBinding<IUpdateTraversal>& Binding, const FTraitUpdateState& TraitState, FUpdateTraversalQueue& TraversalQueue) const override;

		// ITimeline impl
		virtual FTimelineState GetState(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding) const override;

		// IGarbageCollection impl
		virtual void AddReferencedObjects(const FExecutionContext& Context, const TTraitBinding<IGarbageCollection>& Binding, FReferenceCollector& Collector) const override;

	};
}
