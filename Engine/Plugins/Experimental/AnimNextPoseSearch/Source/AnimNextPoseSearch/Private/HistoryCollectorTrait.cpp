// Copyright Epic Games, Inc. All Rights Reserved.

#include "HistoryCollectorTrait.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "EvaluationVM/EvaluationVM.h"
#include "GenerationTools.h"
#include "Module/AnimNextModuleInstance.h"
#include "TraitCore/NodeInstance.h"
#include "PoseHistoryEvaluation.h"

namespace UE::AnimNext
{
	// @todo: revisit this code once a FCSPose<FLODPoseStack> is implemented!
	struct FComponentSpacePoseProvider : public UE::PoseSearch::IComponentSpacePoseProvider
	{
		FComponentSpacePoseProvider(const FLODPoseStack& InPose)
			: Pose(InPose)
		{
			const USkeleton* Skeleton = GetSkeletonInternal();
			const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
			const int32 NumBones = ReferenceSkeleton.GetNum();
			ComponentSpaceFlags.SetNumZeroed(NumBones);
			ComponentSpaceTransforms.SetNum(NumBones);
		}

		virtual FTransform CalculateComponentSpaceTransform(const FSkeletonPoseBoneIndex SkeletonBoneIdx) override
		{
			FTransform& ComponentSpaceTransform = ComponentSpaceTransforms[SkeletonBoneIdx.GetInt()];
			uint8& ComponentSpaceFlag = ComponentSpaceFlags[SkeletonBoneIdx.GetInt()];

			if (ComponentSpaceFlag)
			{
				return ComponentSpaceTransform;
			}

			const USkeleton* Skeleton = GetSkeletonInternal();
			const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();

			const TArrayView<const FBoneIndexType> SkeletonBoneIndexToLODBoneIndexMap = Pose.GetSkeletonBoneIndexToLODBoneIndexMap();
			const FBoneIndexType LODBoneIndex = SkeletonBoneIndexToLODBoneIndexMap[SkeletonBoneIdx.GetInt()];
			
			if (LODBoneIndex != FBoneIndexType(INDEX_NONE))
			{
				ComponentSpaceTransform = Pose.LocalTransformsView[LODBoneIndex];
			}
			else
			{
				// @todo: use the skeletal mesh reference pose instead of the skeleton one to account for retargeting
				ComponentSpaceTransform = ReferenceSkeleton.GetRefBonePose()[SkeletonBoneIdx.GetInt()];
			}
			
			const int32 ParentIndex = ReferenceSkeleton.GetParentIndex(SkeletonBoneIdx.GetInt());
			if (ParentIndex >= 0)
			{
				ComponentSpaceTransform *= CalculateComponentSpaceTransform(FSkeletonPoseBoneIndex(ParentIndex));
			}

			ComponentSpaceFlag = 1;
			return ComponentSpaceTransform;
		}

		virtual const USkeleton* GetSkeletonAsset() const override
		{
			return GetSkeletonInternal();
		}

	private:
		const USkeleton* GetSkeletonInternal() const
		{
			const USkeleton* Skeleton = Pose.RefPose->Skeleton.Get();
			check(Skeleton);
			return Skeleton;
		}

		const FLODPoseStack& Pose;
		TArray<uint8, TInlineAllocator<256 , TMemStackAllocator<>>> ComponentSpaceFlags;
		TArray<FTransform, TInlineAllocator<256 , TMemStackAllocator<>>> ComponentSpaceTransforms;
	};
}

void FAnimNextHistoryCollectorPreEvaluateTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;
	using namespace UE::PoseSearch;

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		FPoseHistoryEvaluationHelper PoseHistoryEvalHelper;
		PoseHistoryEvalHelper.PoseHistoryPtr = InstanceData->PoseHistoryPtr;
		VM.PushValue(POSEHISTORY_STACK_NAME, MakeUnique<FPoseHistoryEvaluationHelper>(PoseHistoryEvalHelper));
	}
}

void FAnimNextHistoryCollectorTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		if (TUniquePtr<FKeyframeState>* Keyframe = VM.PeekValueMutable<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0))
		{
			bool bNeedsReset = false;
			bool bCacheBones = false;

			FMemMark Mark(FMemStack::Get());

			const FLODPoseStack& Pose = (*Keyframe)->Pose;
			TArray<FBoneIndexType> BoneIndicesWithParents;

			if (SharedData->PoseCount != PoseHistory->GetMaxNumPoses() || SharedData->SamplingInterval != PoseHistory->GetSamplingInterval())
			{
				bCacheBones = true;
				PoseHistory->Initialize_AnyThread(SharedData->PoseCount, SharedData->SamplingInterval);
			}

			if (bCacheBones)
			{
				const USkeleton* Skeleton = Pose.RefPose->Skeleton.Get();
				check(Skeleton);

				BoneIndicesWithParents.Add(UE::PoseSearch::RootBoneIndexType);

				for (const FBoneReference& CollectedBone : SharedData->CollectedBones)
				{
					if (CollectedBone.BoneName != NAME_None)
					{
						FBoneReference TempCollectedBone = CollectedBone;
						TempCollectedBone.Initialize(Skeleton);
						if (TempCollectedBone.HasValidSetup())
						{
							BoneIndicesWithParents.AddUnique(TempCollectedBone.BoneIndex);
						}
					}
				}

				// Build separate index array with parent indices guaranteed to be present. Sort for EnsureParentsPresent.
				BoneIndicesWithParents.Sort();
				FAnimationRuntime::EnsureParentsPresent(BoneIndicesWithParents, Skeleton->GetReferenceSkeleton());
			}

			FComponentSpacePoseProvider ComponentSpacePoseProvider(Pose);
			PoseHistory->EvaluateComponentSpace_AnyThread(DeltaTime, ComponentSpacePoseProvider, bStoreScales,
				SharedData->RootBoneRecoveryTime, SharedData->RootBoneTranslationRecoveryRatio, SharedData->RootBoneRotationRecoveryRatio,
				bNeedsReset, bCacheBones, BoneIndicesWithParents);
		}
	}
}

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT(FHistoryCollectorTrait)

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IUpdate) \
		GeneratorMacro(IEvaluate) \
		GeneratorMacro(IPoseHistory) \

	// Trait implementation boilerplate
	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FHistoryCollectorTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR

	void FHistoryCollectorTrait::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
		
#if WITH_EDITOR
		if (InstanceData->bIsPostEvaluateBeingCalled)
		{
			InstanceData->bIsPostEvaluateBeingCalled = false;
		}
		else
		{
			UE_LOG(LogPoseSearch, Error, TEXT("FHistoryCollectorTrait::PreUpdate, PostEvaluate has not being called last frame! Some trait in the TraitStack didn't propagate correctly the PostEvaluate!"));
		}
#endif // WITH_EDITOR

		InstanceData->DeltaTime = TraitState.GetDeltaTime();

		// @todo: implement bResetOnBecomingRelevant
		//bool bResetOnBecomingRelevant = SharedData->GetbResetOnBecomingRelevant(Binding);

		if (SharedData->GetbGenerateTrajectory(Binding))
		{
			// @todo: WIP trajectory generation
			//FPoseSearchTrajectoryData::FSampling TrajectoryDataSampling;
			//TrajectoryDataSampling.NumHistorySamples = FMath::Max(SharedData->PoseCount, SharedData->TrajectoryHistoryCount);
			//TrajectoryDataSampling.SecondsPerHistorySample = SharedData->SamplingInterval;
			//TrajectoryDataSampling.NumPredictionSamples = SharedData->TrajectoryPredictionCount;
			//TrajectoryDataSampling.SecondsPerPredictionSample = SharedData->PredictionSamplingInterval;
			//const bool bInitializeWithRefPose = SharedData->GetbInitializeWithRefPose(Binding);
			//InstanceData->PoseHistory.GenerateTrajectory(InAnimInstance, InAnimInstance->GetDeltaSeconds(), InstanceData->TrajectoryData, TrajectoryDataSampling);
		}
		else
		{
			check(InstanceData->PoseHistoryPtr.IsValid());
			InstanceData->PoseHistoryPtr->SetTrajectory(SharedData->GetTrajectory(Binding));
		}

		Context.PushScopedInterface<IPoseHistory>(Binding);

		IUpdate::PreUpdate(Context, Binding, TraitState);
	}

	void FHistoryCollectorTrait::PostUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		ensure(Context.PopScopedInterface<IPoseHistory>(Binding));
		IUpdate::PostUpdate(Context, Binding, TraitState);
	}

	void FHistoryCollectorTrait::PreEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		IEvaluate::PreEvaluate(Context, Binding);
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		check(InstanceData->PoseHistoryPtr.IsValid());
		FAnimNextHistoryCollectorPreEvaluateTask Task;
		Task.InstanceData = InstanceData;
		Context.AppendTask(Task);
	}

	void FHistoryCollectorTrait::PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		IEvaluate::PostEvaluate(Context, Binding);

		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
		const bool bStoreScales = SharedData->GetbStoreScales(Binding);

#if WITH_EDITOR
		if (InstanceData->bIsPostEvaluateBeingCalled)
		{
			UE_LOG(LogPoseSearch, Error, TEXT("FHistoryCollectorTrait::PostEvaluate, PostEvaluate called without calling PreUpdate on this frame! Some trait in the TraitStack doesn't propagate correctly the PreUpdate!"));
		}
		else
		{
			InstanceData->bIsPostEvaluateBeingCalled = true;
		}
#endif // WITH_EDITOR

		if (!SharedData->PoseHistoryReferenceVariable.IsNone())
		{
			FAnimNextModuleInstance* ModuleInstance = Binding.GetTraitPtr().GetNodeInstance()->GetOwner().GetModuleInstance();
			FPoseHistoryReference Reference;
			Reference.PoseHistory = InstanceData->PoseHistoryPtr;
			ModuleInstance->SetVariable(SharedData->PoseHistoryReferenceVariable, Reference);
		}
		
		check(InstanceData->PoseHistoryPtr.IsValid());
		FAnimNextHistoryCollectorTask Task;
		Task.PoseHistory = InstanceData->PoseHistoryPtr.Get();
		Task.SharedData = SharedData;
		Task.bStoreScales = bStoreScales;
		Task.DeltaTime = InstanceData->DeltaTime;
		Task.HostObject = Context.GetHostObject();
		Context.AppendTask(Task);
	}

	const UE::PoseSearch::IPoseHistory* FHistoryCollectorTrait::GetPoseHistory(FExecutionContext& Context, const TTraitBinding<IPoseHistory>& Binding) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
		check(InstanceData->PoseHistoryPtr.IsValid());
		return InstanceData->PoseHistoryPtr.Get();
	}
}
