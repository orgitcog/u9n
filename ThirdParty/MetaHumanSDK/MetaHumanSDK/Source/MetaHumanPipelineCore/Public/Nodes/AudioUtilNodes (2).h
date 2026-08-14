// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"
#include "AudioResampler.h"
#include "Sound/SoundWave.h"

namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINECORE_API FAudioLoadNode : public FNode
{
public:

	FAudioLoadNode(const FString& InName);

	bool Load(const USoundWave* InSoundWave);

	virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool End(const TSharedPtr<FPipelineData>& InPipelineData) override;

	float FrameRate = 30;
	int32 FrameOffset = 0;

	enum ErrorCode
	{
		NoAudio = 0,
	};

private:

	int32 PcmIndex = 0;
	int32 StartFrame = -1;

	TArray<uint8> PcmData;
	uint16 NumChannels = 0;
	uint32 SampleRate = 0;
};

class METAHUMANPIPELINECORE_API FAudioSaveNode : public FNode
{
public:

	FAudioSaveNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool End(const TSharedPtr<FPipelineData>& InPipelineData) override;

	FString FilePath;

	enum ErrorCode
	{
		FailedToSave = 0,
	};

private:

	int32 NumChannels = 0;
	int32 SampleRate = 0;
	TArray<uint8> PcmData;
};

class METAHUMANPIPELINECORE_API FAudioConvertNode : public FNode
{
public:

	FAudioConvertNode(const FString& InName);

	virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	int32 NumChannels = 0;
	int32 SampleRate = 0;

	enum ErrorCode
	{
		UnsupportedChannelMix = 0,
		FailedToResample,
	};

private:

	Audio::FResampler Resampler;
	bool bResamplerInitialized = false;
};

}