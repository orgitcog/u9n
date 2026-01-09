// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneByteChannel.h"
#include "Decorations/IMovieSceneChannelDecoration.h"
#include "Decorations/IMovieSceneSectionDecoration.h"
#include "EntitySystem/IMovieSceneEntityDecorator.h"
#include "MovieSceneMixedAnimationTarget.h"
#include "MovieSceneNameableTrack.h"
#include "StructUtils/InstancedStruct.h"
#include "Tracks/MovieSceneCommonAnimationTrack.h"
#include "MovieSceneAnimationMixerTrack.generated.h"


// Enum describing what space that root motion should be applied in.
UENUM()
enum class EMovieSceneRootMotionSpace : uint8
{
	// Root motion should be applied in animation space, meaning that it will be applied on top of the blended transform track and transform origin.
	AnimationSpace,
	// Root motion should be applied in world space, meaning that it will override any transform track or transform origin.
	WorldSpace
};


// Enum describing what space that root motion should be applied in.
UENUM()
enum class EMovieSceneRootMotionTransformMode : uint8
{
	Asset,
	Offset,
	Override,
};


USTRUCT()
struct FMovieSceneByteChannelDefaultOnly : public FMovieSceneByteChannel
{
	GENERATED_BODY()

	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);
};

template<>
struct TStructOpsTypeTraits<FMovieSceneByteChannelDefaultOnly> : public TStructOpsTypeTraitsBase2<FMovieSceneByteChannelDefaultOnly>
{
	enum { WithStructuredSerializeFromMismatchedTag = true };
};

template<>
struct TMovieSceneChannelTraits<FMovieSceneByteChannelDefaultOnly> : TMovieSceneChannelTraitsBase<FMovieSceneByteChannelDefaultOnly>
{
#if WITH_EDITOR

	/** Byte channels can have external values (ie, they can get their values from external objects for UI purposes) */
	typedef TMovieSceneExternalValue<uint8> ExtendedEditorDataType;

#endif
};

UINTERFACE(MinimalAPI)
class UMovieSceneAnimationSectionInterface : public UInterface
{
	GENERATED_BODY()
};


class IMovieSceneAnimationSectionInterface
{
public:
	GENERATED_BODY()

	virtual int32 GetRowSortOrder() const = 0;
};

UCLASS(MinimalAPI)
class UMovieSceneAnimationSectionDecoration
	: public UMovieSceneSignedObject
	, public IMovieSceneSectionDecoration
	, public IMovieSceneAnimationSectionInterface
{
	GENERATED_BODY()

	virtual int32 GetRowSortOrder() const
	{
		return RowSortOrder;
	}

	UPROPERTY()
	int32 RowSortOrder = 0;
};

UCLASS(MinimalAPI)
class UMovieSceneAnimationBaseTransformDecoration
	: public UMovieSceneSignedObject
	, public IMovieSceneChannelDecoration
	, public IMovieSceneEntityDecorator
{
public:

	GENERATED_BODY()

	UMovieSceneAnimationBaseTransformDecoration(const FObjectInitializer& ObjInit);

	MOVIESCENEANIMMIXER_API EMovieSceneRootMotionTransformMode GetRootTransformMode() const;

	virtual EMovieSceneChannelProxyType PopulateChannelProxy(FMovieSceneChannelProxyData& OutProxyData) override;
	virtual void ExtendEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity) override;

	UPROPERTY()
	FMovieSceneDoubleChannel Location[3];

	UPROPERTY()
	FMovieSceneDoubleChannel Rotation[3];

	UPROPERTY(EditAnywhere, Category="Root Transform")
	FVector RootOriginLocation;

	UPROPERTY()
	FMovieSceneByteChannelDefaultOnly RootMotionSpace;

	UPROPERTY()
	FMovieSceneByteChannelDefaultOnly TransformMode;
};


/**
 *
 */
UCLASS(MinimalAPI)
class UMovieSceneAnimationMixerTrack
	: public UMovieSceneCommonAnimationTrack
{
	GENERATED_BODY()

public:

	UMovieSceneAnimationMixerTrack(const FObjectInitializer& Init);

	virtual bool FixRowIndices() override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;

	virtual void OnSectionAddedImpl(UMovieSceneSection* Secton) override;

#if WITH_EDITORONLY_DATA

	virtual FText GetTrackRowDisplayName(int32 RowIndex) const override;
	virtual FText GetDefaultDisplayName() const override;
	virtual bool CanRename() const override;
#endif

public:

	UPROPERTY(EditAnywhere, Category = "Mixed Animation")
	TInstancedStruct<FMovieSceneMixedAnimationTarget> MixedAnimationTarget;
};
