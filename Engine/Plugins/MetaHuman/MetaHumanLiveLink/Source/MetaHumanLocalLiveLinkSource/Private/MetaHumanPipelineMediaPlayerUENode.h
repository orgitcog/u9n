// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanPipelineMediaPlayerNode.h"

#include "MediaPlayer.h"
#include "MediaSampleQueue.h"

class UMediaBundle;

namespace UE::MetaHuman::Pipeline
{

class FMediaPlayerUENode : public FMediaPlayerNode, public FGCObject
{
public:

	FMediaPlayerUENode(const FString& InName);

	virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool End(const TSharedPtr<FPipelineData>& InPipelineData) override;

	virtual bool Play(const FString& InVideoURL, int32 InVideoTrack = -1, int32 InVideoTrackFormat = -1,
					  const FString& InAudioURL = "", int32 InAudioTrack = -1, int32 InAudioTrackFormat = -1) override; // Must be called from the game thread
	virtual bool Close() override; // Must be called from the game thread

	enum ErrorCode
	{
		VideoTimeout = 0,
		AudioTimeout,
		NoVideoPlayer,
		NoAudioPlayer,
		BadVideoTrack,
		BadVideoTrackFormat,
		BadAudioTrack,
		BadAudioTrackFormat,
		UnsupportedVideoFormat,
		FailedToPlayVideo,
		FailedToPlayAudio,
		NoVideoSampleData,
	};

	//~ Begin FGCObject interface

	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	virtual FString GetReferencerName() const;

	//~ End FGCObject interface

private:

	FString VideoURL;
	int32 VideoTrack = -1;
	int32 VideoTrackFormat = -1;

	FString AudioURL;
	int32 AudioTrack = -1;
	int32 AudioTrackFormat = -1;

	bool bIsBundle = false;

	TObjectPtr<UMediaPlayer> VideoPlayer = nullptr;
	TObjectPtr<UMediaPlayer> AudioPlayer = nullptr;

	TObjectPtr<UMediaBundle> Bundle = nullptr;

	TSharedPtr<FMediaTextureSampleQueue> VideoFrames = nullptr;
	TSharedPtr<FMediaAudioSampleQueue> AudioFrames = nullptr;

	TArray<float> AudioBuffer;
	int32 AudioBufferSampleRate = -1;
	int32 AudioBufferNumChannels = -1;
	int32 AudioBufferDataSizePerFrame = -1;
};

}