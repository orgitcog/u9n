// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureManagerPipelineNode.h"

#include "CaptureDataConverterNodeParams.h"

class CAPTUREDATACONVERTER_API FCaptureConvertParams
{
public:

	FCaptureConvertParams();
	virtual ~FCaptureConvertParams();

	void SetParams(const FCaptureConvertDataNodeParams& InParams);

protected:

	FCaptureConvertDataNodeParams Params;
	
};