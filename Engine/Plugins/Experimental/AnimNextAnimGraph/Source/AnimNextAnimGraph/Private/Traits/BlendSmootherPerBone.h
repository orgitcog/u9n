// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Animation/AnimationAsset.h"
#include "Curves/CurveFloat.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/IDiscreteBlend.h"
#include "TraitInterfaces/IEvaluate.h"
#include "TraitInterfaces/ISmoothBlend.h"
#include "TraitInterfaces/IUpdate.h"
#include "TraitInterfaces/ISmoothBlendPerBone.h"
#include "HierarchyTable.h"
#include "HierarchyTableBlendProfile.h"

#include "BlendSmootherPerBone.generated.h"

USTRUCT(meta = (DisplayName = "Blend Smoother Per Bone Core"))
struct FAnimNextBlendSmootherPerBoneCoreTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "Blend Profile Per Child Provider"))
struct FAnimNextBlendProfilePerChildProviderTraitSharedData : public FAnimNextBlendSmootherPerBoneCoreTraitSharedData
{
	GENERATED_BODY()

	/** Blend profile that configures how fast to blend each bone. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	TArray<TObjectPtr<UHierarchyTable>> BlendProfiles;
};

USTRUCT(meta = (DisplayName = "Blend Profile Provider"))
struct FAnimNextBlendProfileProviderTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	/** Blend profile that configures how fast to blend each bone. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	TObjectPtr<UHierarchyTable> TimeFactorBlendProfile;
};

namespace UE::AnimNext
{
	/**
	 * FBlendSmootherPerBoneCoreTrait
	 *
	 * An additive trait that smoothly blends with per-bone weights.
	 */
	struct FBlendSmootherPerBoneCoreTrait : FAdditiveTrait, IEvaluate, IUpdate, IDiscreteBlend
	{
		DECLARE_ANIM_TRAIT(FBlendSmootherPerBoneCoreTrait, FAdditiveTrait)

		using FSharedData = FAnimNextBlendSmootherPerBoneCoreTraitSharedData;

		// Struct for tracking blends for each pose
		struct FBlendData
		{
			TSharedPtr<const IBlendProfileInterface> BlendProfileInterface;

			float StartAlpha = 0.0f;
		};

		struct FInstanceData : FTrait::FInstanceData
		{
			// Blend state per child
			TArray<FBlendData> PerChildBlendData;

			// Per-bone blending data for each child
			TArray<FBlendSampleData> PerBoneSampleData;
		};

		// IEvaluate impl
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override;

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;

		// IDiscreteBlend impl
		virtual void OnBlendTransition(FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const override;

#if WITH_EDITOR
		virtual bool IsHidden() const override { return false; }
#endif

	private:
		// Internal impl
		static void InitializeInstanceData(FExecutionContext& Context, const FTraitBinding& Binding, const FSharedData* SharedData, FInstanceData* InstanceData);
	};

	/**
	 * FBlendProfilePerChildProviderTrait
	 */
	struct FBlendProfilePerChildProviderTrait : FAdditiveTrait, IUpdate, ISmoothBlendPerBone
	{
		DECLARE_ANIM_TRAIT(FBlendProfilePerChildProviderTrait, FAdditiveTrait)

		using FSharedData = FAnimNextBlendProfilePerChildProviderTraitSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
			// Blend state per child
			TArray<TSharedPtr<const FHierarchyTableBlendProfile>> BlendProfileInterfaces;
		};

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;

		// ISmoothBlendPerBone impl
		virtual TSharedPtr<const IBlendProfileInterface> GetBlendProfile(FExecutionContext& Context, const TTraitBinding<ISmoothBlendPerBone>& Binding, int32 ChildIndex) const override;

#if WITH_EDITOR
		virtual bool IsHidden() const override { return false; } // override the base, as it is hidden
#endif

	private:
		// Internal impl
		static void InitializeInstanceData(FExecutionContext& Context, const FTraitBinding& Binding, const FSharedData* SharedData, FInstanceData* InstanceData);
	};

	/**
	 * FBlendProfileProviderTrait
	 * 
	 * Serves firstly as a passthrough node that propagates blend profiles of child traits.
	 * If no child trait provides a blend profile then uses the held blend profile.
	 */
	struct FBlendProfileProviderTrait : FAdditiveTrait, IUpdate, ISmoothBlendPerBone
	{
		DECLARE_ANIM_TRAIT(FBlendProfileProviderTrait, FAdditiveTrait)

		using FSharedData = FAnimNextBlendProfileProviderTraitSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
			// Blend state per child
			TSharedPtr<const FHierarchyTableBlendProfile> BlendProfileInterface;
		};

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;

		// ISmoothBlendPerBone impl
		virtual TSharedPtr<const IBlendProfileInterface> GetBlendProfile(FExecutionContext& Context, const TTraitBinding<ISmoothBlendPerBone>& Binding, int32 ChildIndex) const override;

#if WITH_EDITOR
		virtual bool IsHidden() const override { return false; } // override the base, as it is hidden
#endif

	private:
		// Internal impl
		static void InitializeInstanceData(FExecutionContext& Context, const FTraitBinding& Binding, const FSharedData* SharedData, FInstanceData* InstanceData);
	};
}
