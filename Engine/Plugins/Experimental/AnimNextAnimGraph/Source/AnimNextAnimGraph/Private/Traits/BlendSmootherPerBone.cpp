// Copyright Epic Games, Inc. All Rights Reserved.

#include "Traits/BlendSmootherPerBone.h"

#include "AlphaBlend.h"
#include "Animation/BlendProfile.h"
#include "Animation/Skeleton.h"
#include "TraitCore/ExecutionContext.h"
#include "TraitInterfaces/IHierarchy.h"
#include "EvaluationVM/Tasks/BlendKeyframesPerBone.h"
#include "EvaluationVM/Tasks/NormalizeRotations.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT(FBlendSmootherPerBoneCoreTrait)
	AUTO_REGISTER_ANIM_TRAIT(FBlendProfilePerChildProviderTrait)
	AUTO_REGISTER_ANIM_TRAIT(FBlendProfileProviderTrait)

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IDiscreteBlend) \
		GeneratorMacro(IEvaluate) \
		GeneratorMacro(IUpdate) \
	
	// Trait required interfaces implementation boilerplate
	#define TRAIT_REQUIRED_INTERFACE_ENUMERATOR(GeneratorMacroRequired) \
		GeneratorMacroRequired(ISmoothBlend) \
		GeneratorMacroRequired(ISmoothBlendPerBone) \

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FBlendSmootherPerBoneCoreTrait, TRAIT_INTERFACE_ENUMERATOR, TRAIT_REQUIRED_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR
	#undef TRAIT_REQUIRED_INTERFACE_ENUMERATOR

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(ISmoothBlendPerBone) \
		GeneratorMacro(IUpdate) \

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FBlendProfilePerChildProviderTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR
	#undef TRAIT_REQUIRED_INTERFACE_ENUMERATOR

		// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(ISmoothBlendPerBone) \
		GeneratorMacro(IUpdate) \

		GENERATE_ANIM_TRAIT_IMPLEMENTATION(FBlendProfileProviderTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR
	#undef TRAIT_REQUIRED_INTERFACE_ENUMERATOR

	void FBlendSmootherPerBoneCoreTrait::PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TTraitBinding<ISmoothBlend> SmoothBlendTrait;
		Binding.GetStackInterface(SmoothBlendTrait);

		TTraitBinding<IDiscreteBlend> DiscreteBlendTrait;
		Binding.GetStackInterface(DiscreteBlendTrait);

		const int32 NumChildren = InstanceData->PerChildBlendData.Num();

		int32 NumBlending = 0;
		for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			const FBlendData& ChildBlendData = InstanceData->PerChildBlendData[ChildIndex];

			NumBlending += DiscreteBlendTrait.GetBlendWeight(Context, ChildIndex) > 0.0f ? 1 : 0;
		}

		if (NumBlending < 2)
		{
			return;	// If we don't have at least 2 children blending, there is nothing to do
		}

		int32 ChildIndex = NumChildren - 1;
		for (; ChildIndex >= 0; --ChildIndex)
		{
			const float Weight = DiscreteBlendTrait.GetBlendWeight(Context, ChildIndex);

			if (Weight <= 0.0f)
			{
				continue;	// Skip inactive child
			}

			const FBlendData& ChildBlendData = InstanceData->PerChildBlendData[ChildIndex];
			const FBlendSampleData& PoseSampleData = InstanceData->PerBoneSampleData[ChildIndex];

			TSharedPtr<const IBlendProfileInterface> BlendProfileInterface = ChildBlendData.BlendProfileInterface;

			if (BlendProfileInterface)
			{
				Context.AppendTask(FAnimNextBlendOverwriteKeyframePerBoneWithScaleTask::Make(BlendProfileInterface.Get(), PoseSampleData, Weight));
			}
			else
			{
				Context.AppendTask(FAnimNextBlendOverwriteKeyframeWithScaleTask::Make(Weight));
			}

			break;
		}

		// Other children accumulate with scale
		ChildIndex--;
		for (; ChildIndex >= 0; --ChildIndex)
		{
			const float Weight = DiscreteBlendTrait.GetBlendWeight(Context, ChildIndex);

			const FBlendData& ChildBlendData = InstanceData->PerChildBlendData[ChildIndex];
			if (Weight <= 0.0f)
			{
				continue;	// Skip inactive child
			}

			const FBlendSampleData& PoseSampleDataA = InstanceData->PerBoneSampleData[ChildIndex];
			const FBlendSampleData& PoseSampleDataB = InstanceData->PerBoneSampleData[ChildIndex + 1];	// Above on the keyframe stack

			TSharedPtr<const IBlendProfileInterface> BlendProfileInterface = ChildBlendData.BlendProfileInterface;

			if (BlendProfileInterface)
			{
				Context.AppendTask(FAnimNextBlendAddKeyframePerBoneWithScaleTask::Make(BlendProfileInterface.Get(), PoseSampleDataA, PoseSampleDataB, Weight));
			}
			else
			{
				Context.AppendTask(FAnimNextBlendAddKeyframeWithScaleTask::Make(Weight));
			}
		}

		Context.AppendTask(FAnimNextNormalizeKeyframeRotationsTask());
	}

	void FBlendSmootherPerBoneCoreTrait::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// If this is our first update, allocate our blend data
		if (InstanceData->PerChildBlendData.IsEmpty())
		{
			InitializeInstanceData(Context, Binding, SharedData, InstanceData);
		}

		// Update the traits below us, they might trigger a transition
		IUpdate::PreUpdate(Context, Binding, TraitState);

		TTraitBinding<IDiscreteBlend> DiscreteBlendTrait;
		Binding.GetStackInterface(DiscreteBlendTrait);

		const int32 DestinationChildIndex = DiscreteBlendTrait.GetBlendDestinationChildIndex(Context);

		const int32 NumChildren = InstanceData->PerChildBlendData.Num();

		// If we're using a blend profile, extract the scales and build blend sample data
		for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			const float BlendWeight = DiscreteBlendTrait.GetBlendWeight(Context, ChildIndex);
			const FAlphaBlend* BlendState = DiscreteBlendTrait.GetBlendState(Context, ChildIndex);

			FBlendSampleData& PoseSampleData = InstanceData->PerBoneSampleData[ChildIndex];
			PoseSampleData.TotalWeight = BlendWeight;

			const FBlendData& BlendData = InstanceData->PerChildBlendData[ChildIndex];

			for (int32 PerBoneIndex = 0; PerBoneIndex < PoseSampleData.PerBoneBlendData.Num(); ++PerBoneIndex)
			{
				PoseSampleData.PerBoneBlendData[PerBoneIndex] = UBlendProfile::CalculateBoneWeight(
					BlendData.BlendProfileInterface->GetBoneBlendScale(PerBoneIndex),
					EBlendProfileMode::TimeFactor, *BlendState, BlendData.StartAlpha, BlendWeight, false /*bInverse*/);
			}
		}

		if (!InstanceData->PerBoneSampleData.IsEmpty())
		{
			FBlendSampleData::NormalizeDataWeight(InstanceData->PerBoneSampleData);
		}
	}

	void FBlendSmootherPerBoneCoreTrait::OnBlendTransition(FExecutionContext& Context, const TTraitBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
	{
		IDiscreteBlend::OnBlendTransition(Context, Binding, OldChildIndex, NewChildIndex);

		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TTraitBinding<ISmoothBlendPerBone> SmoothBlendPerBoneTrait;
		Binding.GetStackInterface(SmoothBlendPerBoneTrait);

		TTraitBinding<IDiscreteBlend> DiscreteBlendTrait;
		Binding.GetStackInterface(DiscreteBlendTrait);

		const int32 NumChildren = InstanceData->PerChildBlendData.Num();
		if (NewChildIndex >= NumChildren)
		{
			// We have a new child
			check(NewChildIndex == NumChildren);

			TSharedPtr<const IBlendProfileInterface> BlendProfileInterface = SmoothBlendPerBoneTrait.GetBlendProfile(Context, NewChildIndex);

			const FAlphaBlend* BlendState = DiscreteBlendTrait.GetBlendState(Context, NewChildIndex);

			FBlendData& ChildBlendData = InstanceData->PerChildBlendData.AddDefaulted_GetRef();
			ChildBlendData.BlendProfileInterface = BlendProfileInterface;
			ChildBlendData.StartAlpha = BlendState->GetAlpha();

			FBlendSampleData& ChildSampleData = InstanceData->PerBoneSampleData.AddDefaulted_GetRef();
			ChildSampleData.SampleDataIndex = NewChildIndex;
			ChildSampleData.PerBoneBlendData.AddZeroed(BlendProfileInterface ? BlendProfileInterface->GetNumBlendEntries() : 0);
		}

		for (int32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			FBlendData& ChildBlendData = InstanceData->PerChildBlendData[ChildIndex];

			const FAlphaBlend* BlendState = DiscreteBlendTrait.GetBlendState(Context, ChildIndex);
			ChildBlendData.StartAlpha = BlendState->GetAlpha();
		}
	}

	void FBlendSmootherPerBoneCoreTrait::InitializeInstanceData(FExecutionContext& Context, const FTraitBinding& Binding, const FSharedData* SharedData, FInstanceData* InstanceData)
	{
		check(InstanceData->PerChildBlendData.IsEmpty());

		TTraitBinding<ISmoothBlendPerBone> SmoothBlendPerBoneTrait;
		Binding.GetStackInterface(SmoothBlendPerBoneTrait);

		const uint32 NumChildren = IHierarchy::GetNumStackChildren(Context, Binding);

		InstanceData->PerChildBlendData.SetNum(NumChildren);
		InstanceData->PerBoneSampleData.SetNum(NumChildren);

		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			TSharedPtr<const IBlendProfileInterface> BlendProfileInterface = SmoothBlendPerBoneTrait.GetBlendProfile(Context, ChildIndex);

			FBlendData& ChildBlendData = InstanceData->PerChildBlendData[ChildIndex];
			ChildBlendData.BlendProfileInterface = BlendProfileInterface;
			ChildBlendData.StartAlpha = 0.0f;

			if (BlendProfileInterface)
			{
				const uint32 NumBlendEntries = BlendProfileInterface->GetNumBlendEntries();
				FBlendSampleData& SampleData = InstanceData->PerBoneSampleData[ChildIndex];
				SampleData.SampleDataIndex = ChildIndex;
				SampleData.PerBoneBlendData.AddZeroed(NumBlendEntries);
			}
		}
	}

	/*
		FBlendProfilePerChildProviderTrait
	*/

	void FBlendProfilePerChildProviderTrait::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (InstanceData->BlendProfileInterfaces.IsEmpty())
		{
			InitializeInstanceData(Context, Binding, SharedData, InstanceData);
		}

		IUpdate::PreUpdate(Context, Binding, TraitState);
	}

	TSharedPtr<const IBlendProfileInterface> FBlendProfilePerChildProviderTrait::GetBlendProfile(FExecutionContext& Context, const TTraitBinding<ISmoothBlendPerBone>& Binding, int32 ChildIndex) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// Try find another trait that'll provide a custom blend profile before falling back to ours
		if (TSharedPtr<const IBlendProfileInterface> BlendProfileInterface = ISmoothBlendPerBone::GetBlendProfile(Context, Binding, ChildIndex))
		{
			return BlendProfileInterface;
		}

		if (InstanceData->BlendProfileInterfaces.IsValidIndex(ChildIndex))
		{
			return InstanceData->BlendProfileInterfaces[ChildIndex];
		}

		return nullptr;
	}

	void FBlendProfilePerChildProviderTrait::InitializeInstanceData(FExecutionContext& Context, const FTraitBinding& Binding, const FSharedData* SharedData, FInstanceData* InstanceData)
	{
		check(InstanceData->BlendProfileInterfaces.IsEmpty());

		for (TObjectPtr<UHierarchyTable> BlendProfile : SharedData->BlendProfiles)
		{
			if (BlendProfile)
			{
				InstanceData->BlendProfileInterfaces.Add(MakeShared<FHierarchyTableBlendProfile>(BlendProfile, EBlendProfileMode::TimeFactor));
			}
		}
	}

	/*
		FBlendProfileProviderTrait
	*/

	void FBlendProfileProviderTrait::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		if (SharedData->TimeFactorBlendProfile && !InstanceData->BlendProfileInterface)
		{
			InitializeInstanceData(Context, Binding, SharedData, InstanceData);
		}

		IUpdate::PreUpdate(Context, Binding, TraitState);
	}

	TSharedPtr<const IBlendProfileInterface> FBlendProfileProviderTrait::GetBlendProfile(FExecutionContext& Context, const TTraitBinding<ISmoothBlendPerBone>& Binding, int32 ChildIndex) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// Try find another trait that'll provide a custom blend profile before falling back to ours
		if (TSharedPtr<const IBlendProfileInterface> BlendProfileInterface = ISmoothBlendPerBone::GetBlendProfile(Context, Binding, ChildIndex))
		{
			return BlendProfileInterface;
		}
		
		return InstanceData->BlendProfileInterface;
	}

	void FBlendProfileProviderTrait::InitializeInstanceData(FExecutionContext& Context, const FTraitBinding& Binding, const FSharedData* SharedData, FInstanceData* InstanceData)
	{
		InstanceData->BlendProfileInterface = MakeShared<FHierarchyTableBlendProfile>(SharedData->TimeFactorBlendProfile, EBlendProfileMode::TimeFactor);
	}
}
