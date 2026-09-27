// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/IEvaluate.h"
#include "TraitInterfaces/IGarbageCollection.h"
#include "TraitInterfaces/IAttributeProvider.h"
#include "TraitInterfaces/ITimeline.h"
#include "TraitInterfaces/ITimelinePlayer.h"
#include "TraitInterfaces/IUpdate.h"
#include "Animation/AnimSequence.h"
#include "TraitInterfaces/INotifySource.h"

#include "SequencePlayer.generated.h"

USTRUCT(meta = (DisplayName = "Sequence Player"))
struct FAnimNextSequencePlayerTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	/** The sequence to play. */
	UPROPERTY(EditAnywhere, Category = "Default", meta=(ExportAsReference="true"))
	TObjectPtr<UAnimSequence> AnimSequence;

	/** The play rate multiplier at which this sequence plays. */
	UPROPERTY(EditAnywhere, Category = "Default")
	float PlayRate = 1.0f;

	/** The time at which we should start playing this sequence. */
	UPROPERTY(EditAnywhere, Category = "Default")
	float StartPosition = 0.0f;

	/** Whether or not this sequence playback will loop. */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bLoop = false;

	// Latent pin support boilerplate
	#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(AnimSequence) \
		GeneratorMacro(PlayRate) \
		GeneratorMacro(StartPosition) \
		GeneratorMacro(bLoop) \

	GENERATE_TRAIT_LATENT_PROPERTIES(FAnimNextSequencePlayerTraitSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
	#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	/**
	 * FSequencePlayerTrait
	 * 
	 * A trait that can play an animation sequence.
	 */
	struct FSequencePlayerTrait : FBaseTrait, IEvaluate, IAttributeProvider, ITimeline, ITimelinePlayer, IUpdate, IGarbageCollection, INotifySource
	{
		DECLARE_ANIM_TRAIT(FSequencePlayerTrait, FBaseTrait)

		using FSharedData = FAnimNextSequencePlayerTraitSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
			// Cached value of the anim sequence we are playing
			TObjectPtr<UAnimSequence> AnimSequence;

			/** Delta time range required for root motion extraction **/
			FDeltaTimeRecord DeltaTimeRecord;

			// Current time accumulator
			float InternalTimeAccumulator = 0.0f;

			// The last advance type when FAnimationRuntime::AdvanceTime was called
			ETypeAdvanceAnim LastAdvanceType = ETAA_Default;

			void Construct(const FExecutionContext& Context, const FTraitBinding& Binding);
			void Destruct(const FExecutionContext& Context, const FTraitBinding& Binding);
		};

		// IEvaluate impl
		virtual void PreEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;

		// IAttributeProvider impl
		virtual FOnExtractRootMotionAttribute GetOnExtractRootMotionAttribute(FExecutionContext& Context, const TTraitBinding<IAttributeProvider>& Binding) const override;

		// ITimeline impl
		virtual void GetSyncMarkers(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, FTimelineSyncMarkerArray& OutSyncMarkers) const override;
		virtual FTimelineState GetState(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding) const override;
		virtual FTimelineDelta GetDelta(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding) const override;

		// ITimelinePlayer impl
		virtual void AdvanceBy(FExecutionContext& Context, const TTraitBinding<ITimelinePlayer>& Binding, float DeltaTime, bool bDispatchEvents) const override;

		// IUpdate impl
		virtual void OnBecomeRelevant(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;

		// IGarbageCollection impl
		virtual void AddReferencedObjects(const FExecutionContext& Context, const TTraitBinding<IGarbageCollection>& Binding, FReferenceCollector& Collector) const override;

		// INotifySource impl
		virtual void GetNotifies(FExecutionContext& Context, const TTraitBinding<INotifySource>& Binding, float StartPosition, float Duration, bool bLooping, TArray<FAnimNotifyEventReference>& OutNotifies) const override;
	};
}
