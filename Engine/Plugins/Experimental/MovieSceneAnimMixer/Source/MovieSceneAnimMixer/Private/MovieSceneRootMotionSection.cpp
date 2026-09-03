// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneRootMotionSection.h"

#include "AnimMixerComponentTypes.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "Evaluation/MovieSceneEvaluationField.h"
#include "Systems/MovieSceneSkeletalAnimationSystem.h"

UMovieSceneRootMotionSection::UMovieSceneRootMotionSection(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	RootDestinationChannel.SetEnum(StaticEnum<EMovieSceneRootMotionDestination>());
	RootDestinationChannel.SetDefault((uint8)EMovieSceneRootMotionDestination::RootBone);

	SectionRange.Value = TRange<FFrameNumber>::All();
}

EMovieSceneChannelProxyType UMovieSceneRootMotionSection::CacheChannelProxy()
{
	FMovieSceneChannelProxyData Channels;

#if WITH_EDITOR

	static FMovieSceneChannelMetaData MetaData("RootDestination", NSLOCTEXT("AnimMixer", "RootDestinationName", "Root Destination"));
	MetaData.bCanCollapseToTrack = true;

	Channels.Add(RootDestinationChannel, MetaData, TMovieSceneExternalValue<uint8>());

#else

	Channels.Add(RootDestinationChannel);

#endif

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
	return EMovieSceneChannelProxyType::Static;
}

int32 UMovieSceneRootMotionSection::GetRowSortOrder() const
{
	// Always sort root destination sections to the top
	return MIN_int32;
}

void UMovieSceneRootMotionSection::ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;

	const FBuiltInComponentTypes*   BuiltInComponents   = FBuiltInComponentTypes::Get();
	const FAnimMixerComponentTypes* AnimMixerComponents = FAnimMixerComponentTypes::Get();

	if (Params.GetObjectBindingID().IsValid())
	{
		UMovieSceneAnimationMixerTrack* AnimTrack = GetTypedOuter<UMovieSceneAnimationMixerTrack>();

		OutImportedEntity->AddBuilder(
			FEntityBuilder()
			.Add(BuiltInComponents->GenericObjectBinding, Params.GetObjectBindingID())
			.Add(BuiltInComponents->BoundObjectResolver, UMovieSceneSkeletalAnimationSystem::ResolveSkeletalMeshComponentBinding)
			.Add(AnimMixerComponents->Target, AnimTrack->MixedAnimationTarget)
			.Add(BuiltInComponents->ByteChannel, FSourceByteChannel{ &RootDestinationChannel })
			.AddDefaulted(AnimMixerComponents->RootDestination)
		);
	}
}

bool UMovieSceneRootMotionSection::PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder)
{
	if (RootDestinationChannel.HasAnyData())
	{
		const int32 MetaDataIndex = OutFieldBuilder->AddMetaData(InMetaData);
		OutFieldBuilder->AddPersistentEntity(EffectiveRange, this, 0, MetaDataIndex);
	}

	return true;
}
