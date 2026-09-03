// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureManagerPipelineNode.h"
#include "Nodes/CaptureConvertParams.h"

class CAPTUREDATACONVERTER_API FCaptureConvertCustomData :
	public FCaptureManagerPipelineNode,
	public FCaptureConvertParams 
{
public:

	using FCaptureManagerPipelineNode::FCaptureManagerPipelineNode;
};