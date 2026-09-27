// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CubicCameraSystemIngest.h"



class FStereoReconstructionSystemIngest
	: public FCubicCameraSystemIngest
{
public:

	FStereoReconstructionSystemIngest(const FString& InInputDirectory, 
					  bool bInShouldCompressDepthFiles, 
					  bool bInCopyImagesToProject,
					  TRange<float> InDepthDistance,
					  EMetaHumanCaptureDepthPrecisionType InDepthPrecision,
					  EMetaHumanCaptureDepthResolutionType InDepthResolution);

	virtual ~FStereoReconstructionSystemIngest();


protected:
	TRange<float> DepthDistance;

};
