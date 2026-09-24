// Copyright Epic Games, Inc. All Rights Reserved.


#include "Tools/EvaluateSequencerTools.h"
#include "ControlRig.h"
#include "ISequencer.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MovieSceneToolHelpers.h"
#include "ControlRigObjectBinding.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "ILevelSequenceEditorToolkit.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Transform/TransformConstraint.h"
#include "ConstraintsManager.h"
#include "Constraints/MovieSceneConstraintChannelHelper.h"
#include "Constraints/ControlRigTransformableHandle.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieScene3DAttachTrack.h"
#include "Tracks/MovieScene3DPathTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieScene3DConstraintSection.h"
#include "Constraints/TransformConstraintChannelInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Transform/TransformConstraintUtil.h"

class UControlRig;


namespace UE
{
namespace AIE
{
	FTransform FArrayOfTransforms::Interp(const FFrameNumber& InTime, const TArray<int32>& TransformIndices, const TArray<FFrameNumber>& CurrentFrames)
	{
		FTransform Value = FTransform::Identity;
		const int32 StartIndex = Algo::LowerBound(CurrentFrames, InTime);
		if (StartIndex != INDEX_NONE)
		{
			if (StartIndex >= CurrentFrames.Num())
			{
				const int32 Index = TransformIndices[CurrentFrames.Num() -1];
				Value = Transforms[Index];
			}
			else if (InTime == CurrentFrames[StartIndex] ||
				(StartIndex + 1) == CurrentFrames.Num())
			{
				const int32 Index = TransformIndices[StartIndex];
				Value = Transforms[Index];
			}
			else
			{
				const FFrameNumber Frame1 = CurrentFrames[StartIndex];
				const FFrameNumber Frame2 = CurrentFrames[StartIndex + 1];
				if (Frame1 != Frame2) //should never happen ... but?
				{
					const double FrameDiff = (double)(Frame2.Value - Frame1.Value);
					const double T = (double)(InTime.Value - Frame1.Value) / FrameDiff;
					const int32 Index1 = TransformIndices[StartIndex];
					const int32 Index2 = TransformIndices[StartIndex + 1];

					FTransform KeyAtom1 = Transforms[Index1];
					FTransform KeyAtom2 = Transforms[Index1];

					KeyAtom1.NormalizeRotation();
					KeyAtom2.NormalizeRotation();

					Value.Blend(KeyAtom1, KeyAtom2, T);
				}
			}
		}
		return Value;
	}


	static bool GetActorWorldTransform(const FActorForWorldTransforms& ActorSelection, FTransform& OutTransform, FTransform& OutParentTransform)
	{
		USceneComponent* SceneComponent = nullptr;
		AActor* Actor = ActorSelection.Actor.Get();
		if (Actor)
		{
			SceneComponent = Actor->GetRootComponent();
		}
		else
		{
			SceneComponent = ActorSelection.Component.IsValid() ? Cast<USceneComponent>(ActorSelection.Component.Get()) : nullptr;
			if (SceneComponent)
			{
				Actor = SceneComponent->GetTypedOuter<AActor>();
			}
		}

		if (Actor && SceneComponent)
		{
			USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(SceneComponent);

			if (!SkelMeshComp)
			{
				SkelMeshComp = MovieSceneToolHelpers::AcquireSkeletalMeshFromObject(Actor);
			}

			AActor* Parent = ActorSelection.Actor.Get();
			while (Parent)
			{
				TArray<USkeletalMeshComponent*> MeshComps;
				Parent->GetComponents(MeshComps, true);

				for (USkeletalMeshComponent* MeshComp : MeshComps)
				{
					MeshComp->TickAnimation(0.0f, false);
					MeshComp->RefreshBoneTransforms();
					MeshComp->RefreshFollowerComponents();
					MeshComp->UpdateComponentToWorld();
					MeshComp->FinalizeBoneTransform();
					//WIP here for flashing mannies
					//MeshComp->MarkRenderTransformDirty();
					//MeshComp->MarkRenderDynamicDataDirty();
				}

				Parent = Parent->GetAttachParentActor();
			}

			OutTransform = (SkelMeshComp && ActorSelection.SocketName != NAME_None)
				? SkelMeshComp->GetSocketTransform(ActorSelection.SocketName)
				: SceneComponent->GetComponentToWorld();

			AActor* ParentActor = Actor->GetAttachParentActor();
			if (ParentActor)
			{
				SceneComponent = ParentActor->GetRootComponent();
				SkelMeshComp = Cast<USkeletalMeshComponent>(SceneComponent);

				if (!SkelMeshComp)
				{
					SkelMeshComp = MovieSceneToolHelpers::AcquireSkeletalMeshFromObject(Actor);
				}
				FName SocketName = Actor->GetAttachParentSocketName();

				OutParentTransform = (SkelMeshComp && SocketName != NAME_None)
					? SkelMeshComp->GetSocketTransform(SocketName)
					: SceneComponent->GetComponentToWorld();
			}
			else
			{
				OutParentTransform = FTransform::Identity;	
			}
			return true;
		}
		return false;
	}

	static bool GetControlRigParentTransform(UControlRig* ControlRig, FTransform& OutTransform)
	{
		if (!ControlRig)
		{
			return false;
		}
		if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
		{
			USceneComponent* Component = Cast<USceneComponent>(ObjectBinding->GetBoundObject());
			if (!Component)
			{
				return false;
			}
			AActor* Actor = Component->GetTypedOuter< AActor >();
			if (!Actor)
			{
				return false;
			}
			FActorForWorldTransforms ActorSelection;
			ActorSelection.Actor = Actor;
			FTransform OutParentTransform;
			return(GetActorWorldTransform(ActorSelection, OutTransform, OutParentTransform));
		}
		return false;
	}

	static bool GetControlRigWorldTransform(UControlRig* ControlRig, const FName& ControlName, const FTransform& ParentTransform, FTransform& OutTransform)
	{
		if (ControlRig->FindControl(ControlName) == nullptr)
		{
			return false;
		}
		OutTransform = ControlRig->GetControlGlobalTransform(ControlName) * ParentTransform;
		return true;
	}

	bool FEvalHelpers::CalculateWorldTransforms(UWorld* World, ISequencer* Sequencer, const UE::AIE::FFrameTimeByIndex& FrameTimeByIndex,
		const TArray<int32>& Indices, TArray<UE::AIE::FActorAndWorldTransforms>& Actors, TMap<UControlRig*, UE::AIE::FControlRigAndWorldTransforms>& ControlRigs,
		const TSet<const UMovieSceneTrack*>& DependentTracks)
	{
		if (World == nullptr || Sequencer == nullptr || Sequencer->GetFocusedMovieSceneSequence() == nullptr
			|| Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene() == nullptr)
		{
			return false;
		}
		FMovieSceneSequenceIDRef Template = Sequencer->GetFocusedTemplateID();
		FMovieSceneSequenceTransform RootToLocalTransform = Sequencer->GetFocusedMovieSceneSequenceTransform();
		FMovieSceneInverseSequenceTransform LocalToRootTransform = RootToLocalTransform.Inverse();
		UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FFrameRate DisplayRate = MovieScene->GetDisplayRate();


		TSet<UControlRig*> ControlsToRevalaute;
		
		for (int32 Index : Indices)
		{
			const FFrameNumber FrameNumber = FrameTimeByIndex.CalculateFrame(Index);
			FFrameTime GlobalTime = LocalToRootTransform.TryTransformTime(FrameNumber).Get(FrameNumber); //player evals in root time so need to go back to it.

			FMovieSceneContext Context = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Sequencer->GetPlaybackStatus()).SetHasJumped(true);
			Sequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context);

			Controller.EvaluateAllConstraints();

			for (FActorAndWorldTransforms& Actor : Actors)
			{
				FTransform& Transform = Actor.WorldTransforms->Transforms[Index];
				FTransform& ParentTransform = Actor.ParentTransforms->Transforms[Index];

				GetActorWorldTransform(Actor.Actor, Transform,ParentTransform);
			}

			const float DeltaTime = 0.033f;
			for (TPair<UControlRig*, FControlRigAndWorldTransforms>& CR : ControlRigs)
			{
				if (UControlRig* ControlRig = CR.Value.ControlRig.Get())
				{
					FTransform& ParentTransform = CR.Value.ParentTransforms->Transforms[Index];
					ParentTransform = FTransform::Identity;
					if (GetControlRigParentTransform(ControlRig, ParentTransform))
					{
						ControlsToRevalaute.Add(ControlRig);
						if (ControlRig->IsAdditive())
						{
							ControlRig->EvaluateSkeletalMeshComponent(DeltaTime);
						}
						else
						{
							ControlRig->Evaluate_AnyThread();
						}
						for (TPair<FName, TSharedPtr<FArrayOfTransforms>>& Pair : CR.Value.ControlAndWorldTransforms)
						{
							FTransform& Transform = Pair.Value->Transforms[Index];
							GetControlRigWorldTransform(ControlRig, Pair.Key, ParentTransform, Transform);
						}
					}
				}
			}
		}
		
		Sequencer->ForceEvaluate();
		//we now need to make sure the control rigs are back up to date.
		//we do this by going through the control rig tracks that the actors/rigs are dependent up

		for (const UMovieSceneTrack* Track : DependentTracks)
		{
			if (const UMovieSceneControlRigParameterTrack* CRTrack = Cast<UMovieSceneControlRigParameterTrack>(Track))
			{
				if (UControlRig* ControlRig = CRTrack->GetControlRig())
				{
					ControlRig->Evaluate_AnyThread();
					if (ControlRig->GetObjectBinding())
					{
						ControlRig->EvaluateSkeletalMeshComponent(0.0);
					}
				}
			}
		}
		
		return true;
	}

	static FTransform GetThisControlRigComponentTransform(UControlRig* ControlRig)
	{
		FTransform Transform = FTransform::Identity;
		TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding();
		if (ObjectBinding.IsValid())
		{
			if (USceneComponent* BoundSceneComponent = Cast<USceneComponent>(ObjectBinding->GetBoundObject()))
			{
				return BoundSceneComponent->GetComponentTransform();
			}
		}
		return Transform;
	}

	bool FSetTransformHelpers::SetConstrainedTransform(FTransform LocalTransform, UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& InContext)
	{
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(ControlRig->GetWorld());
		const uint32 ControlHash = UTransformableControlHandle::ComputeHash(ControlRig, ControlElement->GetFName());
		const TArray< TWeakObjectPtr<UTickableConstraint> > Constraints = Controller.GetParentConstraints(ControlHash, true);
		if (Constraints.IsEmpty())
		{
			return false;
		}
		const int32 LastActiveIndex = UE::TransformConstraintUtil::GetLastActiveConstraintIndex(Constraints);
		const bool bNeedsConstraintPostProcess = Constraints.IsValidIndex(LastActiveIndex);

		if (!bNeedsConstraintPostProcess)
		{
			return false;
		}
		static constexpr bool bNotify = true, bFixEuler = true, bUndo = true;
		FRigControlModifiedContext Context = InContext;
		Context.EventName = FRigUnit_BeginExecution::EventName;
		Context.bConstraintUpdate = true;
		Context.SetKey = EControlRigSetKey::Never;

		// set the global space, assumes it's attached to actor
		// no need to compensate for constraints here, this will be done after when setting the control in the constraint space
		{
			TGuardValue<bool> CompensateGuard(FMovieSceneConstraintChannelHelper::bDoNotCompensate, true);
			ControlRig->SetControlLocalTransform(
				ControlElement->GetKey().Name, LocalTransform, bNotify, Context, bUndo, bFixEuler);
		}
		FTransform GlobalTransform = ControlRig->GetControlGlobalTransform(ControlElement->GetKey().Name);

		// switch to constraint space
		FTransform ToWorldTransform = GetThisControlRigComponentTransform(ControlRig);
		const FTransform WorldTransform = GlobalTransform * ToWorldTransform;

		const TOptional<FTransform> RelativeTransform =
			UE::TransformConstraintUtil::GetConstraintsRelativeTransform(Constraints, LocalTransform, WorldTransform);
		if (RelativeTransform)
		{
			LocalTransform = *RelativeTransform;
		}

		Context.bConstraintUpdate = false;
		Context.SetKey = InContext.SetKey;
		ControlRig->SetControlLocalTransform(ControlElement->GetKey().Name, LocalTransform, bNotify, Context, bUndo, bFixEuler);
		ControlRig->Evaluate_AnyThread();
		Controller.EvaluateAllConstraints();

		return true;
	}


	void FSetTransformHelpers::SetControlTransform(const FEulerTransform& EulerTransform, UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context)
	{
		if (ControlElement && ControlRig)
		{
			FVector TLocation = EulerTransform.GetLocation();
			FRotator TRotation = EulerTransform.Rotation;
			FVector TScale = EulerTransform.Scale;
			FTransform RealTransform(TRotation, TLocation, TScale);
			if (SetConstrainedTransform(RealTransform, ControlRig, ControlElement, Context))
			{
				return;
			}
			switch (ControlElement->Settings.ControlType)
			{
			case ERigControlType::Transform:
			{
				FVector EulerAngle(TRotation.Roll, TRotation.Pitch, TRotation.Yaw);
				ControlRig->GetHierarchy()->SetControlSpecifiedEulerAngle(ControlElement, EulerAngle);
				ControlRig->SetControlValue<FRigControlValue::FTransform_Float>(ControlElement->GetKey().Name, RealTransform, true, Context, false);
				ControlRig->GetHierarchy()->SetControlSpecifiedEulerAngle(ControlElement, EulerAngle);
				break;

			}
			case ERigControlType::TransformNoScale:
			{
				FTransformNoScale NoScale(TLocation, TRotation.Quaternion());
				ControlRig->SetControlValue<FRigControlValue::FTransformNoScale_Float>(ControlElement->GetKey().Name, NoScale, true, Context, false);
				break;

			}
			case ERigControlType::EulerTransform:
			{
				URigHierarchy* Hierarchy = ControlRig->GetHierarchy();

				if (Hierarchy->UsesPreferredEulerAngles())
				{
					FVector EulerAngle(TRotation.Roll, TRotation.Pitch, TRotation.Yaw);
					FQuat Quat = Hierarchy->GetControlQuaternion(ControlElement, EulerAngle);
					Hierarchy->SetControlSpecifiedEulerAngle(ControlElement, EulerAngle);
					FRotator UERotator(Quat);
					FEulerTransform UETransform(UERotator, TLocation, TScale);
					UETransform.Rotation = UERotator;
					ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlElement->GetKey().Name, UETransform, true, Context, false);
					Hierarchy->SetControlSpecifiedEulerAngle(ControlElement, EulerAngle);
				}
				else
				{
					ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlElement->GetKey().Name, FEulerTransform(RealTransform), true, Context, false);
				}
				break;
			}
			}
			ControlRig->Evaluate_AnyThread();
		}
	}

	bool FSetTransformHelpers::SetActorTransform(ISequencer* Sequencer, USceneComponent* SceneComponent, UMovieScene3DTransformSection* TransformSection, const TArray<FFrameNumber>& Frames,
		const TArray<FTransform>& WorldTransformsToSnapTo, const TArray<FTransform>& ParentWorldTransforms)
	{

		if (!Sequencer || !Sequencer->GetFocusedMovieSceneSequence())
		{
			return false;
		}
		if (!TransformSection)
		{
			return false;
		}
		if (!SceneComponent)
		{
			return false;
		}

		AActor* Actor = SceneComponent->GetTypedOuter<AActor>();

		TransformSection->Modify();

		FMovieSceneInverseSequenceTransform LocalToRootTransform = Sequencer->GetFocusedMovieSceneSequenceTransform().Inverse();
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

		//adjust keys for constraints
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(Actor->GetWorld());
		TArrayView<FMovieSceneDoubleChannel*> Channels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
		const bool bhasContraint = TransformSection->GetConstraintsChannels().Num() > 0;

		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			const FFrameNumber& Frame = Frames[Index];
			FTransform ParentTransform = ParentWorldTransforms.Num() > 0 ? ParentWorldTransforms[Index] : FTransform::Identity;
			FTransform WorldTransform = WorldTransformsToSnapTo[Index];
			FTransform LocalTransform = WorldTransform.GetRelativeTransform(ParentTransform);
			
			//todo test FFrameTime GlobalTime = LocalToRootTransform.TryTransformTime(Frame).Get(Frame);
			FFrameTime GlobalTime(Frame);
			const FFrameNumber GlobalFrame = GlobalTime.GetFrame();

			/*
			// Account for the transform origin only if this is not parented because the transform origin is already being applied to the parent.
			if (!SceneComponent->GetAttachParent() && Section->GetBlendType() == EMovieSceneBlendType::Absolute)
			{
				CurrentTransform *= GetTransformOrigin().Inverse();
			}
			*/

			if (bhasContraint)
			{
				FMovieSceneContext Context = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Sequencer->GetPlaybackStatus()).SetHasJumped(true);
				Sequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context);
				Controller.EvaluateAllConstraints();
				//UE::TransformConstraintUtil::UpdateTransformBasedOnConstraint(LocalTransform, SceneComponent);
			//	if (AActor* Actor = SceneComponent->GetTypedOuter<AActor>())
				{
					TArray< TWeakObjectPtr<UTickableConstraint> > Constraints;
					UE::TransformConstraintUtil::GetParentConstraints(SceneComponent->GetWorld(), Actor, Constraints);

					const int32 LastActiveIndex = UE::TransformConstraintUtil::GetLastActiveConstraintIndex(Constraints);
					if (Constraints.IsValidIndex(LastActiveIndex))
					{
						// switch to constraint space
						const TOptional<FTransform> RelativeTransform = UE::TransformConstraintUtil::GetConstraintsRelativeTransform(Constraints, LocalTransform, WorldTransform);
						if (RelativeTransform)
						{
							LocalTransform = *RelativeTransform;
						}
					}
				}
			}		

			FVector Location = LocalTransform.GetLocation();
			FRotator Rotation = LocalTransform.GetRotation().Rotator();

			TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = Channels[0]->GetData();
			MovieSceneToolHelpers::SetOrAddKey(ChannelData, GlobalFrame, Location.X);
			ChannelData = Channels[1]->GetData();
			MovieSceneToolHelpers::SetOrAddKey(ChannelData, GlobalFrame, Location.Y);
			ChannelData = Channels[2]->GetData();
			MovieSceneToolHelpers::SetOrAddKey(ChannelData, GlobalFrame, Location.Z);
		}

		Channels[0]->AutoSetTangents();
		Channels[1]->AutoSetTangents();
		Channels[2]->AutoSetTangents();

		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
		return true;
	}

	FMovieSceneBinding FSequencerTransformDependencies::GetBindingFromTrack(UMovieScene* InMovieScene, UMovieSceneTrack* Track)
	{
		const TArray<FMovieSceneBinding>& Bindings = InMovieScene->GetBindings();
		for (const FMovieSceneBinding& Binding : Bindings)
		{
			for (UMovieSceneTrack* PossibleTrack : Binding.GetTracks())
			{
				if (PossibleTrack == Track)
				{
					return Binding;
				}
			}
		}
		FMovieSceneBinding EmptyBinding;
		return EmptyBinding;
	}

	TArray<UMovieSceneTrack*> FSequencerTransformDependencies::GetTransformAffectingTracks(UMovieScene* InMovieScene, const FMovieSceneBinding& Binding)
	{
		TArray<UMovieSceneTrack*> TransformTracks;
		for (UMovieSceneTrack* PossibleTrack : Binding.GetTracks())
		{
			if (PossibleTrack)
			{
				if (PossibleTrack->GetClass() == UMovieScene3DTransformTrack::StaticClass() ||
					PossibleTrack->GetClass() == UMovieSceneControlRigParameterTrack::StaticClass() ||
					PossibleTrack->GetClass() == UMovieScene3DAttachTrack::StaticClass() ||
					PossibleTrack->GetClass() == UMovieScene3DPathTrack::StaticClass() ||
					PossibleTrack->GetClass() == UMovieSceneSkeletalAnimationTrack::StaticClass()
					)
				{
					TransformTracks.Add(PossibleTrack);
				}
			}
		}
		return TransformTracks;
	}

	void FSequencerTransformDependencies::CalculateDependencies(ISequencer* InSequencer, AActor* InActor, TArray<UMovieSceneTrack*>& InTracks)
	{
		Tracks.Reset();
		SequencerActors.Reset();
		NonSequencerActors.Reset();
		if (InSequencer == nullptr)
		{
			return;
		}
		UMovieSceneSequence* Sequence = InSequencer->GetFocusedMovieSceneSequence();
		if (Sequence == nullptr || Sequence->GetMovieScene() == nullptr)
		{
			return;
		}
		//find non-sequencer parents
		TArray<const UObject*> Parents;
		MovieSceneToolHelpers::GetParents(Parents, InActor);
		//find non sequencer parents
		for (const UObject* Parent : Parents)
		{
			//unfortunately uses non-const
			if (Parent != InActor)
			{
				UObject* NonConstObj = const_cast<UObject*>(Parent);
				FGuid Binding = Sequence->FindBindingFromObject(NonConstObj, InSequencer->GetSharedPlaybackState());
				if (Binding.IsValid() == false)
				{
					if (AActor* Actor = Cast<AActor>(NonConstObj))
					{
						NonSequencerActors.Add(Actor);
					}
					else if (AActor* OuterActor = NonConstObj->GetTypedOuter<AActor>())
					{
						NonSequencerActors.Add(OuterActor);
					}
				}
				else
				{
					if (AActor* Actor = Cast<AActor>(NonConstObj))
					{
						SequencerActors.Add(Actor, Binding);
					}
					else if (AActor* OuterActor = NonConstObj->GetTypedOuter<AActor>())
					{
						SequencerActors.Add(OuterActor, Binding);
					}
				}
			}
		}
		//now for each track see if has any constraints/spaces
		for (UMovieSceneTrack* Track : InTracks)
		{
			AddTrack(InSequencer, Track);
		}
		//now get tracks from parents, only add them though if not in the list, they may be there because of attach/constraint tracks and current evaluation
		UMovieScene* MovieScene = Sequence->GetMovieScene();

		for (TPair<AActor*,FGuid>& Pair : SequencerActors)
		{
			if (FMovieSceneBinding* Binding = MovieScene->FindBinding(Pair.Value))
			{
				TArray<UMovieSceneTrack*> TransformTracks = GetTransformAffectingTracks(MovieScene, *Binding);
				for (UMovieSceneTrack* PossibleTrack : TransformTracks)
				{
					if (Tracks.Contains(PossibleTrack) == false)
					{
						AddTrack(InSequencer, PossibleTrack);
					}
				}
			}
		}
	}

	void FSequencerTransformDependencies::CopyFrom(const FSequencerTransformDependencies& Other)
	{
		SequencerActors = Other.SequencerActors;
		NonSequencerActors = Other.NonSequencerActors;
		Tracks = Other.Tracks;
	}


	bool FSequencerTransformDependencies::Compare(FSequencerTransformDependencies& Other) const
	{
		if (SequencerActors.Num() != Other.SequencerActors.Num())
		{
			return false;
		}
		else
		{
			for (TPair<AActor*, FGuid>& Pair : Other.SequencerActors)
			{
				if (SequencerActors.Contains(Pair.Key) == false)
				{
					return false;
				}
			}
		}
		if (NonSequencerActors.Num() != Other.NonSequencerActors.Num())
		{
			return false;
		}
		else
		{
			for (AActor* Actor : Other.NonSequencerActors)
			{
				if (NonSequencerActors.Contains(Actor) == false)
				{
					return false;
				}
			}
		}
		if (Tracks.Num() != Other.Tracks.Num())
		{
			return false;
		}
		else
		{
			for (TPair<TWeakObjectPtr<UMovieSceneTrack>,FGuid>& Pair : Other.Tracks)
			{
				const FGuid* Guid = Tracks.Find(Pair.Key);
				if (Guid == nullptr)
				{
					return false;
				}
				else
				{
					if (*Guid != Pair.Value)
					{
						return false;
					}
				}
			}
		}
		//get here then the same
		return true;
	}

	//track may be an attach/consraint track or a cr/tranfsorm track with constraint channels
	void FSequencerTransformDependencies::CalculateTrackDependents(ISequencer* InSequencer, UMovieSceneTrack* InTrack)
	{
		UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		if (UMovieScene3DConstraintTrack* ConstraintTrack = Cast<UMovieScene3DConstraintTrack>(InTrack))
		{
			for (UMovieSceneSection* ConstraintSection : ConstraintTrack->GetAllSections())
			{
				FMovieSceneObjectBindingID ConstraintBindingID = (Cast<UMovieScene3DConstraintSection>(ConstraintSection))->GetConstraintBindingID();
				if (FMovieSceneBinding* Binding = MovieScene->FindBinding(ConstraintBindingID.GetGuid()))
				{
					TArray<UMovieSceneTrack*> TransformTracks = GetTransformAffectingTracks(MovieScene,*Binding);
					for (UMovieSceneTrack* PossibleTrack : TransformTracks)
					{
						AddTrack(InSequencer, PossibleTrack); 

					}
				}
			}
		}
		for (UMovieSceneSection* Section : InTrack->GetAllSections())
		{
			if (IMovieSceneConstrainedSection* ConstrainedSection = Cast<IMovieSceneConstrainedSection>(Section))
			{
				for (FConstraintAndActiveChannel& ConstraintChannel : ConstrainedSection->GetConstraintsChannels())
				{
					if (UTickableTransformConstraint* TransformConstraint = Cast< UTickableTransformConstraint>(ConstraintChannel.GetConstraint()))
					{
						const FConstraintChannelInterfaceRegistry& InterfaceRegistry = FConstraintChannelInterfaceRegistry::Get();
						ITransformConstraintChannelInterface* ParentInterface = InterfaceRegistry.FindConstraintChannelInterface(TransformConstraint->ParentTRSHandle->GetClass());
						if (ParentInterface)
						{
							if (UMovieSceneSection* ParentSection = ParentInterface->GetHandleSection(TransformConstraint->ParentTRSHandle, InSequencer->AsShared()))
							{
								if (UMovieSceneTrack* ParentTrack = ParentSection->GetTypedOuter<UMovieSceneTrack>())
								{
									AddTrack(InSequencer, ParentTrack); //always add this track it may be a control rig track
									FMovieSceneBinding Binding = GetBindingFromTrack(MovieScene, ParentTrack);
									if (Binding.GetObjectGuid().IsValid())
									{
										TArray<UMovieSceneTrack*> TransformTracks = GetTransformAffectingTracks(MovieScene, Binding);
										for (UMovieSceneTrack* PossibleTrack : TransformTracks)
										{
											AddTrack(InSequencer, PossibleTrack); 
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	void FSequencerTransformDependencies::AddTrack(ISequencer* InSequencer, UMovieSceneTrack* InTrack)
	{
		if (Tracks.Contains(InTrack) == false)
		{
			Tracks.Add(InTrack, InTrack->GetSignature());
			CalculateTrackDependents(InSequencer, InTrack);
		}
	}

} // namespace AIE
} // namespace UE

