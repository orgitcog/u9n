// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaStreamActor.h"

#include "MediaStreamComponent.h"

AMediaStreamActor::AMediaStreamActor()
{
	MediaStreamComponent = CreateDefaultSubobject<UMediaStreamComponent>(TEXT("MediaStreamComponent"));
	SetRootComponent(MediaStreamComponent);
}
