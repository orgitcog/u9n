// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/AudioUtilNodes.h"

#include "Misc/FileHelper.h"
#include "Audio.h"



namespace UE::MetaHuman::Pipeline
{

// SoundWave PCM data is always 16 bit

FAudioLoadNode::FAudioLoadNode(const FString& InName) : FNode("AudioLoad", InName)
{
	Pins.Add(FPin("Audio Out", EPinDirection::Output, EPinType::Audio));
}

bool FAudioLoadNode::Load(const USoundWave* InSoundWave)
{
	PcmData.Reset();
	SampleRate = 0;
	NumChannels = 0;

	bool bIsOk = false;
#if WITH_EDITOR
	bIsOk = InSoundWave && InSoundWave->GetImportedSoundWaveData(PcmData, SampleRate, NumChannels);
#endif

	return bIsOk;
}

bool FAudioLoadNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	if (PcmData.IsEmpty())
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::NoAudio);
		InPipelineData->SetErrorNodeMessage(TEXT("No audio data"));
		return false;
	}

	const int32 FrameOffsetInSamples = FrameOffset / FrameRate * SampleRate;
	PcmIndex = FrameOffsetInSamples * NumChannels * 2;

	StartFrame = -1;

	return true;
}

bool FAudioLoadNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const int32 CurrentFrame = InPipelineData->GetFrameNumber();

	if (StartFrame == -1)
	{
		StartFrame = CurrentFrame;
	}

	const int32 EndOfFrameInSamples = (CurrentFrame - StartFrame + FrameOffset + 1) / FrameRate * SampleRate;
	const int32 EndOfFrameInBytes = FMath::Min(EndOfFrameInSamples * NumChannels * 2, PcmData.Num());
	const int32 BytesForThisFrame = EndOfFrameInBytes - PcmIndex;

	if (BytesForThisFrame > 0)
	{
		FAudioDataType Output;
		Output.NumChannels = NumChannels;
		Output.SampleRate = SampleRate;
		Output.NumSamples = BytesForThisFrame / (Output.NumChannels * 2);
		Output.Data.SetNumUninitialized(Output.NumSamples * Output.NumChannels);

		const int16* InputData = (const int16*) &PcmData[PcmIndex];
		const float InputDataMaxValue = std::numeric_limits<int16>::max();
		float* OutputData = Output.Data.GetData();
		const int32 NumElements = Output.NumSamples * Output.NumChannels;
		for (int32 Index = 0; Index < NumElements; ++Index, ++InputData, ++OutputData)
		{
			*OutputData = *InputData / InputDataMaxValue;
		}

		PcmIndex += BytesForThisFrame;

		InPipelineData->SetData<FAudioDataType>(Pins[0], MoveTemp(Output));

		return true;
	}
	else
	{
		return false;
	}
}

bool FAudioLoadNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	PcmData.Reset();
	SampleRate = 0;
	NumChannels = 0;

	return true;
}



FAudioSaveNode::FAudioSaveNode(const FString& InName) : FNode("AudioSave", InName)
{
	Pins.Add(FPin("Audio In", EPinDirection::Input, EPinType::Audio));
}

bool FAudioSaveNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const FAudioDataType& Input = InPipelineData->GetData<FAudioDataType>(Pins[0]);

	NumChannels = Input.NumChannels;
	SampleRate = Input.SampleRate;

	const float* InputData = (const float*) Input.Data.GetData();
	const int16 PcmDataMaxValue = std::numeric_limits<int16>::max();
	const int32 NumElements = Input.NumSamples * Input.NumChannels;
	for (int32 Index = 0; Index < NumElements; ++Index, ++InputData)
	{
		const int16 Value = *InputData * PcmDataMaxValue;
		const uint8* PcmValue = (const uint8*) &Value;

		PcmData.Add(PcmValue[0]);
		PcmData.Add(PcmValue[1]);
	}

	return true;
}

bool FAudioSaveNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	if (!PcmData.IsEmpty())
	{
		TArray<uint8> WavFileData;
		SerializeWaveFile(WavFileData, PcmData.GetData(), PcmData.Num(), NumChannels, SampleRate);

		if (!FFileHelper::SaveArrayToFile(WavFileData, *FilePath))
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToSave);
			InPipelineData->SetErrorNodeMessage(TEXT("Failed to save"));
			return false;
		}
	}

	PcmData.Reset();

	return true;
}



FAudioConvertNode::FAudioConvertNode(const FString& InName) : FNode("AudioResample", InName)
{
	Pins.Add(FPin("Audio In", EPinDirection::Input, EPinType::Audio));
	Pins.Add(FPin("Audio Out", EPinDirection::Output, EPinType::Audio));
}

bool FAudioConvertNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	bResamplerInitialized = false;

	return true;
}

bool FAudioConvertNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	FAudioDataType Output = InPipelineData->GetData<FAudioDataType>(Pins[0]);

	if (Output.NumChannels != NumChannels)
	{
		if (Output.NumChannels == 2 && NumChannels == 1)
		{
			TArray<float> Mono;
			Mono.SetNumUninitialized(Output.NumSamples);

			const float* StereoData = Output.Data.GetData();
			float* MonoData = Mono.GetData();

			for (int32 Index = 0; Index < Output.NumSamples; ++Index, StereoData += 2, ++MonoData)
			{
				*MonoData = (StereoData[0] + StereoData[1]) / 2;
			}

			Output.NumChannels = 1;
			Output.Data = Mono;
		}
		else
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::UnsupportedChannelMix);
			InPipelineData->SetErrorNodeMessage(FString::Printf(TEXT("Unsupported channel mix - have %i want %i"), Output.NumChannels, NumChannels));
			return false;
		}
	}

	if (Output.SampleRate != SampleRate)
	{
		if (!bResamplerInitialized)
		{
			Resampler.Init(Audio::EResamplingMethod::ZeroOrderHold, float(SampleRate) / Output.SampleRate, 1);
			bResamplerInitialized = true;
		}

		TArray<float> ResampledBuffer;
		const int32 ResampledSamplesCount = (float) Output.NumSamples / (float) Output.SampleRate * SampleRate;
		ResampledBuffer.SetNumZeroed(ResampledSamplesCount);

		int32 ResampledNumFrames = 0;
		if (Resampler.ProcessAudio(Output.Data.GetData(), Output.NumSamples, true, ResampledBuffer.GetData(), ResampledSamplesCount, ResampledNumFrames) == 0)
		{
			Output.SampleRate = SampleRate;
			Output.NumSamples = ResampledSamplesCount;
			Output.Data = ResampledBuffer;
		}
		else
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToResample);
			InPipelineData->SetErrorNodeMessage(TEXT("Resampling failed"));
			return false;
		}
	}

	InPipelineData->SetData<FAudioDataType>(Pins[1], MoveTemp(Output));

	return true;
}

}