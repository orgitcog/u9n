// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Nodes/ConvertAudioNode.h"

#include "CaptureThirdPartyNodeParams.h"

#include "CaptureDataConverterNodeParams.h"

class FCaptureConvertAudioDataThirdParty final :
	public FConvertAudioNode
{
public:

	FCaptureConvertAudioDataThirdParty(FCaptureThirdPartyNodeParams InThirdPartEncoder,
									   const FTakeMetadata::FAudio& InAudio,
									   const FString& InOutputDirectory,
									   const FCaptureConvertDataNodeParams& InParams,
									   const FCaptureConvertAudioOutputParams& InAudioParams);

private:

	virtual FResult Run() override;

	FResult CopyAudioFile();
	FResult ConvertAudioFile();

	FCaptureThirdPartyNodeParams ThirdPartyEncoder;
	FCaptureConvertDataNodeParams Params;
	FCaptureConvertAudioOutputParams AudioParams;
};