// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Channels/MovieSceneByteChannel.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "MovieSceneSection.h"
#include "MovieSceneAnimationMixerTrack.h"
#include "MovieSceneRootMotionSection.generated.h"

UENUM()
enum class EMovieSceneRootMotionDestination : uint8
{
	/** Throw away any transform on the root bone */
	Discard,
	/** Leave the root bone with whatever transform it ended up with after evaluation */
	RootBone,
	/** Copy the root bone's transform onto the owning Component, and reset the root transform */
	Component,
	/** Copy the root bone's transform onto the owning Actor, and reset the root transform */
	Actor,
	/** Leave the root motion transform on an attribute for external systems to pick up */
	Attribute,
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName="Root Motion")
class UMovieSceneRootMotionSection
	: public UMovieSceneSection
	, public IMovieSceneEntityProvider
	, public IMovieSceneAnimationSectionInterface
{
	GENERATED_BODY()

public:

	UMovieSceneRootMotionSection(const FObjectInitializer& Init);

	virtual int32 GetRowSortOrder() const;

	virtual EMovieSceneChannelProxyType CacheChannelProxy() override;

	virtual void ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity) override;
	virtual bool PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder) override;

private:

	UPROPERTY()
	FMovieSceneByteChannel RootDestinationChannel;
};
