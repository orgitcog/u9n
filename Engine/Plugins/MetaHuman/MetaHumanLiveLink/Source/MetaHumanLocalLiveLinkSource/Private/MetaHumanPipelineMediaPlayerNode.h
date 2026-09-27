// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"
#include "MetaHumanLocalLiveLinkSubject.h"

#include "IMediaTextureSample.h"

namespace UE::MetaHuman::Pipeline
{

class FVideoSample
{
public:
	TArray<uint8> Data;
	FQualifiedFrameTime SampleTime;
	FMetaHumanLocalLiveLinkSubject::ETimeSource SampleTimeSource = FMetaHumanLocalLiveLinkSubject::ETimeSource::NotSet;
};

class FMediaPlayerNode : public FNode
{
public:

	FMediaPlayerNode(const FString& InTypeName, const FString& InName);

	virtual bool Play(const FString& InVideoURL, int32 InVideoTrack = -1, int32 InVideoTrackFormat = -1,
					  const FString& InAudioURL = "", int32 InAudioTrack = -1, int32 InAudioTrackFormat = -1) = 0;// Must be called from the game thread
	virtual bool Close() = 0; // Must be called from the game thread

	static FString BundleURL;

	double StartTimeout = 5;
	double FormatWaitTime = 0.1;
	double SampleTimeout = 5;

protected:

	void ConvertSample(const FIntPoint& InRes, const int32 InStride, const EMediaTextureSampleFormat InFormat, const uint8* InVideoSampleData, FUEImageDataType& OutImage) const;

	bool bIsFirstFrame = false;
	bool bAllowFrameDropping = true;

	const double StartWaitTime = 0.1;
	const double SampleWaitTime = 0.001;

	FFrameRate FrameRate;

	template <class T> void SafeRelease(T** InPointer)
	{
		if (*InPointer)
		{
			(*InPointer)->Release();
			*InPointer = nullptr;
		}
	}
};

}