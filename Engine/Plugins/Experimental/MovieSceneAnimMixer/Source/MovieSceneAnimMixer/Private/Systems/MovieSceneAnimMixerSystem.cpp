// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneAnimMixerSystem.h"

#include "MovieSceneRootMotionSection.h"
#include "Systems/ByteChannelEvaluatorSystem.h"

#include "EntitySystem/BuiltInComponentTypes.h"
#include "AnimMixerComponentTypes.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneBoundObjectInstantiator.h"
#include "EntitySystem/MovieSceneBoundSceneComponentInstantiator.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"
#include "EntitySystem/MovieSceneEntityMutations.h"
#include "MovieSceneTracksComponentTypes.h"

#include "Systems/WeightAndEasingEvaluatorSystem.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"
#include "Systems/MovieSceneTransformOriginSystem.h"
#include "EvaluationVM/Tasks/ExecuteProgram.h"
#include "EvaluationVM/Tasks/BlendKeyframes.h"
#include "EvaluationVM/Tasks/ApplyAdditiveKeyframe.h"
#include "EvaluationVM/Tasks/NormalizeRotations.h"
#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"
#include "EvaluationVM/EvaluationProgram.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Algo/ForEach.h"
#include "Algo/MaxElement.h"
#include "Systems/MovieScenePropertyInstantiator.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogatedPropertyInstantiator.h"
#include "UObject/Object.h"
#include "EntitySystem/MovieSceneEntityGroupingSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Component/AnimNextComponent.h"
#include "Systems/MovieSceneAnimInstanceTargetSystem.h"
#include "Systems/MovieSceneAnimNextTargetSystem.h"
#include "Systems/MovieSceneAnimBlueprintTargetSystem.h"
#include "Systems/MovieSceneComponentTransformSystem.h"
#include "Systems/MovieSceneRootMotionSystem.h"
#include "AnimSubsystem_SequencerMixer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneAnimMixerSystem)


namespace UE::MovieScene
{
	struct FUpdateTaskEntities
	{
		static void ForEachEntity(
			TSharedPtr<FAnimNextEvaluationTask> Task,
			TSharedPtr<FMovieSceneAnimMixerEntry> MixerEntry,
			const FMovieSceneRootMotionSettings* RootMotionSettings,
			const double* WeightAndEasings)
		{
			MixerEntry->EvalTask = Task;
			MixerEntry->PoseWeight = WeightAndEasings ? *WeightAndEasings : 1.0;
			if (RootMotionSettings)
			{
				MixerEntry->RootMotionSettings = *RootMotionSettings;
			}
		}
	};

	struct FEvaluateAnimMixers
	{
		TMap<FMovieSceneAnimMixerKey, TSharedPtr<FMovieSceneAnimMixer>>* Mixers;
		const UMovieSceneRootMotionSystem* RootMotionSystem;
		const UMovieSceneTransformOriginSystem* TransformOriginSystem;

		FEvaluateAnimMixers(TMap<FMovieSceneAnimMixerKey, TSharedPtr<FMovieSceneAnimMixer>>* InMixers, UMovieSceneEntitySystemLinker* InLinker)
			: Mixers(InMixers)
		{
			RootMotionSystem = InLinker->FindSystem<UMovieSceneRootMotionSystem>();
			TransformOriginSystem = InLinker->FindSystem<UMovieSceneTransformOriginSystem>();
		}

		void ForEachEntity(
			FObjectComponent MeshComponent,
			const TInstancedStruct<FMovieSceneMixedAnimationTarget>& Target,
			TSharedPtr<FAnimNextEvaluationTask>& MixerTask
			) const
		{
			using ESpaceConversions = FAnimNextConvertRootMotionToWorldSpaceTask::ESpaceConversions;

			TSharedPtr<FMovieSceneAnimMixer> Mixer = Mixers->FindRef(FMovieSceneAnimMixerKey({ MeshComponent.GetObject(), Target }));
			if (!Mixer)
			{
				return;
			}

			if (Mixer->WeakEntries.Num() == 0)
			{
				MixerTask = nullptr;
				Mixer->EvaluationProgram = nullptr;
				return;
			}

			USceneComponent* Component = Cast<USceneComponent>(MeshComponent.GetObject());
			USceneComponent* Root = Component ? Component->GetOwner()->GetRootComponent() : nullptr;

			const bool bComponentHasKeyedTransform = (Component && RootMotionSystem->IsTransformKeyed(Component));
			const bool bRootComponentHasKeyedTransform = (Root && RootMotionSystem->IsTransformKeyed(Root));

			// Create a new eval program
			Mixer->EvaluationProgram = MakeShared<UE::AnimNext::FEvaluationProgram>();

			if (!MixerTask.IsValid())
			{
				MixerTask = MakeShared<FAnimNextExecuteProgramTask>();
			}
			TSharedPtr<FAnimNextExecuteProgramTask> EvalProgramTask = StaticCastSharedPtr<FAnimNextExecuteProgramTask>(MixerTask);
			EvalProgramTask->Program = Mixer->EvaluationProgram;

			// From lowest to highest priority, add the tasks to the program, grouped by their priority.
			// Tasks within the same priority are blending using a weighted average, which is then collapsed into a single pose, and blended with the
			//      subsequnt priority based on its total accumulated PoseWeight.
			//
			// For example, 5 animations with a structure of:
			// Priority 0: (A, w:0.5), (B, w:0.5), (C, w:0.5)
			// Priority 2: (D, w:0.25), (E, w:0.5)
			//
			// Would result in a blend stack equivalent to the following operation:
			// P1 = A*(0.5/1.5) + B*(0.5/1.5) + C(0.5/1.5)
			// P2 = D*(0.25/0.75) + B*(0.5/0.75)
			//
			// FinalPose = (0.25*P1) + (0.75*P2)
			//
			// Note that we assume here that the target will push some kind of 'base pose' to the VM before evaluating the task. This allows us to blend in/out from gameplay for example.

			const int32 NumMixerEntries = Mixer->WeakEntries.Num();

			// Function that performs a lookahead accumulation of the total number of absolute weights for the specified start index's priority,
			//  and returns a value indicating whether the entries need a separate blend stack (true) or whether they can be blended as a single operation
			auto LookaheadComputeAccumulatedWeight = [&Mixer, NumMixerEntries](int32 StartAtIndex, int32 Priority, float& OutAccumulatedWeight) -> bool
			{
				int32 NumAbsoluteBlends = 0;
				int32 NumSkippedBlends = 0;
				float AccumulatedWeight = 0.f;
				for (int32 NextIndex = StartAtIndex; NextIndex < NumMixerEntries; ++NextIndex)
				{
					TSharedPtr<FMovieSceneAnimMixerEntry> NextMixerEntry = Mixer->WeakEntries[NextIndex].Pin();
					if (!ensure(NextMixerEntry))
					{
						continue;
					}
					if (NextMixerEntry->Priority != Priority || NextMixerEntry->bAdditive)
					{
						break;
					}
					else if (NextMixerEntry->bRequiresBlend && !NextMixerEntry->bAdditive)
					{
						if (!FMath::IsNearlyEqual(AccumulatedWeight, AccumulatedWeight + NextMixerEntry->PoseWeight, KINDA_SMALL_NUMBER))
						{
							++NumAbsoluteBlends;
							AccumulatedWeight += NextMixerEntry->PoseWeight;
						}
						else
						{
							++NumSkippedBlends;
						}
					}
				}

				// When skipping blends, the accumulated weight still needs to be factored in for the absolute blends that aren't skipped,
				// so if the total is greater than one, use a separate blend stack, and the accumulated weight.
				if (NumAbsoluteBlends + NumSkippedBlends > 1 && !FMath::IsNearlyZero(AccumulatedWeight, KINDA_SMALL_NUMBER))
				{
					OutAccumulatedWeight = AccumulatedWeight;
					return true;
				}
				return false;
			};

			TSharedPtr<FMovieSceneMixerRootMotionComponentData> RootMotion = Mixer->WeakRootMotion.Pin();

			for (int32 Index = 0; Index < NumMixerEntries; )
			{
				TSharedPtr<FMovieSceneAnimMixerEntry> PeekEntry = Mixer->WeakEntries[Index].Pin();
				if (!ensure(PeekEntry))
				{
					continue;
				}

				const int32 Priority = PeekEntry->Priority;

				// Recompute the accumulated weights for this priority group
				float AccumulatedPriorityWeight = 1.f;
				// If there is more than one pose in this priority with an absolute weight, we need to blend those together using a weighted average before applying additives and blending with the next priority or the base pose
				const bool bNeedsSeparateWeightStack = LookaheadComputeAccumulatedWeight(Index, Priority, AccumulatedPriorityWeight);

				bool bIsFirstAbsoluteBlend = true;
				for ( ; Index < NumMixerEntries; ++Index)
				{
					const TSharedPtr<FMovieSceneAnimMixerEntry>& MixerEntry = Mixer->WeakEntries[Index].Pin();
					if (!MixerEntry)
					{
						continue;
					}

					if (MixerEntry->Priority != Priority)
					{
						// Break and continue with the outer loop
						break;
					}

					if (!MixerEntry->EvalTask.IsValid())
					{
						continue;
					}

					if (bNeedsSeparateWeightStack && FMath::IsNearlyZero(MixerEntry->PoseWeight, KINDA_SMALL_NUMBER))
					{
						continue;
					}

					// Evaluate the pose itself
					Mixer->EvaluationProgram->AppendTaskPtr(MixerEntry->EvalTask);

					// Handle root space conversions and manipulations
					{
						FTransform TransformOrigin  = FTransform::Identity;
						FTransform RootTransform    = FTransform::Identity;
						FVector    RootOffsetOrigin = FVector(ForceInitToZero);

						ESpaceConversions Conversion = ESpaceConversions::None;
						EMovieSceneRootMotionSpace ThisRootSpace = EMovieSceneRootMotionSpace::AnimationSpace;

						if (MixerEntry->RootMotionSettings)
						{
							ThisRootSpace = MixerEntry->RootMotionSettings->RootMotionSpace;

							// If we have a root location or rotation offset/override, set that up
							const bool bHasLocation = !MixerEntry->RootMotionSettings->RootLocation.IsZero();
							const bool bHasRotation = !MixerEntry->RootMotionSettings->RootRotation.IsIdentity();
							if (bHasLocation || bHasRotation)
							{
								if (MixerEntry->RootMotionSettings->TransformMode == EMovieSceneRootMotionTransformMode::Offset)
								{
									Conversion |= ESpaceConversions::RootTransformOffset;
									RootOffsetOrigin = MixerEntry->RootMotionSettings->RootOriginLocation;
								}
								else
								{
									Conversion |= ESpaceConversions::RootTransformOverride;
								}

								RootTransform = FTransform(
									MixerEntry->RootMotionSettings->RootRotation,
									MixerEntry->RootMotionSettings->RootLocation
									);
							}
						}

						if (ThisRootSpace == EMovieSceneRootMotionSpace::AnimationSpace)
						{
							Conversion |= ESpaceConversions::ComponentToActorRotation;

							if (bRootComponentHasKeyedTransform)
							{
								Conversion |= ESpaceConversions::AnimationToWorld;
							}
							else if (TransformOriginSystem && TransformOriginSystem->GetTransformOrigin(MixerEntry->InstanceHandle, TransformOrigin))
							{
								Conversion |= ESpaceConversions::TransformOriginToWorld;
							}
						}
						else
						{
							Conversion |= ESpaceConversions::WorldSpaceComponentTransformCompensation;
						}

						if (Conversion != ESpaceConversions::None)
						{
							// Need to convert root motion attribute to world space for blending, insert task to do that
							Mixer->EvaluationProgram->AppendTask(FAnimNextConvertRootMotionToWorldSpaceTask::Make(
								RootMotion, TransformOrigin, RootTransform, RootOffsetOrigin, Conversion));
						}
					}

					if (MixerEntry->bRequiresBlend)
					{
						if (MixerEntry->bAdditive)
						{
							// Add additive blend task.
							Mixer->EvaluationProgram->AppendTask(FAnimNextApplyAdditiveKeyframeTask::Make(MixerEntry->PoseWeight));
						}
						else if (bNeedsSeparateWeightStack)
						{
							// Add absolute blend task. The first absolute task within a priority level needs to overwrite the pose with its weighted average,
							//     subsequent poses are added to this. This in effect leaves us with a single pose on the stack at the end of this priority level
							//     that includes the weighted average of all absolutes within this priority level, plus all absolutes
							if (bIsFirstAbsoluteBlend)
							{
								bIsFirstAbsoluteBlend = false;
								Mixer->EvaluationProgram->AppendTask(FAnimNextBlendOverwriteKeyframeWithScaleTask::Make(MixerEntry->PoseWeight / AccumulatedPriorityWeight));
							}
							else
							{
								Mixer->EvaluationProgram->AppendTask(FMovieSceneAccumulateAbsoluteBlendTask::Make(MixerEntry->PoseWeight / AccumulatedPriorityWeight));
							}
						}
						else
						{
							Mixer->EvaluationProgram->AppendTask(FAnimNextBlendTwoKeyframesPreserveRootMotionTask::Make(MixerEntry->PoseWeight));
						}
					}
				}
				// If we have a previous amount to blend with the last priority, add the tasks to ensure that happens
				if (bNeedsSeparateWeightStack)
				{
					Mixer->EvaluationProgram->AppendTask(FAnimNextNormalizeKeyframeRotationsTask());

					// For now we always blend weighted averages with the next pose with a weight of 1.
					//    Ultimately this should be controlled by its own weight within an anim mixer layer track.
					constexpr float WeightedAverageBlendWeight = 1.f;
					Mixer->EvaluationProgram->AppendTask(FAnimNextBlendTwoKeyframesPreserveRootMotionTask::Make(WeightedAverageBlendWeight));
				}
			}

			if (RootMotion)
			{
				// Add task to store the final root motion result
				Mixer->EvaluationProgram->AppendTask(FAnimNextStoreRootTransformTask::Make(RootMotion, bComponentHasKeyedTransform, bRootComponentHasKeyedTransform));
			}
		}
	};
}

FAnimNextBlendTwoKeyframesPreserveRootMotionTask FAnimNextBlendTwoKeyframesPreserveRootMotionTask::Make(float InterpolationAlpha)
{
	FAnimNextBlendTwoKeyframesPreserveRootMotionTask Task;
	Task.InterpolationAlpha = InterpolationAlpha;
	return Task;
}

void FAnimNextBlendTwoKeyframesPreserveRootMotionTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;
	using namespace UE::MovieScene;

	const UE::Anim::FAttributeId RootMotionDeltaAttributeId(UE::Anim::IAnimRootMotionProvider::AttributeName, FCompactPoseBoneIndex(0));

	const FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();

	TOptional<FTransform> OverrideRootTransform;
	TOptional<float>      OverrideRootWeight;
	TOptional<FTransform> OverrideRootDeltaTransform;
	bool                  bIsAuthoritative = false;

	// This task is used to perform a two way blend between 2 poses without blending root motion attributes
	//    if they do not exist in either pose. By default these attributes would blend with the identity matrix
	//    in a way that would cause undesirable effects when blending sequencer's pose with the incoming pose
	//    from the upstream graph.

	//        [ Sequencer pose A           ]  [ Sequencer poseB            ]
	//        |    + RootMotionTransform   |  |                            |
	//        [____________________________]  [____________________________]
	//                                 \           /
	//                                  \  Blend  /
	//                                   \       /
	//                                    \     /
	//                                     \   /
	//                                      \ /
	//                       [ Final Sequencer Pose       ]           [ Upstream pose (locomotion) ]
	//                       |  + RootMotionTransform     |           |    + RootMotionDelta       |
	//                       [____________________________]           [____________________________]
	//                                                    \           /
	//                                                     \  Blend  /
	//                                                      \       /
	//                                                       \     /
	//                                                        \   /
	//                                                         \ /
	//                                           [ Final Pose               ]
	//                                           |    + RootMotionTransform |
	//                                           |    + RootMotionDelta     |
	//                                           [__________________________]
	// From this final pose, FAnimNextStoreRootTransformTask is able to accurately read both Sequencer's desired world space
	//    root transform, its desired weight, and the incoming desired root motion delta from locomotion. It can then blend
	//    all these things together to form the final root motion delta which can be consumed by external systems like Mover
	//
	// If there are competing sources of Sequencer root motion, one may be authoritative, so blending will also be skipped
	//    in this case, but only for the RootMotionTransform, since Sequencer doesn't directly write to RootMotionDelta

	{
		const TUniquePtr<FKeyframeState>* KeyframeA = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0);
		const TUniquePtr<FKeyframeState>* KeyframeB = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 1);
		if (KeyframeA && KeyframeB)
		{
			UE::Anim::FStackAttributeContainer& AttributesA = (*KeyframeA)->Attributes;
			UE::Anim::FStackAttributeContainer& AttributesB = (*KeyframeB)->Attributes;

			const FTransformAnimationAttribute* RootMotionTransformA = AttributesA.Find<FTransformAnimationAttribute>(AnimMixerComponents->RootTransformAttributeId);
			const FTransformAnimationAttribute* RootMotionDeltaA     = AttributesA.Find<FTransformAnimationAttribute>(RootMotionDeltaAttributeId);
			const FIntegerAnimationAttribute* RootMotionIsAuthoritativeA = AttributesA.Find<FIntegerAnimationAttribute>(AnimMixerComponents->RootTransformIsAuthoritativeAttributeId);

			const FTransformAnimationAttribute* RootMotionTransformB = AttributesB.Find<FTransformAnimationAttribute>(AnimMixerComponents->RootTransformAttributeId);
			const FTransformAnimationAttribute* RootMotionDeltaB     = AttributesB.Find<FTransformAnimationAttribute>(RootMotionDeltaAttributeId);
			const FIntegerAnimationAttribute* RootMotionIsAuthoritativeB = AttributesB.Find<FIntegerAnimationAttribute>(AnimMixerComponents->RootTransformIsAuthoritativeAttributeId);

			// If Keyframe A is authoritative, or there is no transform from B, preserve only A. Otherwise it would be blended with identity, or non-authoritative source of root motion.
			const bool bAIsAuthoritative = RootMotionTransformA && RootMotionIsAuthoritativeA && !RootMotionIsAuthoritativeB;
			const bool bPreserveTransformA =  bAIsAuthoritative || (RootMotionTransformA && !RootMotionTransformB);

			// Only preserve the transform from B if it is authoritative.
			const bool bPreserveTransformB = RootMotionTransformB && RootMotionIsAuthoritativeB && !RootMotionIsAuthoritativeA;
			
			if (bPreserveTransformA)
			{
				OverrideRootTransform = RootMotionTransformA->Value;
			}
			else if (bPreserveTransformB)
			{
				OverrideRootTransform = RootMotionTransformB->Value;
			}
			
			if (!RootMotionDeltaA && RootMotionDeltaB)
			{
				// Preserve root motion *delta* from B if it's not in A (ie, don't blend it with identity!)
				OverrideRootDeltaTransform = RootMotionDeltaB->Value;
			}

			bIsAuthoritative = RootMotionIsAuthoritativeA || RootMotionIsAuthoritativeB;
		}
	}

	// Do the actual blend
	FAnimNextBlendTwoKeyframesTask::Execute(VM);

	if (OverrideRootTransform || OverrideRootDeltaTransform)
	{
		TUniquePtr<FKeyframeState> Keyframe;
		if (VM.PopValue(KEYFRAME_STACK_NAME, Keyframe) && Keyframe)
		{
			if (OverrideRootTransform)
			{
				Keyframe->Attributes.FindOrAdd<FTransformAnimationAttribute>(AnimMixerComponents->RootTransformAttributeId)->Value
					= OverrideRootTransform.GetValue();
			}
			if (OverrideRootDeltaTransform)
			{
				Keyframe->Attributes.FindOrAdd<FTransformAnimationAttribute>(RootMotionDeltaAttributeId)->Value
					= OverrideRootDeltaTransform.GetValue();
			}
			if (bIsAuthoritative)
			{
				Keyframe->Attributes.FindOrAdd<FIntegerAnimationAttribute>(AnimMixerComponents->RootTransformIsAuthoritativeAttributeId)->Value
					= 1;
			}

			VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(Keyframe));
		}
	}
}


FMovieSceneAccumulateAbsoluteBlendTask FMovieSceneAccumulateAbsoluteBlendTask::Make(float ScaleFactor)
{
	FMovieSceneAccumulateAbsoluteBlendTask Task;
	Task.ScaleFactor = ScaleFactor;

	return Task;
}

void FMovieSceneAccumulateAbsoluteBlendTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	// Pop our top two poses, we'll re-use the top keyframe for our result

	TUniquePtr<FKeyframeState> KeyframeA;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, KeyframeA))
	{
		// We have no inputs, nothing to do
		return;
	}

	TUniquePtr<FKeyframeState> KeyframeB;
	if (!VM.PopValue(KEYFRAME_STACK_NAME, KeyframeB))
	{
		// We have a single input, leave it on top of the stack
		VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeB));
		return;
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
	{
		check(KeyframeA->Pose.GetNumBones() == KeyframeB->Pose.GetNumBones());

		BlendAddWithScale(KeyframeB->Pose.LocalTransforms.GetView(), KeyframeA->Pose.LocalTransforms.GetConstView(), ScaleFactor);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
	{
		KeyframeB->Curves.Accumulate(KeyframeA->Curves, ScaleFactor);
	}

	if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
	{
		UE::Anim::Attributes::AccumulateAttributes(KeyframeA->Attributes, KeyframeB->Attributes, ScaleFactor, AAT_None);
	}

	VM.PushValue(KEYFRAME_STACK_NAME, MoveTemp(KeyframeB));
}

UMovieSceneAnimMixerSystem::UMovieSceneAnimMixerSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();
	RelevantComponent = AnimMixerComponents->Task;
	Phase = ESystemPhase::Instantiation | ESystemPhase::Scheduling;
	SystemCategories = EEntitySystemCategory::BlenderSystems;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

		DefineImplicitPrerequisite(UMovieSceneGenericBoundObjectInstantiator::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UMovieSceneBoundSceneComponentInstantiator::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UWeightAndEasingEvaluatorSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UMovieSceneComponentTransformSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UMovieSceneTransformOriginSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UByteChannelEvaluatorSystem::StaticClass(), GetClass());
		DefineComponentConsumer(GetClass(), BuiltInComponents->BoundObjectKey);
		DefineComponentConsumer(GetClass(), BuiltInComponents->BoundObject);
		DefineComponentConsumer(GetClass(), AnimMixerComponents->Task);
		DefineComponentConsumer(GetClass(), AnimMixerComponents->Target);
		DefineComponentConsumer(GetClass(), AnimMixerComponents->Priority);
		DefineComponentProducer(GetClass(), AnimMixerComponents->MixerTask);
	}
}

TSharedPtr<FMovieSceneMixerRootMotionComponentData> UMovieSceneAnimMixerSystem::FindRootMotion(FMovieSceneAnimMixerKey InMixerKey) const
{
	TSharedPtr<FMovieSceneAnimMixer> Mixer = Mixers.FindRef(InMixerKey);

	return Mixer ? Mixer->WeakRootMotion.Pin() : TSharedPtr<FMovieSceneMixerRootMotionComponentData>();
}

void UMovieSceneAnimMixerSystem::AssignRootMotion(FMovieSceneAnimMixerKey InMixerKey, TSharedPtr<FMovieSceneMixerRootMotionComponentData> RootMotion)
{
	TSharedPtr<FMovieSceneAnimMixer> Mixer = Mixers.FindRef(InMixerKey);

	if (!Mixer && RootMotion)
	{
		Mixer = MakeShared<FMovieSceneAnimMixer>();
		Mixers.Add(InMixerKey, Mixer);
	}
	if (Mixer)
	{
		Mixer->bNeedsResort |= (Mixer->WeakRootMotion.Pin() != RootMotion);
		Mixer->WeakRootMotion = RootMotion;
	}
}

void UMovieSceneAnimMixerSystem::InitializeAllRootMotion()
{
	for (auto It = Mixers.CreateIterator(); It; ++It)
	{
		TSharedPtr<FMovieSceneMixerRootMotionComponentData> RootMotion = It.Value()->WeakRootMotion.Pin();;
		if (RootMotion)
		{
			RootMotion->Initialize();
		}
	}
}

bool UMovieSceneAnimMixerSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	return Mixers.Num() != 0;
}

void UMovieSceneAnimMixerSystem::OnLink()
{
	RootMotionSystem = Linker->LinkSystem<UMovieSceneRootMotionSystem>();
	Linker->SystemGraph.AddReference(this, RootMotionSystem);
}

void UMovieSceneAnimMixerSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	TSharedRef<FMovieSceneEntitySystemRunner> Runner = Linker->GetRunner();

	if (Runner->GetCurrentPhase() == ESystemPhase::Instantiation)
	{
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();
		FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();

		// ----------------------------------------------------------------------------------------------------------------------------------------
		// Step 1 - Remove expired mixer entries.
		FEntityTaskBuilder()
		.Read(BuiltInComponents->BoundObjectKey)
		.Write(AnimMixerComponents->MixerEntry)
		.FilterAll({ BuiltInComponents->Tags.NeedsUnlink, AnimMixerComponents->Task })
		.Iterate_PerEntity(&Linker->EntityManager,
			[this](FObjectKey BoundObjectKey, TSharedPtr<FMovieSceneAnimMixerEntry>& MixerEntry)
			{
				if (MixerEntry)
				{
					if (TSharedPtr<FMovieSceneAnimMixer> Mixer = MixerEntry->WeakMixer.Pin())
					{
						Mixer->bNeedsResort = true;
					}
				}
				MixerEntry = nullptr;
			}
		);

		// ----------------------------------------------------------------------------------------------------------------------------------------
		// Step 2 - Create new mixer entries for new anim tasks, gathering any new mixer entities that need to be created
		TArray<FMovieSceneAnimMixerKey> NewMixers;

		FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(BuiltInComponents->InstanceHandle)
		.Read(BuiltInComponents->BoundObjectKey)
		.Write(AnimMixerComponents->Target)
		.Write(AnimMixerComponents->MixerEntry)
		.Read(AnimMixerComponents->Task)
		.Read(AnimMixerComponents->Priority)
		.ReadOptional(AnimMixerComponents->RootMotionSettings)
		.FilterAll({ BuiltInComponents->Tags.NeedsLink})
		.Iterate_PerEntity( &Linker->EntityManager,
		[this, &NewMixers, BuiltInComponents, AnimMixerComponents]
		(
			FMovieSceneEntityID EntityID,
			FInstanceHandle InstanceHandle,
			FObjectKey BoundObjectKey,
			TInstancedStruct<FMovieSceneMixedAnimationTarget>& Target,
			TSharedPtr<FMovieSceneAnimMixerEntry>& InOutMixerEntry,
			TSharedPtr<FAnimNextEvaluationTask> Task,
			int32 Priority,
			const FMovieSceneRootMotionSettings* RootMotionSettings)
			{
				// For new entities, we 'resolve' the animation target so if 'Automatic' is picked we choose the right one automatically.
				Target = ResolveAnimationTarget(BoundObjectKey, Target);
				FMovieSceneAnimMixerKey Key({ BoundObjectKey, Target });
				TSharedPtr<FMovieSceneAnimMixer> Mixer = Mixers.FindRef(Key);
				if (!Mixer)
				{
					Mixer = MakeShared<FMovieSceneAnimMixer>();
					Mixers.Add(Key, Mixer);
					NewMixers.Add(Key);
				}
				else if (Mixer->WeakEntries.Num() == 0)
				{
					NewMixers.Add(Key);
				}

				// Create a new mixer entry if necessary
				if (!InOutMixerEntry)
				{
					InOutMixerEntry = MakeShared<FMovieSceneAnimMixerEntry>();
				}

				FMovieSceneAnimMixerEntry* Entry = InOutMixerEntry.Get();

				Entry->InstanceHandle = InstanceHandle;
				Entry->EntityID = EntityID;
				Entry->EvalTask = Task;
				Entry->Priority = Priority;
				Entry->PoseWeight = 0;
				if (RootMotionSettings)
				{
					Entry->RootMotionSettings = *RootMotionSettings;

					// If we know we'll need root motion, ensure it is set up correctly with a lifetime reference that keeps it
					//     alive as long as this entry
					TSharedPtr<FMovieSceneMixerRootMotionComponentData> RootMotion = Mixer->WeakRootMotion.Pin();
					if (!RootMotion)
					{
						RootMotion = MakeShared<FMovieSceneMixerRootMotionComponentData>();

						RootMotion->RootDestination = EMovieSceneRootMotionDestination::RootBone;
						RootMotion->OriginalBoundObject = Cast<USceneComponent>(BoundObjectKey.ResolveObjectPtr());

						Mixer->WeakRootMotion = RootMotion;
					}

					// Overwrite the RootDestination for the root motion if we have a legacy setting.
					//   If any actual RootDestination components exist, they will simply override this on eval
					switch (RootMotionSettings->LegacySwapRootBone)
					{
					case ESwapRootBone::SwapRootBone_None:
						break;
					case ESwapRootBone::SwapRootBone_Component:
						RootMotion->RootDestination = EMovieSceneRootMotionDestination::Component;
						break;
					case ESwapRootBone::SwapRootBone_Actor:
						RootMotion->RootDestination = EMovieSceneRootMotionDestination::Actor;
						break;
					}

					Entry->RootMotionLifetimeReference = RootMotion;
				}
				else
				{
					Entry->RootMotionLifetimeReference = nullptr;
				}

				const FComponentMask& Type = Linker->EntityManager.GetEntityType(EntityID);
				Entry->bAdditive = Type.Contains(BuiltInComponents->Tags.AdditiveAnimation);
				Entry->bRequiresBlend = Type.Contains(AnimMixerComponents->Tags.RequiresBlending);

				TSharedPtr<FMovieSceneAnimMixer> ExistingMixer = Entry->WeakMixer.Pin();
				if (ExistingMixer)
				{
					if (ExistingMixer != Mixer)
					{
						ExistingMixer->WeakEntries.Remove(InOutMixerEntry);
						Entry->WeakMixer = nullptr;

						Mixer->WeakEntries.Emplace(InOutMixerEntry);
						Entry->WeakMixer = Mixer;
					}
				}
				else
				{
					Mixer->WeakEntries.Emplace(InOutMixerEntry);
					Entry->WeakMixer = Mixer;
				}

				Mixer->bNeedsResort = true;
			});

		// ----------------------------------------------------------------------------------------------------------------------------------------
		// Step 3 - Create new mixer entities
		for (const FMovieSceneAnimMixerKey& NewMixerKey : NewMixers)
		{
			TSharedPtr<FMovieSceneAnimMixer> NewMixer = Mixers.FindChecked(NewMixerKey);
			NewMixer->MixerEntityID = FEntityBuilder()
				.Add(AnimMixerComponents->MeshComponent, FObjectComponent::Weak(NewMixerKey.BoundObjectKey.ResolveObjectPtr()))
				.Add(AnimMixerComponents->Target, NewMixerKey.Target)
				.Add(AnimMixerComponents->MixerTask, nullptr)
				.AddTag(BuiltInComponents->Tags.RestoreState) // TODO: For now we always restore state on the mixer when it gets unlinked.
				.CreateEntity(&Linker->EntityManager);
		}

		auto SortPredicate = [](const TWeakPtr<FMovieSceneAnimMixerEntry>& A, const TWeakPtr<FMovieSceneAnimMixerEntry>& B)
		{
			return *A.Pin() < *B.Pin();
		};

		// ----------------------------------------------------------------------------------------------------------------------------------------
		// Step 4 - Update mixer entities, and remove stale ones. 
		for (auto It = Mixers.CreateIterator(); It; ++It)
		{
			TSharedPtr<FMovieSceneAnimMixer> Mixer = It.Value();

			if (Mixer->bNeedsResort)
			{
				bool bNeedsRootMotion = false;

				// Remove nullptrs
				for (int32 Index = Mixer->WeakEntries.Num()-1; Index >= 0; --Index)
				{
					if (Mixer->WeakEntries[Index].Pin() == nullptr)
					{
						Mixer->WeakEntries.RemoveAtSwap(Index, 1, EAllowShrinking::No);
					}
				}

				Algo::Sort(Mixer->WeakEntries, SortPredicate);
				Mixer->bNeedsResort = false;
			}

			if (Mixer->WeakEntries.IsEmpty())
			{
				Mixer->EvaluationProgram = nullptr;

				if (Mixer->MixerEntityID)
				{
					Linker->EntityManager.AddComponent(Mixer->MixerEntityID, BuiltInComponents->Tags.NeedsUnlink, EEntityRecursion::Full);
					Mixer->MixerEntityID = FMovieSceneEntityID();
				}

				// If we don't have any root motion then we can clean this up as well
				if (Mixer->WeakRootMotion.Pin() == nullptr)
				{
					It.RemoveCurrent();
				}
			}
		}
	}
}

void UMovieSceneAnimMixerSystem::OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();
	FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();

	// TODO: We should be able to optimize here if we figure out which tasks won't contribute to the final pose. This should be doable 
	// with a little extra API and pre-examination of per-bone blend weights, etc.

	// Update mixer entry tasks- todo this is slow with the current data hierarchy
	// Maybe we want a flat map of entity id to entry, and then put indices into that into the mixer or something similar.
	FTaskID UpdateTask = FEntityTaskBuilder()
		.Read(AnimMixerComponents->Task)
		.Write(AnimMixerComponents->MixerEntry)
		.ReadOptional(AnimMixerComponents->RootMotionSettings)
		.ReadOptional(BuiltInComponents->WeightAndEasingResult)
		.FilterNone({BuiltInComponents->Tags.NeedsUnlink})
		.Schedule_PerEntity<FUpdateTaskEntities>(&Linker->EntityManager, TaskScheduler);


	// For each mixer, build the evaluation program task list.
	FTaskID MixTask = FEntityTaskBuilder()
		.Read(AnimMixerComponents->MeshComponent)
		.Read(AnimMixerComponents->Target)
		.Write(AnimMixerComponents->MixerTask)
		.Schedule_PerEntity<FEvaluateAnimMixers>(&Linker->EntityManager, TaskScheduler, &Mixers, Linker);

	TaskScheduler->AddPrerequisite(UpdateTask, MixTask);
}

void UMovieSceneAnimMixerSystem::OnCleanTaggedGarbage()
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();

	FEntityTaskBuilder()
	.Read(BuiltInComponents->BoundObjectKey)
	.Write(AnimMixerComponents->MixerEntry)
	.FilterAll({ BuiltInComponents->Tags.NeedsUnlink, AnimMixerComponents->Task })
	.Iterate_PerEntity(&Linker->EntityManager,
		[this](FObjectKey BoundObjectKey, TSharedPtr<FMovieSceneAnimMixerEntry>& MixerEntry)
		{
			if (MixerEntry)
			{
				TSharedPtr<FMovieSceneAnimMixer> Mixer = MixerEntry->WeakMixer.Pin();
				if (Mixer)
				{
					Mixer->bNeedsResort = true;
				}
			}
			MixerEntry = nullptr;
		}
	);

	FEntityTaskBuilder()
	.Read(AnimMixerComponents->MeshComponent)
	.Read(AnimMixerComponents->Target)
	.FilterAll({ BuiltInComponents->Tags.NeedsUnlink, AnimMixerComponents->MixerTask })
	.Iterate_PerEntity(&Linker->EntityManager,
		[this](FObjectComponent MeshComponent, const TInstancedStruct<FMovieSceneMixedAnimationTarget>& Target)
		{
			Mixers.Remove(FMovieSceneAnimMixerKey({ MeshComponent.GetObject(), Target }));
		}
	);
}

TInstancedStruct<FMovieSceneMixedAnimationTarget> UMovieSceneAnimMixerSystem::ResolveAnimationTarget(FObjectKey ObjectKey, const TInstancedStruct<FMovieSceneMixedAnimationTarget>& InTarget)
{
	// If user has selected the default 'automatic' target, attempt to choose one automatically for them.
	if (!InTarget.IsValid() || InTarget.GetScriptStruct() == FMovieSceneMixedAnimationTarget::StaticStruct())
	{
		if (UObject* Object = ObjectKey.ResolveObjectPtr())
		{
			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Object);
			UAnimNextComponent* AnimNextComponent = nullptr;
			if (!SkeletalMeshComponent)
			{
				if (AActor* Actor = Cast<AActor>(Object))
				{
					SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
					AnimNextComponent = SkeletalMeshComponent->GetOwner()->FindComponentByClass<UAnimNextComponent>();
				}
			}
			else 
			{
				AnimNextComponent = SkeletalMeshComponent->GetOwner()->FindComponentByClass<UAnimNextComponent>();
			}

			if (AnimNextComponent != nullptr && (!SkeletalMeshComponent || !SkeletalMeshComponent->bEnableAnimation))
			{
				// If we have an anim next component and the skeletal mesh component has animation disabled, default to anim next target
				return TInstancedStruct<FMovieSceneAnimNextInjectionTarget>::Make();
			}
			else if (SkeletalMeshComponent)
			{
				if (UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance())
				{
					if (const FAnimSubsystem_SequencerMixer* MixerSubsystem = AnimInstance->FindSubsystem<FAnimSubsystem_SequencerMixer>())
					{
						// We have an anim blueprint with sequencer mixer target node(s). Create a target using the default target name.
						return TInstancedStruct<FMovieSceneAnimBlueprintTarget>::Make();
					}
				}

				// Fallback to using a custom anim instance as the target
				return TInstancedStruct<FMovieSceneAnimInstanceTarget>::Make();
			}
		}
	}
	return InTarget;
}