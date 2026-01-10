// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Nodes/ConvertAudioNode.h"

#include "CaptureDataConverterNodeParams.h"

#include "MediaSample.h"

class FCaptureConvertAudioData final : 
	public FConvertAudioNode
{
public:

	FCaptureConvertAudioData(const FTakeMetadata::FAudio& InAudio,
							 const FString& InOutputDirectory,
							 const FCaptureConvertDataNodeParams& InParams,
							 const FCaptureConvertAudioOutputParams& InAudioParams);

private:

	virtual FResult Run() override;

	FResult CopyAudioFile();
	FResult ConvertAudioFile();

	FCaptureConvertDataNodeParams Params;
	FCaptureConvertAudioOutputParams AudioParams;
};