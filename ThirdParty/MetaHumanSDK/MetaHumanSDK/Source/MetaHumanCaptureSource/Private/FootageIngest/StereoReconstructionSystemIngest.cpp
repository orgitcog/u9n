// Copyright Epic Games, Inc.All Rights Reserved.

#include "StereoReconstructionSystemIngest.h"

#define LOCTEXT_NAMESPACE "FootageIngest"

FStereoReconstructionSystemIngest::FStereoReconstructionSystemIngest(const FString& InInputDirectory, 
									 bool bInShouldCompressDepthFiles, 
									 bool bInCopyImagesToProject,
									 TRange<float> InDepthDistance,
									 EMetaHumanCaptureDepthPrecisionType InDepthPrecision,
									 EMetaHumanCaptureDepthResolutionType InDepthResolution)
	: FCubicCameraSystemIngest(InInputDirectory, bInShouldCompressDepthFiles, bInCopyImagesToProject, InDepthPrecision, InDepthResolution),
	DepthDistance(MoveTemp(InDepthDistance))
{
}

FStereoReconstructionSystemIngest::~FStereoReconstructionSystemIngest() = default;

#undef LOCTEXT_NAMESPACE