// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceSequence.h"

#include "MetaHumanPerformanceMovieSceneMediaTrack.h"
#include "MetaHumanPerformanceMovieSceneAudioTrack.h"

#include "Tracks/MovieSceneSkeletalAnimationTrack.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

UMetaHumanPerformanceSequence::UMetaHumanPerformanceSequence(const FObjectInitializer& ObjectInitializer)
	: Super{ ObjectInitializer }
	, MovieScene{ nullptr }
{
}

void UMetaHumanPerformanceSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
	if (Context != nullptr)
	{
		Bindings.Emplace(ObjectId, &PossessedObject);
	}
}

bool UMetaHumanPerformanceSequence::CanPossessObject(UObject& Object, UObject* InPlaybackContext) const
{
	return Object.IsA<AActor>() || Object.IsA<UActorComponent>() || Object.IsA<UAnimInstance>();
}

void UMetaHumanPerformanceSequence::LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	if (Context != nullptr && Bindings.Contains(ObjectId))
	{
		OutObjects.Add(Bindings[ObjectId]);
	}
}

UMovieScene* UMetaHumanPerformanceSequence::GetMovieScene() const
{
	checkf(MovieScene != nullptr, TEXT("Invalid MovieScene"));
	return MovieScene;
}

UObject* UMetaHumanPerformanceSequence::GetParentObject(UObject* Object) const
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

void UMetaHumanPerformanceSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
	if (Bindings.Contains(ObjectId))
	{
		Bindings.Remove(ObjectId);
	}
}

void UMetaHumanPerformanceSequence::UnbindObjects(const FGuid&, const TArray<UObject*>&, UObject*)
{
}

void UMetaHumanPerformanceSequence::UnbindInvalidObjects(const FGuid&, UObject*)
{
}

#if WITH_EDITOR

FText UMetaHumanPerformanceSequence::GetDisplayName() const
{
	return NSLOCTEXT("MetaHumanPerformanceSequence", "DisplayName", "MetaHuman Performance Sequence");
}

ETrackSupport UMetaHumanPerformanceSequence::IsTrackSupportedImpl(TSubclassOf<class UMovieSceneTrack> InTrackClass) const
{
	if (InTrackClass == UMetaHumanPerformanceMovieSceneMediaTrack::StaticClass() ||
		InTrackClass == UMetaHumanPerformanceMovieSceneAudioTrack::StaticClass() ||
		InTrackClass == UMovieSceneSkeletalAnimationTrack::StaticClass())
	{
		return ETrackSupport::Supported;
	}

	return Super::IsTrackSupportedImpl(InTrackClass);
}

#endif // WITH_EDITOR
