// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Nodes/ConvertVideoNode.h"

#include "CaptureThirdPartyNodeParams.h"

#include "CaptureDataConverterNodeParams.h"

class FCaptureConvertVideoDataThirdParty final :
	public FConvertVideoNode
{
public:

	FCaptureConvertVideoDataThirdParty(FCaptureThirdPartyNodeParams InThirdPartyEncoder,
									   const FTakeMetadata::FVideo& InVideo,
									   const FString& InOutputDirectory,
									   const FCaptureConvertDataNodeParams& InParams,
									   const FCaptureConvertVideoOutputParams& InVideoParams);

private:

	virtual FResult Run() override;

	FResult ConvertData();
	FResult CopyData();

	FCaptureThirdPartyNodeParams ThirdPartyEncoder;
	FCaptureConvertDataNodeParams Params;
	FCaptureConvertVideoOutputParams VideoParams;
};