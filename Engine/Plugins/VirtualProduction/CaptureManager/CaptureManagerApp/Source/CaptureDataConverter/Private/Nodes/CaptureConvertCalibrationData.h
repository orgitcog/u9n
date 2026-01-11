// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Nodes/ConvertCalibrationNode.h"

#include "CaptureDataConverterNodeParams.h"

#include "MediaSample.h"

class FCaptureConvertCalibrationData final :
	public FConvertCalibrationNode
{
public:

	FCaptureConvertCalibrationData(const FTakeMetadata::FCalibration& InCalibration,
								   const FString& InOutputDirectory,
								   const FCaptureConvertDataNodeParams& InParams,
								   const FCaptureConvertCalibrationOutputParams& InCalibrationParams);

private:

	virtual FResult Run() override;

	FResult CopyCalibrationFile();
	FResult ConvertCalibrationFile();

	FCaptureConvertDataNodeParams Params;
	FCaptureConvertCalibrationOutputParams CalibrationParams;
};