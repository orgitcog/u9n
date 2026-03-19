// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/DisplayClusterMediaInputViewportTile.h"

#include "DisplayClusterMediaHelpers.h"


FDisplayClusterMediaInputViewportTile::FDisplayClusterMediaInputViewportTile(
	const FString& InMediaId,
	const FString& InClusterNodeId,
	const FString& InViewportId,
	const FIntPoint& InTilePosition,
	UMediaSource* InMediaSource
)
	: FDisplayClusterMediaInputViewportBase(
		InMediaId,
		InClusterNodeId,
		DisplayClusterMediaHelpers::GenerateTileViewportName(InViewportId, InTilePosition),
		InMediaSource)
{
}
