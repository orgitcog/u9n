// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureManagerPipelineNode.h"

#include "CaptureManagerTakeMetadata.h"

class CAPTUREMANAGERPIPELINE_API FConvertCalibrationNode : public FCaptureManagerPipelineNode
{
public:

	FConvertCalibrationNode(const FTakeMetadata::FCalibration& InCalibration,
							const FString& InOutputDirectory);
protected:

	FTakeMetadata::FCalibration Calibration;
	FString OutputDirectory;

private:

	virtual FResult Prepare() override final;
	virtual FResult Validate() override final;

	FString GetCalibrationDirectory() const;
};