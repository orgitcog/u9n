// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StereoReconstructionSystemIngest.h"



class FHMCArchiveIngest
	: public FStereoReconstructionSystemIngest
{
public:

	FHMCArchiveIngest(const FString& InInputDirectory, 
					  bool bInShouldCompressDepthFiles, 
					  bool bInCopyImagesToProject,
					  TRange<float> InDepthDistance,
					  EMetaHumanCaptureDepthPrecisionType InDepthPrecision,
					  EMetaHumanCaptureDepthResolutionType InDepthResolution);

	virtual ~FHMCArchiveIngest();


private:
	static constexpr int32 DepthSaveNodeCount = 4;
	static constexpr int32 MaxStandardHMCImageSize = 3145728; // Technoprops resolution = 1536 * 2048

protected:
	//~ FCubicCameraSystemIngest interface
	virtual TResult<void, FMetaHumanCaptureError> IngestFiles(const FStopToken& InStopToken,
													  const FMetaHumanTakeInfo& InTakeInfo,
													  const FCubicTakeInfo& InCubicTakeInfo,
													  const FCameraContextMap& InCameraContextMap,
													  const TMap<FString, FCubicCameraInfo>& InTakeCameras,
		                                              FCameraCalibration& OutDepthCameraCalibration) override;

};
