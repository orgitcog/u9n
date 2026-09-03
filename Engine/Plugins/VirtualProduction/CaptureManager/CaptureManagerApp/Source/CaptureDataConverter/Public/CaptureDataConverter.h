// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureManagerPipeline.h"
#include "CaptureDataConverterError.h"

#include "CaptureManagerTakeMetadata.h"

#include "CaptureDataConverterNodeParams.h"

#include "Nodes/CaptureConvertCustomData.h"

struct FCaptureDataConverterParams
{
	FTakeMetadata TakeMetadata;

	FString TakeName;
	FString TakeOriginDirectory;
	FString TakeOutputDirectory;

	TOptional<FCaptureConvertVideoOutputParams> VideoOutputParams;
	TOptional<FCaptureConvertAudioOutputParams> AudioOutputParams;
	TOptional<FCaptureConvertDepthOutputParams> DepthOutputParams;
	TOptional<FCaptureConvertCalibrationOutputParams> CalibrationOutputParams;
};

template<typename T>
using FCaptureDataConverterResult = TValueOrError<T, FCaptureDataConverterError>;

class CAPTUREDATACONVERTER_API FCaptureDataConverter
{
public:

	DECLARE_DELEGATE_OneParam(FProgressReporter, double InProgress);

	FCaptureDataConverter();
	~FCaptureDataConverter();

	void AddCustomNode(TSharedPtr<FCaptureConvertCustomData> InCustomNode);
	void AddSyncNode(TSharedPtr<FCaptureConvertCustomData> InCustomNode);

	[[nodiscard]] FCaptureDataConverterResult<void> Run(FCaptureDataConverterParams InParams, FProgressReporter InProgressReporter);
	void Cancel();

private:

	TArray<TSharedPtr<FCaptureConvertCustomData>> CustomNodes;
	TArray<TSharedPtr<FCaptureConvertCustomData>> SyncNodes;

	TSharedPtr<FCaptureManagerPipeline> Pipeline;
	UE::CaptureManager::FStopRequester StopRequester;
};
