// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/TVariant.h"
#include "FrameTrackingContourData.h"
#include "FrameTrackingConfidenceData.h"
#include "FrameAnimationData.h"
#include "DepthMapDiagnosticsResult.h"
#include "Misc/QualifiedFrameTime.h"


namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINECORE_API FInvalidDataType
{
};

enum class EPipelineExitStatus
{
	Unknown = 0,
	OutOfScope,
	AlreadyRunning,
	NotInGameThread,
	InvalidNodeTypeName,
	InvalidNodeName,
	DuplicateNodeName,
	InvalidPinName,
	DuplicatePinName,
	InvalidConnection,
	AmbiguousConnection,
	Unconnected,
	LoopConnection,
	Ok,
	Aborted,
	StartError,
	ProcessError,
	EndError,
	TooFast,
	InsufficientThreadsForNodes
};

class METAHUMANPIPELINECORE_API FUEImageDataType
{
public:

	int32 Width = -1;
	int32 Height = -1;
	TArray<uint8> Data; // bgra order, a=255 for fully opaque
};

class METAHUMANPIPELINECORE_API FUEGrayImageDataType
{
public:

	int32 Width = -1;
	int32 Height = -1;
	TArray<uint8> Data;
};

class METAHUMANPIPELINECORE_API FHSImageDataType
{
public:

	int32 Width = -1;
	int32 Height = -1;
	TArray<float> Data;
};

class METAHUMANPIPELINECORE_API FScalingDataType
{
public:

	float Factor = -1.0f;
};

class METAHUMANPIPELINECORE_API FDepthDataType
{
public:

	int32 Width = -1;
	int32 Height = -1;
	TArray<float> Data;
};

class METAHUMANPIPELINECORE_API FFlowOutputDataType
{
public:

	TArray<float> Flow;
	TArray<float> Confidence;
	TArray<float> SourceCamera;
	TArray<float> TargetCamera;
};

class METAHUMANPIPELINECORE_API FAudioDataType
{
public:

	int32 NumChannels = -1;
	int32 SampleRate = -1;
	int32 NumSamples = -1;
	TArray<float> Data;
};

using FDataTreeType = TVariant<FInvalidDataType, EPipelineExitStatus, int32, float, double, bool, FString, FUEImageDataType, FUEGrayImageDataType, FHSImageDataType, FScalingDataType, FFrameTrackingContourData, FFrameAnimationData, FDepthDataType, FFrameTrackingConfidenceData, TArray<int32>, TArray<FFrameAnimationData>, FFlowOutputDataType, TMap<FString, FDepthMapDiagnosticsResult>, FAudioDataType, FQualifiedFrameTime>;

}
