// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/DisplayClusterMediaInputViewportFull.h"


FDisplayClusterMediaInputViewportFull::FDisplayClusterMediaInputViewportFull(
	const FString& InMediaId,
	const FString& InClusterNodeId,
	const FString& InViewportId,
	UMediaSource* InMediaSource
)
	: FDisplayClusterMediaInputViewportBase(InMediaId, InClusterNodeId, InViewportId, InMediaSource)
{
}
