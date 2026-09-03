// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneRootMotionSystem.h"

#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/IMovieSceneTaskScheduler.h"
#include "MovieSceneTracksComponentTypes.h"
#include "MovieSceneRootMotionSection.h"

#include "EvaluationVM/EvaluationTask.h"
#include "EvaluationVM/EvaluationVM.h"
#include "Component/AnimNextComponent.h"

#include "Systems/MovieSceneAnimMixerSystem.h"
#include "AnimMixerComponentTypes.h"
#include "Module/AnimNextModuleInstance.h"
#include "Animation/AnimRootMotionProvider.h"

#include "SceneInterface.h"

#include "Components/SkeletalMeshComponent.h"
#include "Graph/AnimNext_LODPose.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"
#include "Systems/DoubleChannelEvaluatorSystem.h"
#include "Systems/ByteChannelEvaluatorSystem.h"
#include "Misc/MemStack.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "Systems/MovieScenePropertyInstantiator.h"
#include "Systems/MovieSceneComponentTransformSystem.h"
#include "PreAnimatedState/MovieScenePreAnimatedComponentTransformStorage.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogatedPropertyInstantiator.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "Misc/ScopeRWLock.h"
#include "VisualLogger/VisualLogger.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneRootMotionSystem)

namespace UE::MovieScene
{
	struct FGatherRootDestinations
	{
		static void ForEachEntity(
			uint8 ByteResult,
			EMovieSceneRootMotionDestination& OutRootDestination,
			TSharedPtr<FMovieSceneMixerRootMotionComponentData>& RootMotion
			)
		{
			OutRootDestination = (EMovieSceneRootMotionDestination)(ByteResult);
			RootMotion->RootDestination = OutRootDestination;
		}
	};

	struct FInitializeRootMotionTask
	{
		UMovieSceneAnimMixerSystem* AnimMixer;
		
		void Run(FEntityAllocationWriteContext WriteContext) const
		{
			AnimMixer->InitializeAllRootMotion();
		}

	};

	struct FIsActorBeingMovedStatics
	{
#if !WITH_EDITOR
		constexpr bool IsActive() const
		{
			return false;
		}

		constexpr void Initialize()
		{
		}

#else

		bool IsActive() const
		{
			return bIsActorBeingMoved.load(std::memory_order_relaxed);
		}

		void Initialize()
		{
			static bool bFirst = true;
			if (bFirst)
			{
				bFirst = false;
				GEditor->OnBeginObjectMovement().AddRaw(this, &FIsActorBeingMovedStatics::Set, true);
				GEditor->OnEndObjectMovement().AddRaw(this, &FIsActorBeingMovedStatics::Set, false);
			}
		}

	private:

		void Set(UObject&, bool bIsActive)
		{
			ensure(IsInGameThread());
			bIsActorBeingMoved.exchange(bIsActive, std::memory_order_relaxed);
		}

		mutable std::atomic<bool> bIsActorBeingMoved = false;
#endif // WITH_EDITOR

	} GActorMovementTracker;

} // namespace UE::MovieScene



TOptional<FQuat> FMovieSceneMixerRootMotionComponentData::GetInverseMeshToActorRotation() const
{
	UE::TReadScopeLock ScopeLock(RootMotionLock);
	return InverseMeshToActorRotation;
}

void FMovieSceneMixerRootMotionComponentData::Initialize()
{
	UE::TWriteScopeLock ScopeLock(RootMotionLock);

	InverseMeshToActorRotation.Reset();

	USceneComponent* BoundObject = OriginalBoundObject.Get();

	if (!BoundObject)
	{
		Target = nullptr;
		// Leave the last known component and actor transform
		return;
	}

	// If we're applying to the actor or a root custom attribute, we need to factor out the actor->compont rotation
	const bool bNeedInverseMeshRotation = RootDestination == EMovieSceneRootMotionDestination::Actor
			|| RootDestination == EMovieSceneRootMotionDestination::Attribute;

	USceneComponent* RootComponent = BoundObject->GetOwner()->GetRootComponent();
	if (RootComponent)
	{
		ComponentToActorTransform = BoundObject->GetComponentTransform().GetRelativeTransform(RootComponent->GetComponentTransform()).Inverse();

		if (bNeedInverseMeshRotation && RootComponent != BoundObject)
		{
			InverseMeshToActorRotation = RootComponent->GetComponentTransform().GetRelativeTransformReverse(BoundObject->GetComponentTransform()).GetRotation();
		}

		ActorTransform = RootComponent->GetRelativeTransform();
	}

	switch (RootDestination)
	{
	case EMovieSceneRootMotionDestination::Discard:
	case EMovieSceneRootMotionDestination::RootBone:
	case EMovieSceneRootMotionDestination::Attribute:
		Target = nullptr;
		break;

	case EMovieSceneRootMotionDestination::Component:
		Target = BoundObject;
		break;

	case EMovieSceneRootMotionDestination::Actor:
		Target = RootComponent;
		break;
	}

	// If we want to swap the root bone with the component transform, but that component is the root component,
	//     that is the same behavior as swapping with the actor, so don't perform any inverse component transformations
	bComponentSpaceRoot = (RootDestination == EMovieSceneRootMotionDestination::RootBone)
		|| (RootDestination == EMovieSceneRootMotionDestination::Component && RootComponent != Target);
}

FAnimNextConvertRootMotionToWorldSpaceTask::FAnimNextConvertRootMotionToWorldSpaceTask(const TSharedPtr<FMovieSceneMixerRootMotionComponentData>& InRootMotionData, const FTransform& InTransformOrigin, const FTransform& InRootTransform, const FVector& InRootOffsetOrigin, ESpaceConversions InConversions)
	: WeakRootMotionData(InRootMotionData)
	, RootTransform(InRootTransform)
	, TransformOrigin(InTransformOrigin)
	, RootOffsetOrigin(InRootOffsetOrigin)
	, Conversions(InConversions)
{
}

FAnimNextConvertRootMotionToWorldSpaceTask FAnimNextConvertRootMotionToWorldSpaceTask::Make(const TSharedPtr<FMovieSceneMixerRootMotionComponentData>& InRootMotionData, const FTransform& InTransformOrigin, const FTransform& InRootTransform, const FVector& InRootOffsetOrigin, ESpaceConversions InConversions)
{
	return FAnimNextConvertRootMotionToWorldSpaceTask(InRootMotionData, InTransformOrigin, InRootTransform, InRootOffsetOrigin, InConversions);
}

void FAnimNextConvertRootMotionToWorldSpaceTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext; 
	using namespace UE::MovieScene;

	if (!EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes | EEvaluationFlags::Trajectory))
	{
		return;
	}

	TUniquePtr<FKeyframeState>* Keyframe = VM.PeekValueMutable<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0);
	if (!Keyframe || !Keyframe->IsValid())
	{
		return;
	}

	const FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();

	FTransformAnimationAttribute* RootMotionAttribute = (*Keyframe)->Attributes.Find<FTransformAnimationAttribute>(AnimMixerComponents->RootTransformAttributeId);
	if (!RootMotionAttribute)
	{
		return;
	}

	FTransform RelativeTransform = EnumHasAnyFlags(Conversions, ESpaceConversions::RootTransformOverride)
		? RootTransform
		: RootMotionAttribute->Value;

	TSharedPtr<FMovieSceneMixerRootMotionComponentData> RootMotionData = WeakRootMotionData.Pin();
	if (RootMotionData)
	{
		if (EnumHasAnyFlags(Conversions, ESpaceConversions::ComponentToActorRotation))
		{
			TOptional<FQuat> InverseMeshToActorRotation = RootMotionData->GetInverseMeshToActorRotation();
			if (InverseMeshToActorRotation.IsSet())
			{
				RelativeTransform.SetTranslation(InverseMeshToActorRotation.GetValue() * RelativeTransform.GetTranslation());
			}
		}
	}

	// Apply root transform offsets if necessary
	if (EnumHasAnyFlags(Conversions, ESpaceConversions::RootTransformOffset))
	{
		// Multiply by the inverse of the root origin to get the root motion relative to the origin
		RelativeTransform *= FTransform(-RootOffsetOrigin);
		RelativeTransform = RelativeTransform * RootTransform;
		// Put the transform back in component space
		RelativeTransform *= FTransform(RootOffsetOrigin);
	}

	if (RootMotionData)
	{
		if (EnumHasAnyFlags(Conversions, ESpaceConversions::AnimationToWorld))
		{
			RelativeTransform *= RootMotionData->ActorTransform;
		}
	}

	// Doesn't really make sense to mix both AnimationToWorld & TransformOriginToWorld
	if (EnumHasAnyFlags(Conversions, ESpaceConversions::TransformOriginToWorld))
	{
		RelativeTransform *= TransformOrigin;
	}

	if (EnumHasAnyFlags(Conversions, ESpaceConversions::WorldSpaceComponentTransformCompensation) && RootMotionData)
	{
		RelativeTransform = RootMotionData->ComponentToActorTransform * RelativeTransform;
	}

	RootMotionAttribute->Value = RelativeTransform;
}

FAnimNextStoreRootTransformTask::FAnimNextStoreRootTransformTask(const TSharedPtr<FMovieSceneMixerRootMotionComponentData>& InRootMotionData, bool bInComponentHasKeyedTransform, bool bInRootComponentHasKeyedTransform)
	: WeakRootMotionData(InRootMotionData)
	, bComponentHasKeyedTransform(bInComponentHasKeyedTransform)
	, bRootComponentHasKeyedTransform(bInRootComponentHasKeyedTransform)
{

}

FAnimNextStoreRootTransformTask FAnimNextStoreRootTransformTask::Make(const TSharedPtr<FMovieSceneMixerRootMotionComponentData>& InRootMotionData, bool bInComponentHasKeyedTransform, bool bInRootComponentHasKeyedTransform)
{
	return FAnimNextStoreRootTransformTask(InRootMotionData, bInComponentHasKeyedTransform, bInRootComponentHasKeyedTransform);
}


void FAnimNextStoreRootTransformTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;
	using namespace UE::MovieScene;

	FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();

	TSharedPtr<FMovieSceneMixerRootMotionComponentData> RootMotionData = WeakRootMotionData.Pin();
	if (!RootMotionData.IsValid() || !EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes | EEvaluationFlags::Trajectory))
	{
		return;
	}

	TUniquePtr<FKeyframeState>* Keyframe = VM.PeekValueMutable<TUniquePtr<FKeyframeState>>(KEYFRAME_STACK_NAME, 0);
	if (!Keyframe || !Keyframe->IsValid())
	{
		return;
	}

	FTransformAnimationAttribute* RootMotionAttribute = (*Keyframe)->Attributes.Find<FTransformAnimationAttribute>(AnimMixerComponents->RootTransformAttributeId);
	if (!RootMotionAttribute)
	{
		return;
	}

	const EMovieSceneRootMotionDestination FinalDestination = RootMotionData->RootDestination;

	FTransform RootTransform = RootMotionAttribute->Value;

	if (RootMotionData->bComponentSpaceRoot)
	{
		RootTransform *= RootMotionData->ActorTransform.Inverse();

		TOptional<FQuat> InverseMeshToActorRotation = RootMotionData->GetInverseMeshToActorRotation();
		if (InverseMeshToActorRotation.IsSet())
		{
			RootTransform.SetTranslation(InverseMeshToActorRotation.GetValue().Inverse() * RootTransform.GetTranslation());
		}
	}

	const int32 RootBoneIndex = (*Keyframe)->Pose.GetRefPose().GetLODBoneIndexFromSkeletonBoneIndex(0);

	switch (FinalDestination)
	{
	case EMovieSceneRootMotionDestination::Discard:
		if (RootBoneIndex != INDEX_NONE)
		{
			(*Keyframe)->Pose.LocalTransformsView[RootBoneIndex] = FTransform::Identity;
		}
		return;

	case EMovieSceneRootMotionDestination::RootBone:
		if (RootBoneIndex != INDEX_NONE)
		{
			(*Keyframe)->Pose.LocalTransformsView[RootBoneIndex] = RootTransform;
		}
		return;

	case EMovieSceneRootMotionDestination::Component:
	case EMovieSceneRootMotionDestination::Actor:
	case EMovieSceneRootMotionDestination::Attribute:
		break;
	}


	const FFloatAnimationAttribute* RootMotionWeight = (*Keyframe)->Attributes.Find<FFloatAnimationAttribute>(AnimMixerComponents->RootTransformWeightAttributeId);
	const float TransformWeight = RootMotionWeight ? RootMotionWeight->Value : 1.f;


	// If we have a root motion provider, convert our prospective root motion transform into a delta so that
	//   pose history and locomotion react correctly. This code also supports blending into and out of gameplay
	//   animation
	if (const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get())
	{
		FTransform RootMotionDelta;
		if (RootMotionProvider->ExtractRootMotion((*Keyframe)->Attributes, RootMotionDelta))
		{
			const FTransform ActorHeadingTransform = RootMotionData->ActorTransform;

			TOptional<FQuat> InverseMeshToActorRotation = RootMotionData->GetInverseMeshToActorRotation();
			if (InverseMeshToActorRotation.IsSet())
			{
				// Rotate the translation of the root motion delta to compensate for component rotation
				RootMotionDelta.SetTranslation(InverseMeshToActorRotation.GetValue() * RootMotionDelta.GetTranslation());
			}

			// Blend the root motion delta with Sequencer's desired delta based on its weight
			const FTransform LocomotionDelta  = RootMotionDelta;
			const FTransform DesiredRootDelta = RootTransform * ActorHeadingTransform.Inverse();

			FTransform BlendResult(FMath::Lerp(RootMotionDelta.GetTranslation(), DesiredRootDelta.GetTranslation(), TransformWeight));
			BlendResult.SetRotation(FQuat::Slerp(RootMotionDelta.GetRotation(), DesiredRootDelta.GetRotation(), TransformWeight));

			// Assign the final result in component space
			RootMotionDelta = BlendResult;
			if (InverseMeshToActorRotation.IsSet())
			{
				// Unrotate the translation of the root motion delta to compensate for component rotation
				RootMotionDelta.SetTranslation(InverseMeshToActorRotation->Inverse() * RootMotionDelta.GetTranslation());
			}

#if ENABLE_VISUAL_LOG
			if (FVisualLogger::IsRecording())
			{
				static const TCHAR* LogName = TEXT("MovieSceneRootMotion");
				USceneComponent* Component = RootMotionData->OriginalBoundObject.Get();

				auto DrawMarker = [Component](const FTransform& Transform, const FColor& Color, auto&& Name)
				{
					FVector Dir(50.f, 0.f, 0.f);

					UE_VLOG_CIRCLE(Component, LogName, Display, Transform.GetLocation(), FVector::UpVector, 10.f, Color, Name);
					UE_VLOG_ARROW(Component, LogName, Display, Transform.GetLocation(), Transform.GetLocation() + Transform.GetRotation()*Dir, Color, TEXT(""));
				};

				DrawMarker(ActorHeadingTransform,                   FColorList::Black, TEXT("Actor"));
				DrawMarker(RootTransform,                           FColorList::Blue,  TEXT("Sequencer"));
				DrawMarker(LocomotionDelta * ActorHeadingTransform, FColorList::Red,   TEXT("Locomotion"));
				DrawMarker(BlendResult     * ActorHeadingTransform, FColorList::Green, TEXT("Result"));
			}
#endif

			RootMotionProvider->OverrideRootMotion(RootMotionDelta, (*Keyframe)->Attributes);
		}
	}

	if (FinalDestination != EMovieSceneRootMotionDestination::Attribute)
	{
		TWeakPtr<FMovieSceneMixerRootMotionComponentData> WeakRootMotion = RootMotionData;

		auto ApplyRootMotion = [RootTransform, WeakTarget = RootMotionData->Target]
		{
			USceneComponent* TargetComponent = WeakTarget.Get();
			if (TargetComponent && !GActorMovementTracker.IsActive())
			{
				TargetComponent->SetRelativeLocationAndRotation(RootTransform.GetLocation(), RootTransform.GetRotation().Rotator());
			}
		};
		FAnimNextModuleInstance::RunTaskOnGameThread(MoveTemp(ApplyRootMotion));
	}
}

UMovieSceneRootMotionSystem::UMovieSceneRootMotionSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();

	RelevantComponent = AnimMixerComponents->MixerRootMotion;
	Phase = ESystemPhase::Instantiation | ESystemPhase::Scheduling;

	SystemCategories |= FSystemInterrogator::GetExcludedFromInterrogationCategory();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// This must be run before the anim mixer to ensure that the anim mixer sets up its program correctly with root motion
		DefineImplicitPrerequisite(GetClass(), UMovieSceneAnimMixerSystem::StaticClass());

		DefineImplicitPrerequisite(UByteChannelEvaluatorSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UDoubleChannelEvaluatorSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UMovieScenePiecewiseDoubleBlenderSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UMovieSceneComponentTransformSystem::StaticClass(), GetClass());
	}
}

bool UMovieSceneRootMotionSystem::IsTransformKeyed(const FObjectKey& Object) const
{
	return ObjectsWithTransforms.Contains(Object);
}

void UMovieSceneRootMotionSystem::OnLink()
{
	UMovieSceneAnimMixerSystem* AnimMixer = Linker->LinkSystem<UMovieSceneAnimMixerSystem>();
	Linker->SystemGraph.AddReference(AnimMixer, this);

	UE::MovieScene::GActorMovementTracker.Initialize();
}

void UMovieSceneRootMotionSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();
	FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();
	TSharedRef<FMovieSceneEntitySystemRunner> Runner = Linker->GetRunner();

	if (Runner->GetCurrentPhase() == ESystemPhase::Instantiation)
	{
		UMovieSceneAnimMixerSystem* AnimMixer = Linker->FindSystem<UMovieSceneAnimMixerSystem>();
		if (!ensure(AnimMixer))
		{
			// Should never exist without the anim mixer
			return;
		}

		// Remove expiring root motions
		FEntityTaskBuilder()
		.Write(AnimMixerComponents->MixerRootMotion)
		.FilterAll({ BuiltInComponents->Tags.NeedsUnlink })
		.Iterate_PerEntity( &Linker->EntityManager,
			[](TSharedPtr<FMovieSceneMixerRootMotionComponentData>& OutRootMotion)
			{
				OutRootMotion = nullptr;
			}
		);

		// Set up root motion behaviors
		FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(BuiltInComponents->RootInstanceHandle)
		.Read(BuiltInComponents->BoundObjectKey)
		.Read(BuiltInComponents->BoundObject)
		.Write(AnimMixerComponents->Target)
		.Write(AnimMixerComponents->MixerRootMotion)
		.FilterAll({ AnimMixerComponents->RootDestination, BuiltInComponents->Tags.NeedsLink })
		.Iterate_PerAllocation( &Linker->EntityManager,
			[this, AnimMixer, BuiltInComponents](
				FEntityAllocationIteratorItem Item,
				TRead<FMovieSceneEntityID> EntityIDs,
				TRead<FRootInstanceHandle> RootInstanceHandles,
				TRead<FObjectKey> BoundObjectKeys,
				TRead<UObject*> BoundObjects,
				TWrite<TInstancedStruct<FMovieSceneMixedAnimationTarget>> OutTargets,
				TWrite<TSharedPtr<FMovieSceneMixerRootMotionComponentData>> OutRootMotions)
			{
				const FEntityAllocation* Allocation = Item.GetAllocation();
				const int32              Num        = Allocation->Num();


				// @todo: figure out restore state semantics with root motion
				const bool bWantsRestore = false; 
				const bool bCapturePreAnimatedState = Linker->PreAnimatedState.IsCapturingGlobalState() || bWantsRestore;

				FPreAnimatedEntityCaptureSource* EntityMetaData = nullptr;
				TSharedPtr<FPreAnimatedComponentTransformStorage> ComponentTransformStorage;
				if (bCapturePreAnimatedState)
				{
					EntityMetaData = Linker->PreAnimatedState.GetOrCreateEntityMetaData();
					ComponentTransformStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedComponentTransformStorage>();
				}

				static const FName PreAnimatedTransformName("Transform");
				for (int32 Index = 0; Index < Num; ++Index)
				{
					USceneComponent* BoundObject = Cast<USceneComponent>(BoundObjects[Index]);
					if (!BoundObject)
					{
						continue;
					}

					if (bCapturePreAnimatedState)
					{
						const FMovieSceneEntityID EntityID           = EntityIDs[Index];
						const FRootInstanceHandle RootInstanceHandle = RootInstanceHandles[Index];
						const FCachePreAnimatedValueParams CacheParams;

						// Track transform for the component and the root component
						FPreAnimatedStateEntry Entry = ComponentTransformStorage->MakeEntry(BoundObject, PreAnimatedTransformName);
						EntityMetaData->BeginTrackingEntity(Entry, EntityID, RootInstanceHandle, bWantsRestore);
						ComponentTransformStorage->CachePreAnimatedTransform(CacheParams, BoundObject);

						USceneComponent* RootComponent = BoundObject->GetOwner()->GetRootComponent();
						if (RootComponent && RootComponent != BoundObject)
						{
							FPreAnimatedStateEntry RootEntry = ComponentTransformStorage->MakeEntry(RootComponent, PreAnimatedTransformName);
							EntityMetaData->BeginTrackingEntity(RootEntry, EntityID, RootInstanceHandle, bWantsRestore);
							ComponentTransformStorage->CachePreAnimatedTransform(CacheParams, RootComponent);
						}
					}

					FObjectKey BoundObjectKey = BoundObjectKeys[Index];
					TInstancedStruct<FMovieSceneMixedAnimationTarget>& OutTarget = OutTargets[Index];
					TSharedPtr<FMovieSceneMixerRootMotionComponentData>& OutRootMotion = OutRootMotions[Index];

					// For new entities, we 'resolve' the animation target so if 'Automatic' is picked we choose the right one automatically.
					OutTarget = UMovieSceneAnimMixerSystem::ResolveAnimationTarget(BoundObjectKey, OutTarget);

					FMovieSceneAnimMixerKey Key{BoundObjectKey, OutTarget};

					if (OutRootMotion)
					{
						AnimMixer->AssignRootMotion(Key, OutRootMotion);
					}
					else if (TSharedPtr<FMovieSceneMixerRootMotionComponentData> RootMotion = AnimMixer->FindRootMotion(Key))
					{
						OutRootMotion = RootMotion;
					}
					else
					{
						// Make a new one
						OutRootMotion = MakeShared<FMovieSceneMixerRootMotionComponentData>();
						AnimMixer->AssignRootMotion(Key, OutRootMotion);
					}

					USceneComponent* Target = Cast<USceneComponent>(BoundObjectKey.ResolveObjectPtr());
					OutRootMotion->OriginalBoundObject = Target;
				}
			}
		);
	}

	ObjectsWithTransforms.Reset();

	// Gather which objects have transforms
	FEntityTaskBuilder()
	.Read(BuiltInComponents->BoundObjectKey)
	.FilterAll({ TrackComponents->ComponentTransform.PropertyTag, BuiltInComponents->CustomPropertyIndex })
	.FilterNone({ BuiltInComponents->Tags.NeedsUnlink })
	.Iterate_PerAllocation(&Linker->EntityManager,
		[this](const FEntityAllocation* Allocation, const FObjectKey* RootMotionTargets)
		{
			const int32 Num = Allocation->Num();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				this->ObjectsWithTransforms.Add(RootMotionTargets[Index]);
			}
		}
	);

	ObjectsWithTransforms.Shrink();
}

void UMovieSceneRootMotionSystem::OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();
	FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();

	UMovieSceneAnimMixerSystem* AnimMixer = Linker->FindSystem<UMovieSceneAnimMixerSystem>();
	if (!ensure(AnimMixer))
	{
		return;
	}

	FTaskID WaitForAllTransforms = FEntityTaskBuilder()
	.Write(BuiltInComponents->CustomPropertyIndex)
	.FilterAll({ TrackComponents->ComponentTransform.PropertyTag })
	.Fork_PerAllocation<FNoopTask>(&Linker->EntityManager, TaskScheduler);

	// Gather root destination results
	FTaskID GatherRootDestinationTask = FEntityTaskBuilder()
	.Read(BuiltInComponents->ByteResult)
	.Write(AnimMixerComponents->RootDestination)
	.Write(AnimMixerComponents->MixerRootMotion)
	.FilterNone({ BuiltInComponents->Tags.Ignored })
	.Schedule_PerEntity<FGatherRootDestinations>(&Linker->EntityManager, TaskScheduler);

	// Reset root motion data and gather current component/actor transforms
	FTaskParams InitialzeParams(TEXT("Initialize Root Motion"));
	InitialzeParams.ForceGameThread();
	InitialzeParams.bForcePropagateDownstream = true;
	FTaskID InitializeRootMotion = TaskScheduler->AddTask<FInitializeRootMotionTask>(InitialzeParams, AnimMixer);

	TaskScheduler->AddPrerequisite(WaitForAllTransforms,      InitializeRootMotion);
	TaskScheduler->AddPrerequisite(GatherRootDestinationTask, InitializeRootMotion);
}
