// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IMetaHumanWaveFileWriter
{
public:
	virtual bool Open(const FString& WavFilename, int32 SampleRate, int32 NumChannels, int32 BitsPerSample) = 0;
	virtual bool Append(class IMediaAudioSample* Sample) = 0;
	virtual bool Close() = 0;

	virtual ~IMetaHumanWaveFileWriter() {}

	static TSharedPtr<IMetaHumanWaveFileWriter> Create();
};
