// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureManagerPipelineNode.h"

#include "CaptureManagerTakeMetadata.h"

class CAPTUREMANAGERPIPELINE_API FConvertVideoNode : public FCaptureManagerPipelineNode
{
public:

	FConvertVideoNode(const FTakeMetadata::FVideo& InVideo, 
					  const FString& InOutputDirectory);

	virtual ~FConvertVideoNode() override;

protected:

	FTakeMetadata::FVideo Video;
	FString OutputDirectory;

private:

	virtual FResult Prepare() override final;
	virtual FResult Validate() override final;

	FString GetVideoDirectory() const;

	static FResult CheckImagesForVideo(const FString& InVideoPath);
};