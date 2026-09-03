// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMediaAudioSample.h"
#include "IMediaTextureSample.h"

class IMediaAudioSample;
class IMediaTextureSample;

class IMetaHumanMediaAudioSourceReader
{
public:
	virtual ~IMetaHumanMediaAudioSourceReader() = default;

	virtual bool Open(const FString& URL) = 0;
	virtual FTimespan GetTotalDuration() const = 0;
	virtual IMediaAudioSample* Next() = 0;	// The sample is valid till the next call to Next() or Close()
	virtual EMediaAudioSampleFormat GetFormat() const = 0;
	virtual uint32 GetSampleRate() const = 0;
	virtual uint32 GetChannels() const = 0;
	virtual void Close() = 0;
	static TSharedPtr<IMetaHumanMediaAudioSourceReader, ESPMode::ThreadSafe> Create();
};

class IMetaHumanMediaVideoSourceReader
{
public:
	virtual ~IMetaHumanMediaVideoSourceReader() = default;

	virtual bool Open(const FString& URL) = 0;
	virtual FTimespan GetTotalDuration() const = 0;
	virtual IMediaTextureSample* Next() = 0;  // The sample is valid till the next call to Next() or Close()
	virtual FIntPoint GetDim() const = 0;
	virtual EMediaTextureSampleFormat GetFormat() const = 0;
	virtual void SetDefaultOrientation(EMediaOrientation InOrientation) = 0;  // Used if the orientation couldn't be determined from the video
	virtual void Close() = 0;
	static TSharedPtr<IMetaHumanMediaVideoSourceReader, ESPMode::ThreadSafe> Create();
};
