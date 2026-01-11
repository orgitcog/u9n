// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/DisplayClusterMediaInputCameraFull.h"

#include "DisplayClusterMediaHelpers.h"


FDisplayClusterMediaInputCameraFull::FDisplayClusterMediaInputCameraFull(
	const FString& InMediaId,
	const FString& InClusterNodeId,
	const FString& InCameraId,
	UMediaSource* InMediaSource
)
	: FDisplayClusterMediaInputCameraBase(
		InMediaId,
		InClusterNodeId,
		InCameraId,
		DisplayClusterMediaHelpers::GenerateICVFXViewportName(InClusterNodeId, InCameraId),
		InMediaSource)
{
}
