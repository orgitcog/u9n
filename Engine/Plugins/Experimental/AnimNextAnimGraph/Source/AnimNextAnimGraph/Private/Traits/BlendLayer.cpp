// Copyright Epic Games, Inc. All Rights Reserved.

#include "Traits/BlendLayer.h"

#include "AnimationRuntime.h"
#include "Animation/AnimTypes.h"
#include "TraitCore/ExecutionContext.h"
#include "EvaluationVM/Tasks/BlendKeyframes.h"
#include "EvaluationVM/Tasks/NormalizeRotations.h"
#include "HierarchyTable.h"
#include "MaskProfile/HierarchyTableTypeMask.h"
#include "SkeletonHierarchyTableType.h"

class FHierarchyTableBlendProfileProvider : public IInterpolationIndexProvider
{
public:
	TObjectPtr<UHierarchyTable> HierarchyTable;

	// IInterpolationIndexProvider
	virtual TSharedPtr<FPerBoneInterpolationData> GetPerBoneInterpolationData(const USkeleton* Skeleton) const
	{
		return nullptr;
	}

	virtual int32 GetPerBoneInterpolationIndex(const FCompactPoseBoneIndex& InCompactPoseBoneIndex, const FBoneContainer& BoneContainer, const IInterpolationIndexProvider::FPerBoneInterpolationData* Data) const override
	{
		return 0;
	}

	virtual int32 GetPerBoneInterpolationIndex(const FSkeletonPoseBoneIndex InSkeletonBoneIndex, const USkeleton* TargetSkeleton, const IInterpolationIndexProvider::FPerBoneInterpolationData* Data) const override
	{
		if (HierarchyTable)
		{
			if (HierarchyTable->GetTableData().IsValidIndex(InSkeletonBoneIndex.GetInt()))
			{
				return InSkeletonBoneIndex.GetInt();
			}
		}
		return INDEX_NONE;
	}
	// End IInterpolationIndexProvider
};


namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT(FBlendLayerTrait)

	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IContinuousBlend) \
		GeneratorMacro(IEvaluate) \
		GeneratorMacro(IHierarchy) \
		GeneratorMacro(IUpdate) \
		GeneratorMacro(IUpdateTraversal) \

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FBlendLayerTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR

	void FBlendLayerTrait::PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TObjectPtr<UHierarchyTable> HierarchyTable = SharedData->BlendProfile;

		if (InstanceData->ChildBase.IsValid() && InstanceData->ChildBlend.IsValid() && HierarchyTable)
		{
			// We have two children, interpolate them

			TTraitBinding<IContinuousBlend> ContinuousBlendTrait;
			Binding.GetStackInterface(ContinuousBlendTrait);

			TSharedPtr<FHierarchyTableBlendProfileProvider> InterpolationIndexProvider = MakeShared<FHierarchyTableBlendProfileProvider>();
			InterpolationIndexProvider->HierarchyTable = HierarchyTable;

			const FHierarchyTable_TableType_Skeleton& TableMetadata = HierarchyTable->GetTableMetadata<FHierarchyTable_TableType_Skeleton>();
			const TObjectPtr<USkeleton> Skeleton = TableMetadata.Skeleton;

			const float BlendWeight = ContinuousBlendTrait.GetBlendWeight(Context, 1);

			Context.AppendTask(FAnimNextBlendKeyframePerBoneWithScaleTask::Make(InterpolationIndexProvider, Skeleton, InstanceData->BoneMaskWeights, InstanceData->CurveMaskWeights, InstanceData->AttributeMaskWeights, BlendWeight));
			Context.AppendTask(FAnimNextNormalizeKeyframeRotationsTask());
		}
		else
		{
			// We have only one child that is active, do nothing
		}
	}

	void FBlendLayerTrait::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TTraitBinding<IContinuousBlend> ContinuousBlendTrait;
		Binding.GetStackInterface(ContinuousBlendTrait);

		TObjectPtr<UHierarchyTable> HierarchyTable = SharedData->BlendProfile;

		if (InstanceData->bBoneMaskWeightsNeedEvaluating)
		{
			InstanceData->BoneMaskWeights.Empty();

			if (HierarchyTable)
			{
				const int32 NumBlendEntries = SharedData->BlendProfile->GetTableData().Num();
				InstanceData->BoneMaskWeights.SetNum(NumBlendEntries);

				for (int32 Index = 0; Index < NumBlendEntries; ++Index)
				{
					InstanceData->BoneMaskWeights[Index] = HierarchyTable->GetTableEntry(Index)->GetValue<FHierarchyTable_ElementType_Mask>()->Value;
				}
			}

			InstanceData->bBoneMaskWeightsNeedEvaluating = false;
		}

		if (InstanceData->bCurveMaskWeightsNeedEvaluating)
		{
			InstanceData->CurveMaskWeights.Empty();

			if (HierarchyTable)
			{
				for (const FHierarchyTableEntryData& TableEntry : HierarchyTable->GetTableData())
				{
					if (TableEntry.GetMetadata<FHierarchyTable_TablePayloadType_Skeleton>().EntryType == ESkeletonHierarchyTable_TablePayloadEntryType::Curve)
					{
						const float EntryWeight = TableEntry.GetValue<FHierarchyTable_ElementType_Mask>()->Value;
						InstanceData->CurveMaskWeights.Add<UE::Anim::FCurveElement>({ TableEntry.Identifier, EntryWeight });
					}
				}
			}

			InstanceData->bCurveMaskWeightsNeedEvaluating = false;
		}

		if (InstanceData->bAttributeMaskWeightsNeedEvaluating)
		{
			InstanceData->AttributeMaskWeights.Empty();

			if (HierarchyTable)
			{
				for (const FHierarchyTableEntryData& TableEntry : HierarchyTable->GetTableData())
				{
					if (TableEntry.GetMetadata<FHierarchyTable_TablePayloadType_Skeleton>().EntryType == ESkeletonHierarchyTable_TablePayloadEntryType::Attribute)
					{
						const FHierarchyTableEntryData* ParentEntry = HierarchyTable->GetTableEntry(TableEntry.Parent);
						check(ParentEntry && ParentEntry->GetMetadata<FHierarchyTable_TablePayloadType_Skeleton>().EntryType == ESkeletonHierarchyTable_TablePayloadEntryType::Bone);

						UE::Anim::FAttributeId Attribute(TableEntry.Identifier, TableEntry.Parent, TEXT("bone"));

						const float EntryWeight = TableEntry.GetValue<FHierarchyTable_ElementType_Mask>()->Value;
						InstanceData->AttributeMaskWeights.Add(FAnimNextBlendKeyframePerBoneWithScaleTask::FMaskedAttributeWeight(Attribute, EntryWeight));
					}
				}
			}

			InstanceData->bAttributeMaskWeightsNeedEvaluating = false;
		}

		if (!InstanceData->ChildBase.IsValid())
		{
			InstanceData->ChildBase = Context.AllocateNodeInstance(Binding, SharedData->ChildBase);
		}
		else
		{
			InstanceData->bWasChildBaseRelevant = true;
		}

		if (FAnimWeight::IsRelevant(SharedData->BlendWeight) && SharedData->BlendProfile)
		{
			if (!InstanceData->ChildBlend.IsValid())
			{
				// We need to blend a child that isn't instanced yet, allocate it
				InstanceData->ChildBlend = Context.AllocateNodeInstance(Binding, SharedData->ChildBlend);
			}
			else
			{
				InstanceData->bWasChildBlendRelevant = true;
			}
		}
		else
		{
			InstanceData->ChildBlend.Reset();
		}
	}

	void FBlendLayerTrait::QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TTraitBinding<IUpdateTraversal>& Binding, const FTraitUpdateState& TraitState, FUpdateTraversalQueue& TraversalQueue) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TTraitBinding<IContinuousBlend> ContinuousBlendTrait;
		Binding.GetStackInterface(ContinuousBlendTrait);

		const float BlendWeightBlend = ContinuousBlendTrait.GetBlendWeight(Context, 1);
		if (InstanceData->ChildBase.IsValid())
		{
			TraversalQueue.Push(InstanceData->ChildBase, TraitState.WithWeight(1.0f).AsNewlyRelevant(!InstanceData->bWasChildBaseRelevant));
		}

		if (InstanceData->ChildBlend.IsValid() && SharedData->BlendProfile)
		{
			TraversalQueue.Push(InstanceData->ChildBlend, TraitState.WithWeight(BlendWeightBlend).AsNewlyRelevant(!InstanceData->bWasChildBlendRelevant));
		}
	}

	uint32 FBlendLayerTrait::GetNumChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding) const
	{
		return 2;
	}

	void FBlendLayerTrait::GetChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding, FChildrenArray& Children) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// Add the two children, even if the handles are empty
		Children.Add(InstanceData->ChildBase);
		Children.Add(InstanceData->ChildBlend);
	}

	float FBlendLayerTrait::GetBlendWeight(const FExecutionContext& Context, const TTraitBinding<IContinuousBlend>& Binding, int32 ChildIndex) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

		const float BlendWeight = SharedData->GetBlendWeight(Binding);
		float ClampedWeight = FMath::Clamp(BlendWeight, 0.0f, 1.0f);
		if (!SharedData->BlendProfile)
		{
			ClampedWeight = 0.0f;
		}

		if (ChildIndex == 0)
		{
			return 1.0f - ClampedWeight;
		}
		else if (ChildIndex == 1)
		{
			return ClampedWeight;
		}
		else
		{
			// Invalid child index
			return -1.0f;
		}
	}
}