// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanSequence.h"

#include "MetaHumanAudioTrack.h"
#include "MetaHumanMovieSceneMediaTrack.h"
#include "CaptureData.h"

#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"

#include "Animation/AnimInstance.h"
#include "MovieScene.h"
#include "ImgMediaSource.h"
#include "Components/SkeletalMeshComponent.h"

UMetaHumanSceneSequence::UMetaHumanSceneSequence(const FObjectInitializer& ObjectInitializer)
	: Super{ ObjectInitializer }
	, MovieScene{ nullptr }
{
}

void UMetaHumanSceneSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
	if (Context != nullptr)
	{
		Bindings.Emplace(ObjectId, &PossessedObject);
	}
}

bool UMetaHumanSceneSequence::CanPossessObject(UObject& Object, UObject* InPlaybackContext) const
{
	return Object.IsA<AActor>() || Object.IsA<UActorComponent>() || Object.IsA<UAnimInstance>();
}

void UMetaHumanSceneSequence::LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	if (Context != nullptr && Bindings.Contains(ObjectId))
	{
		OutObjects.Add(Bindings[ObjectId]);
	}
}

UMovieScene* UMetaHumanSceneSequence::GetMovieScene() const
{
	checkf(MovieScene != nullptr, TEXT("Invalid MovieScene"));
	return MovieScene;
}

UObject* UMetaHumanSceneSequence::GetParentObject(UObject* Object) const
{
	if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		return Component->GetOwner();
	}

	if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(Object))
	{
		if (AnimInstance->GetWorld())
		{
			return AnimInstance->GetOwningComponent();
		}
	}

	return nullptr;
}

void UMetaHumanSceneSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
	if (Bindings.Contains(ObjectId))
	{
		Bindings.Remove(ObjectId);
	}
}

void UMetaHumanSceneSequence::UnbindObjects(const FGuid&, const TArray<UObject*>&, UObject*)
{
}

void UMetaHumanSceneSequence::UnbindInvalidObjects(const FGuid&, UObject*)
{
}

#if WITH_EDITOR

FText UMetaHumanSceneSequence::GetDisplayName() const
{
	return NSLOCTEXT("MetaHumanSequence", "DisplayName", "MetaHuman Sequence");
}

ETrackSupport UMetaHumanSceneSequence::IsTrackSupportedImpl(TSubclassOf<class UMovieSceneTrack> InTrackClass) const
{
	if (InTrackClass == UMetaHumanMovieSceneMediaTrack::StaticClass() ||
		InTrackClass == UMetaHumanAudioTrack::StaticClass() ||
		InTrackClass == UMovieSceneSkeletalAnimationTrack::StaticClass() ||
		InTrackClass == UMovieSceneControlRigParameterTrack::StaticClass())
	{
		return ETrackSupport::Supported;
	}

	return Super::IsTrackSupportedImpl(InTrackClass);
}

void UMetaHumanSceneSequence::SetTickRate(UFootageCaptureData* InFootageCaptureData)
{
	check(MovieScene);

	// Set a sequence tick rate that is appropriate for the footage.
	// If the footage is an integer frame rate, then the tick rate of 24000/1 is best
	// since that supports a range of rates, eg mixing 24fps and 60fps media.
	// However, for fractional rates then set the tick rate to be the video frame rate
	// to allow for frame accurate transport. This comes at the cost of being able
	// to use media of mixed frame rates.

	FFrameRate TickRate = FFrameRate(24000, 1);

	if (InFootageCaptureData != nullptr && InFootageCaptureData->IsInitialized(UCaptureData::EInitializedCheck::ImageSequencesOnly))
	{
		FFrameRate FrameRate = InFootageCaptureData->ImageSequences[0]->FrameRateOverride;

		if (FrameRate.IsValid())
		{
			double DecimalFrameRate = FrameRate.AsDecimal();

			if (DecimalFrameRate != int32(DecimalFrameRate))
			{
				TickRate = FrameRate;
			}
		}
	}

	MovieScene->SetTickResolutionDirectly(TickRate);
}

#endif // WITH_EDITOR
