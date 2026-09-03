// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSequence.h"
#include "FrameRange.h"

#include "MetaHumanSequence.generated.h"

/**
 * Movie scene used by the MetaHuman system
 */
UCLASS()
class METAHUMANSEQUENCER_API UMetaHumanSceneSequence : public UMovieSceneSequence
{
	GENERATED_BODY()

public:
	UMetaHumanSceneSequence(const FObjectInitializer& ObjectInitializer);

	//~ UMovieSceneSequence interface
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) override;
	virtual bool CanPossessObject(UObject& Object, UObject* InPlaybackContext) const override;
	virtual void LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;
	virtual void UnbindObjects(const FGuid& ObjectId, const TArray<UObject*>& InObjects, UObject* Context) override;
	virtual void UnbindInvalidObjects(const FGuid& ObjectId, UObject* Context) override;

#if WITH_EDITOR
	virtual FText GetDisplayName() const override;
	virtual ETrackSupport IsTrackSupportedImpl(TSubclassOf<class UMovieSceneTrack> InTrackClass) const override;
	void SetTickRate(class UFootageCaptureData* InFootageCaptureData);
#endif

public:
	UPROPERTY()
	TObjectPtr<UMovieScene> MovieScene;

#if WITH_EDITOR
	DECLARE_DELEGATE_FourParams(FGetExcludedFrameInfo, FFrameRate& OutSourceRate, FFrameRangeMap& OutExcludedFramesMap, int32& OutMediaStartFrame, TRange<FFrameNumber>& OutProcessingLimit);

	FGetExcludedFrameInfo GetExcludedFrameInfo;
#endif

private:
	UPROPERTY()
	TMap<FGuid, TObjectPtr<UObject>> Bindings;
};
