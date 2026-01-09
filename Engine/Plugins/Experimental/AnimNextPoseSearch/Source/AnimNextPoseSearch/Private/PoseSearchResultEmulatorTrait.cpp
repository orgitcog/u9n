// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchResultEmulatorTrait.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "DataInterface/DataInterfaceStructAdapter.h"
#include "Graph/AnimNextGraphInstance.h"
#include "GraphInterfaces/AnimNextNativeDataInterface_AnimSequencePlayer.h"
#include "GraphInterfaces/AnimNextNativeDataInterface_BlendSpacePlayer.h"
#include "PoseSearch/MultiAnimAsset.h"
#include "TraitInterfaces/IBlendStack.h"
#include "TraitInterfaces/IGraphFactory.h"
#include "TraitInterfaces/ITimeline.h"
#include "EvaluationVM/EvaluationVM.h"
#include "GraphInterfaces/AnimNextNativeDataInterface_SynchronizeUsingGroups.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT(FPoseSearchResultEmulatorTrait)

#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IUpdate) \
		GeneratorMacro(IEvaluate) \

	// Trait implementation boilerplate
	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FPoseSearchResultEmulatorTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
#undef TRAIT_INTERFACE_ENUMERATOR

	void FPoseSearchResultEmulatorTrait::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		using namespace UE::PoseSearch;
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TTraitBinding<IBlendStack> BlendStackBinding;
		if (!Binding.GetStackInterface(BlendStackBinding))
		{
			UE_LOG(LogPoseSearch, Error, TEXT("FPoseSearchResultEmulatorTrait::PreUpdate, missing IBlendStack"));
			return;
		}

		TTraitBinding<ITimeline> TimelineTrait;
		if (!Binding.GetStackInterface(TimelineTrait))
		{
			UE_LOG(LogPoseSearch, Error, TEXT("FPoseSearchResultEmulatorTrait::PreUpdate, missing ITimeline"));
			return;
		}

		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		const UObject* AnimObject = SharedData->GetSelectedAnim(Binding);
		if (AnimObject == nullptr)
		{
			// @TODO: Kill this with fire.
			const FPoseSearchBlueprintResult& Result = SharedData->GetPoseSearchResult(Binding);
			AnimObject = Result.SelectedAnim.Get();
		}

		if (AnimObject)
		{
			// Shared parameters
			const FName Role = SharedData->GetRole(Binding);
			const float SelectedTime = SharedData->GetSelectedTime(Binding);
			const float WantedPlayRate = SharedData->GetWantedPlayRate(Binding);
			const bool bLoop = SharedData->GetbLoop(Binding);
			const float XAxisSamplePoint = SharedData->GetXAxisSamplePoint(Binding);
			const float YAxisSamplePoint = SharedData->GetYAxisSamplePoint(Binding);

			const UAnimationAsset* AnimationAsset = nullptr;
			if (const UMultiAnimAsset* MultiAnimAsset = Cast<UMultiAnimAsset>(AnimObject))
			{
				AnimationAsset = MultiAnimAsset->GetAnimationAsset(Role);
			}
			else
			{
				AnimationAsset = Cast<UAnimationAsset>(AnimObject);
			}

			IBlendStack::FGraphRequestPtr ActiveGraphRequest;
			BlendStackBinding.GetActiveGraph(Context, ActiveGraphRequest);
			const UObject* CurrentAnim = nullptr;
			float TimeDelta = 0.0f;
			bool bIsBlendSpace = AnimationAsset && AnimationAsset->IsA<UBlendSpace>();
			const bool bHasValidActiveGraph = ActiveGraphRequest && ActiveGraphRequest->FactoryObject;
			if (bHasValidActiveGraph)
			{
				CurrentAnim = ActiveGraphRequest->FactoryObject;
				const FTimelineState TimelineState = TimelineTrait.GetState(Context);
				float TimelinePosition = TimelineState.GetPosition();

				const bool bCurrentAnimIsBlendSpace = CurrentAnim->IsA<UBlendSpace>();
				if (bCurrentAnimIsBlendSpace)
				{
					// Convert to normalized time.
					const float TimelineDuration = TimelineState.GetDuration();
					if (TimelineDuration > 0.0f)
					{
						// NOTE: This doesn't work because AnimNext normalized time does not match database indexing normalized time.
						TimelinePosition /= TimelineState.GetDuration();
					}
				}

				TimeDelta = FMath::Abs(TimelinePosition - SelectedTime);
			}

			const float MaxTimeDeltaAllowed = SharedData->GetMaxTimeDeltaAllowed(Binding);
			const bool bDoBlend =	CurrentAnim != AnimationAsset
								||	(TimeDelta > MaxTimeDeltaAllowed && !bIsBlendSpace);

			if (bDoBlend)
			{
				UE::AnimNext::IBlendStack::FGraphRequest NewGraphRequest;
				NewGraphRequest.BlendArgs = SharedData->GetBlendArguments(Binding);

				// Create a default 'native data interface' payload via which we expect to communicate with the graph 
				FAnimNextDataInterfacePayload GraphPayload;
				FInstancedStruct AssetPlayerPayload;
				if (bIsBlendSpace)
				{
					// Tell blendspace samples to synchronize among themselves, but not with anything else.
					FInstancedStruct SyncGroupPayload;
					auto& NativeInterface = SyncGroupPayload.InitializeAs<FAnimNextNativeDataInterface_SynchronizeUsingGroups>();
					NativeInterface.GroupName = NAME_None;
					NativeInterface.GroupRole = EAnimGroupSynchronizationRole::AlwaysFollower;
					NativeInterface.SyncMode = EAnimGroupSynchronizationMode::SynchronizeUsingUniqueGroupName;
					NativeInterface.MatchSyncPoint = true;
					GraphPayload.AddNative(MoveTemp(SyncGroupPayload));

					FAnimNextNativeDataInterface_BlendSpacePlayer& NewNativeInterface = AssetPlayerPayload.InitializeAs<FAnimNextNativeDataInterface_BlendSpacePlayer>();
					NewNativeInterface.BlendSpace = Cast<UBlendSpace>(AnimationAsset);

					NewNativeInterface.XAxisSamplePoint = XAxisSamplePoint;
					NewNativeInterface.YAxisSamplePoint = YAxisSamplePoint;

					NewNativeInterface.PlayRate = WantedPlayRate;
					NewNativeInterface.StartPosition =  SelectedTime;
					NewNativeInterface.Loop = bLoop;
				}
				else
				{
					FAnimNextNativeDataInterface_AnimSequencePlayer& NewNativeInterface = AssetPlayerPayload.InitializeAs<FAnimNextNativeDataInterface_AnimSequencePlayer>();
					NewNativeInterface.AnimSequence = Cast<UAnimSequence>(AnimationAsset);
					NewNativeInterface.PlayRate = WantedPlayRate;
					NewNativeInterface.StartPosition =  SelectedTime;
					NewNativeInterface.Loop = bLoop;
				}

				GraphPayload.AddNative(MoveTemp(AssetPlayerPayload));

				const UAnimNextAnimationGraph* AnimationGraph = IGraphFactory::GetGraphFromObjectWithFallback(Context, Binding, AnimationAsset, GraphPayload);
				if (AnimationGraph != nullptr)
				{
					NewGraphRequest.GraphPayload = MoveTemp(GraphPayload);
					NewGraphRequest.AnimationGraph = AnimationGraph;
					NewGraphRequest.FactoryObject = AnimationAsset;
					BlendStackBinding.PushGraph(Context, MoveTemp(NewGraphRequest));
				}
			}
			
			// HACK: Grab the active request again. It seems the previous active graph request can become invalid here.
			BlendStackBinding.GetActiveGraph(Context, ActiveGraphRequest);
			if (ActiveGraphRequest)
			{
				for (FStructView& Payload : ActiveGraphRequest->GraphPayload.Get())
				{
					if (FAnimNextNativeDataInterface_AnimSequencePlayer* PlayAnimPayload = Payload.GetPtr<FAnimNextNativeDataInterface_AnimSequencePlayer>())
					{
						PlayAnimPayload->PlayRate = WantedPlayRate;
						break;
					}
					else if (FAnimNextNativeDataInterface_BlendSpacePlayer* BlendSpacePayload = Payload.GetPtr<FAnimNextNativeDataInterface_BlendSpacePlayer>())
					{
						BlendSpacePayload->PlayRate = WantedPlayRate;
						BlendSpacePayload->XAxisSamplePoint = XAxisSamplePoint;
						BlendSpacePayload->YAxisSamplePoint = YAxisSamplePoint;
						break;
					}
				}
			}
		}

		IUpdate::PreUpdate(Context, Binding, TraitState);
	}

	void FPoseSearchResultEmulatorTrait::PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
	{
		IEvaluate::PostEvaluate(Context, Binding);

		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		const bool bDisableRootMotion = SharedData->GetbDisableRootMotion(Binding);

		if (bDisableRootMotion)
		{
			FAnimNextPoseSearchResultEmulatorTask Task;
			Context.AppendTask(Task);
		}
	}
	
} // UE::AnimNext

void FAnimNextPoseSearchResultEmulatorTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;

	const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get();
	if (!RootMotionProvider)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FAnimNextPoseSearchResultEmulatorTask::Execute, missing RootMotionProvider"));
		return;
	}

	const TUniquePtr<FKeyframeState>* Keyframe = VM.PeekValue<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0);
	if (!Keyframe)
	{
		return;
	}

	RootMotionProvider->OverrideRootMotion(FTransform::Identity, Keyframe->Get()->Attributes);
}
