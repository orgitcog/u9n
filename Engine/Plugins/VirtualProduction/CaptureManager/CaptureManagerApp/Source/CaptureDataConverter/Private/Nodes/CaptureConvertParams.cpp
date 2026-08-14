// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/CaptureConvertParams.h"

FCaptureConvertParams::FCaptureConvertParams() = default;
FCaptureConvertParams::~FCaptureConvertParams() = default;

void FCaptureConvertParams::SetParams(const FCaptureConvertDataNodeParams& InParams)
{
	Params = InParams;
}
