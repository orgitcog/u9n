// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureManagerPipelineNode.h"

#include "CaptureManagerTakeMetadata.h"

class CAPTUREMANAGERPIPELINE_API FConvertAudioNode : public FCaptureManagerPipelineNode
{
public:

	FConvertAudioNode(const FTakeMetadata::FAudio& InAudio,
					  const FString& InOutputDirectory);

	virtual ~FConvertAudioNode() override;

protected:

	FTakeMetadata::FAudio Audio;
	FString OutputDirectory;

private:

	virtual FResult Prepare() override final;
	virtual FResult Validate() override final;

	FString GetAudioDirectory() const;

	static FResult CheckForAudioFile(const FString& InAudioPath);
};