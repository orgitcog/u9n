// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/StaticMeshComponent.h"

#include "MediaStreamActor.generated.h"

class UMediaStreamComponent;

UCLASS(BlueprintType, MinimalAPI, ComponentWrapperClass, NotPlaceable,
	PrioritizeCategories = (MediaStream, MediaControls, MediaSource, MediaDetails, MediaTexture, MediaCache, MediaPlayer))
class AMediaStreamActor : public AActor
{
	GENERATED_BODY()

public:
	AMediaStreamActor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Media Stream")
	TObjectPtr<UMediaStreamComponent> MediaStreamComponent;
};
