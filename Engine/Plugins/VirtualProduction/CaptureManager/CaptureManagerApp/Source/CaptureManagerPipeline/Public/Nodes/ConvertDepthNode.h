// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureManagerPipelineNode.h"

#include "CaptureManagerTakeMetadata.h"

class CAPTUREMANAGERPIPELINE_API FConvertDepthNode : public FCaptureManagerPipelineNode
{
public:

	FConvertDepthNode(const FTakeMetadata::FVideo& InDepth,
					  const FString& InOutputDirectory);

	virtual ~FConvertDepthNode() override;

protected:

	FTakeMetadata::FVideo Depth;
	FString OutputDirectory;

private:

	virtual FResult Prepare() override final;
	virtual FResult Validate() override final;

	FString GetDepthDirectory() const;

	static FResult CheckImagesForDepth(const FString& InDepthPath);
};