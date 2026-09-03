// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#include "Nodes/ConvertAudioNode.h"
#include "Nodes/ConvertDepthNode.h"
#include "Nodes/ConvertVideoNode.h"
#include "Nodes/ConvertCalibrationNode.h"

#include "Templates/SharedPointer.h"
#include "Misc/Guid.h"

#include "Async/Monitor.h"

enum class EPipelineExecutionPolicy
{
	Asynchronous = 0,
	Synchronous
};

class CAPTUREMANAGERPIPELINE_API FCaptureManagerPipeline : public TSharedFromThis<FCaptureManagerPipeline>
{
public:
	using FResult = TMap<FGuid, FCaptureManagerPipelineNode::FResult>;

	FCaptureManagerPipeline(EPipelineExecutionPolicy InExecutionPolicy);
	~FCaptureManagerPipeline();

	FGuid AddGenericNode(TSharedPtr<FCaptureManagerPipelineNode> InNode);
	FGuid AddConvertVideoNode(TSharedPtr<FConvertVideoNode> InNode);
	FGuid AddConvertAudioNode(TSharedPtr<FConvertAudioNode> InNode);
	FGuid AddConvertDepthNode(TSharedPtr<FConvertDepthNode> InNode);
	FGuid AddConvertCalibrationNode(TSharedPtr<FConvertCalibrationNode> InNode);

	FGuid AddSyncedNode(TSharedPtr<FCaptureManagerPipelineNode> InNode);

	// Blocking function
	[[nodiscard]] FCaptureManagerPipeline::FResult Run();

	void Cancel();

private:

	FGuid AddParallelPipelineNode(TSharedPtr<FCaptureManagerPipelineNode> InNode);

	TPimplPtr<class FCaptureManagerPipelineImpl> Impl;

	
	using FNodeMap = TMap<FGuid, TSharedPtr<FCaptureManagerPipelineNode>>;

	UE::CaptureManager::FMonitor<FNodeMap> ParallelNodes;
	UE::CaptureManager::FMonitor<FNodeMap> SyncNodes;

	EPipelineExecutionPolicy ExecutionPolicy;
};