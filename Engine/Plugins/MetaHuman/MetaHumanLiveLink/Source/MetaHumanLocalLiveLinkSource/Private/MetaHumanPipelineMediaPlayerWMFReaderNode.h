// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanPipelineMediaPlayerNode.h"

namespace UE::MetaHuman::Pipeline
{

class FMediaPlayerWMFReaderNode : public FMediaPlayerNode
{
public:

	FMediaPlayerWMFReaderNode(const FString& InName);

	virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	virtual bool Play(const FString& InVideoURL, int32 InVideoTrack = -1, int32 InVideoTrackFormat = -1,
					  const FString& InAudioURL = "", int32 InAudioTrack = -1, int32 InAudioTrackFormat = -1) override; // Must be called from the game thread
	virtual bool Close() override; // Must be called from the game thread

	enum ErrorCode
	{
		VideoTimeout = 0,
		NoVideoPlayer,
		FailedToGetVideoSample,
		FailedToGetVideoSampleBuffer,
		FailedToGetVideoSampleLength,
		FailedToGetVideoSampleData,
	};

private:

	TSharedPtr<class FMediaPlayerWMFReaderNodeImpl> Impl;
	uint32 Width = 0;
	uint32 Height = 0;
	EMediaTextureSampleFormat Format = EMediaTextureSampleFormat::Undefined;
};

}