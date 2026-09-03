// Copyright Epic Games, Inc. All Rights Reserved.

#include "Speech2FaceInternal.h"
#include "MetaHumanAuthoringObjects.h"
#include "UObject/Package.h"
#include "DataDefs.h"
#include "Sound/SoundWave.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathUtility.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "SampleBuffer.h"

DEFINE_LOG_CATEGORY(LogSpeech2FaceSolver)

#if WITH_EDITOR

FSpeech2FaceInternal::FSpeech2FaceInternal() = default;

TUniquePtr<FSpeech2FaceInternal> FSpeech2FaceInternal::Create(const FAudioDrivenAnimationModels& InModels)
{
	TUniquePtr<FSpeech2FaceInternal> Result = TUniquePtr<FSpeech2FaceInternal>(new FSpeech2FaceInternal());

	if (!Result->Init(InModels))
	{
		return nullptr;
	}
	return Result;
}

bool FSpeech2FaceInternal::Init(const FAudioDrivenAnimationModels& InModels)
{
	check(IsInGameThread());

	AudioExtractor = TryLoadModelData(InModels.AudioEncoder);
	RigLogicPredictor = TryLoadModelData(InModels.AnimationDecoder);

	return AudioExtractor && RigLogicPredictor;
}

TSharedPtr<UE::NNE::IModelInstanceCPU> FSpeech2FaceInternal::TryLoadModelData(const FSoftObjectPath& InModelAssetPath)
{
	const FSoftObjectPtr ModelAsset(InModelAssetPath);
	UNNEModelData* ModelData = Cast<UNNEModelData>(ModelAsset.LoadSynchronous());

	if (!IsValid(ModelData))
	{
		check(false);
		UE_LOG(LogSpeech2FaceSolver, Error, TEXT("Failed to load model, it is invalid (nullptr)"));
		return nullptr;
	}

	if (!FModuleManager::Get().LoadModule(TEXT("NNERuntimeORT")))
	{
		UE_LOG(LogSpeech2FaceSolver, Error, TEXT("Failed to load model, could not load NNE Runtime module (NNERuntimeORT): %s"), *ModelData->GetPathName());
		return nullptr;
	}

	const TWeakInterfacePtr<INNERuntimeCPU> NNERuntimeCPU = UE::NNE::GetRuntime<INNERuntimeCPU>(TEXT("NNERuntimeORTCpu"));

	if (!NNERuntimeCPU.IsValid())
	{
		UE_LOG(LogSpeech2FaceSolver, Error, TEXT("Failed to load model, could not load NNE Runtime: %s"), *ModelData->GetPathName());
		return nullptr;
	}

	TSharedPtr<UE::NNE::IModelCPU> ModelCpu = NNERuntimeCPU->CreateModelCPU(ModelData);

	if (!ModelCpu.IsValid())
	{
		UE_LOG(LogSpeech2FaceSolver, Error, TEXT("Failed to load model, could not create model CPU: %s"), *ModelData->GetPathName());
		return nullptr;
	}

	TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance = ModelCpu->CreateModelInstanceCPU();

	if (ModelInstance.IsValid())
	{
		UE_LOG(LogSpeech2FaceSolver, Display, TEXT("Loaded model: %s"), *ModelData->GetPathName());
	}
	else
	{
		UE_LOG(LogSpeech2FaceSolver, Error, TEXT("Failed to load model, could not create model instance: %s"), *ModelData->GetPathName());
	}

	return ModelInstance;
}

bool FSpeech2FaceInternal::GenerateFaceAnimation(const FSpeech2Face::FAudioParams& InAudioParams,
	float InOutputAnimationFps,
	bool bInGenerateBlinks,
	TFunction<bool()> InShouldCancelCallback,
	TArray<FSpeech2Face::FAnimationFrame>& OutAnimation,
	TArray<FSpeech2Face::FAnimationFrame>& OutHeadAnimation) const
{
	using namespace UE::MetaHuman;

	check(InAudioParams.SpeechRecording.IsValid());
	check(InAudioParams.AudioStartOffsetSec >= 0);

	// If the user has not opted to downmix the audio, the audio channel index should be valid.
	check(InAudioParams.bDownmixChannels || (InAudioParams.AudioChannelIndex < InAudioParams.SpeechRecording->NumChannels && InAudioParams.AudioChannelIndex >= 0));
	check(InOutputAnimationFps > 0);

	TArray<uint8> PcmData;
	uint16 ChannelNum;
	uint32 SampleRate;
	if (!InAudioParams.SpeechRecording->GetImportedSoundWaveData(PcmData, SampleRate, ChannelNum))
	{
		UE_LOG(LogSpeech2FaceSolver, Error, TEXT("Could not get imported PCM data for SoundWave %s"), *InAudioParams.SpeechRecording->GetName());
		return false;
	}

	if (InShouldCancelCallback())
	{
		return false;
	}

	// Prepare audio
	UE_LOG(LogSpeech2FaceSolver, Log, TEXT("Preparing samples for solve"));
	FloatSamples Samples;
	if (!GetFloatSamples(InAudioParams.SpeechRecording, PcmData, SampleRate, InAudioParams.bDownmixChannels, InAudioParams.AudioChannelIndex, InAudioParams.AudioStartOffsetSec, Samples))
	{
		return false;
	}

	if (InShouldCancelCallback())
	{
		return false;
	}

	UE_LOG(LogSpeech2FaceSolver, Log, TEXT("Extracting audio features"));
	TArray<float> ExtractedAudioData;
	if (!ExtractAudioFeatures(Samples, AudioExtractor, ExtractedAudioData))
	{
		return false;
	}

	if (InShouldCancelCallback())
	{
		return false;
	}

	TArray<float> RigLogicValues;
	TArray<float> RigLogicBlinkValues;
	TArray<float> RigLogicHeadValues;

	UE_LOG(LogSpeech2FaceSolver, Log, TEXT("Running predictor"));

	if (!RunPredictor(RigControlNames.Num(), BlinkRigControlNames.Num(), Samples.Num(), ExtractedAudioData, RigLogicValues, RigLogicBlinkValues, RigLogicHeadValues))
	{
		return false;
	}

	if (InShouldCancelCallback())
	{
		return false;
	}

	TArray<FString> HeadControlNamesGui;
	HeadControlsGuiToRawLookupTable.GetKeys(HeadControlNamesGui);

	// Copy rig logic values to structured frame by frame rig control values
	if (InOutputAnimationFps == RigLogicPredictorOutputFps)
	{
		UE_LOG(LogSpeech2FaceSolver, Log, TEXT("Copying samples"));
		const int32 NumFrames = RigLogicValues.Num() / RigControlNames.Num();
		OutAnimation.Empty(NumFrames);
		OutHeadAnimation.Empty(NumFrames);

		for (int32 FrameIndex = 0; FrameIndex < NumFrames; FrameIndex++)
		{
			// Face animation
			OutAnimation.AddDefaulted();
			OutAnimation.Last().Reserve(RigControlNames.Num());

			for (int32 ControlIndex = 0; ControlIndex < RigControlNames.Num(); ControlIndex++)
			{
				OutAnimation.Last().Add(RigControlNames[ControlIndex], RigLogicValues[FrameIndex * RigControlNames.Num() + ControlIndex]);
			}

			if (bInGenerateBlinks)
			{
				for (int32 BlinkControlIndex = 0; BlinkControlIndex < BlinkRigControlNames.Num(); BlinkControlIndex++)
				{
					OutAnimation.Last()[BlinkRigControlNames[BlinkControlIndex]] = RigLogicBlinkValues[FrameIndex * BlinkRigControlNames.Num() + BlinkControlIndex];
				}
			}

			// Head animation
			FSpeech2Face::FAnimationFrame& HeadAnimationFrame = OutHeadAnimation.AddDefaulted_GetRef();

			for (const FString& HeadControlNameGui : HeadControlNamesGui)
			{
				const int32 ModelHeadControlIndex = ModelHeadControls.IndexOfByKey(HeadControlNameGui);

				if (ModelHeadControlIndex != INDEX_NONE)
				{
					const int32 HeadValueIndex = FrameIndex * ModelHeadControls.Num() + ModelHeadControlIndex;
					check(RigLogicHeadValues.IsValidIndex(HeadValueIndex));

					const float ModelHeadControlValue = RigLogicHeadValues[HeadValueIndex];
					HeadAnimationFrame.Emplace(HeadControlNameGui, ModelHeadControlValue);
				}
				else
				{
					// Not provided by the model so we clamp it to zero
					HeadAnimationFrame.Emplace(HeadControlNameGui, 0.0f);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogSpeech2FaceSolver, Log, TEXT("Resampling"));

		// Resample output animation
		OutAnimation = ResampleAnimation(RigLogicValues, RigControlNames, RigControlNames.Num(), InOutputAnimationFps);

		if (bInGenerateBlinks)
		{
			TArray<FSpeech2Face::FAnimationFrame> BlinkAnimation = ResampleAnimation(RigLogicBlinkValues, BlinkRigControlNames, BlinkRigControlNames.Num(), InOutputAnimationFps);
			for (int32 FrameIndex = 0; FrameIndex < BlinkAnimation.Num(); FrameIndex++)
			{
				for (const FString& BlinkControlName : BlinkRigControlNames)
				{
					OutAnimation[FrameIndex][BlinkControlName] += BlinkAnimation[FrameIndex][BlinkControlName];
				}
			}
		}

		// Head animation
		TArray<FSpeech2Face::FAnimationFrame> ResampledHeadAnimation = ResampleAnimation(RigLogicHeadValues, ModelHeadControls, ModelHeadControls.Num(), InOutputAnimationFps);

		for (const FSpeech2Face::FAnimationFrame& ResampledHeadAnimationFrame : ResampledHeadAnimation)
		{
			FSpeech2Face::FAnimationFrame& HeadAnimationFrame = OutHeadAnimation.AddDefaulted_GetRef();

			for (const FString& HeadControlNameGui : HeadControlNamesGui)
			{
				const float* HeadControlValue = ResampledHeadAnimationFrame.Find(HeadControlNameGui);

				if (HeadControlValue)
				{
					HeadAnimationFrame.Emplace(HeadControlNameGui, *HeadControlValue);
				}
				else
				{
					// Not provided by the model so we clamp it to zero
					HeadAnimationFrame.Emplace(HeadControlNameGui, 0.0f);
				}
			}
		}
	}

	// We should always have the same number of frames for the face and head animation
	check(OutHeadAnimation.Num() == OutAnimation.Num());

	UE_LOG(LogSpeech2FaceSolver, Log, TEXT("Sound Wave Processing Complete"));
	return true;
}

void FSpeech2FaceInternal::SetMood(const EAudioDrivenAnimationMood& InMood)
{
	DesiredMood = InMood;
}

void FSpeech2FaceInternal::SetMoodIntensity(const float InMoodIntensity)
{
	check(InMoodIntensity >= 0.0f);
	DesiredMoodIntensity = InMoodIntensity;
}

bool FSpeech2FaceInternal::ExtractAudioFeatures(const FloatSamples& Samples, const TSharedPtr<UE::NNE::IModelInstanceCPU>& AudioExtractor, TArray<float>& OutAudioData)
{
	using namespace UE::NNE;

	OutAudioData.Empty((Samples.Num() / SamplesPerFrame) * 512);

	// Restrict extracting of audio features to 30 second chunks as the model does not support more
	for (int32 SampleIndex = 0; SampleIndex < Samples.Num(); SampleIndex += RigLogicPredictorMaxAudioSamples)
	{
		const uint32 SamplesCount = FMath::Clamp(Samples.Num() - SampleIndex, 0, RigLogicPredictorMaxAudioSamples);

		TArray<uint32, TInlineAllocator<2>> ExtractorInputShapesData = { 1, SamplesCount };
		TArray<FTensorShape, TInlineAllocator<1>> ExtractorInputShapes = { FTensorShape::Make(ExtractorInputShapesData) };
		if (AudioExtractor->SetInputTensorShapes(ExtractorInputShapes) != IModelInstanceCPU::ESetInputTensorShapesStatus::Ok)
		{
			UE_LOG(LogSpeech2FaceSolver, Error, TEXT("Could not set the audio extractor input tensor shapes"));
			return false;
		}

		// Todo: last frame of the last chunk will not be complete (if not multiple of SamplesPerFrame). Should we ceil/pad/0-fill? 
		const uint32 NumFrames = static_cast<uint32>(SamplesCount / SamplesPerFrame);
		TArray<uint32, TInlineAllocator<3>> ExtractorOutputShapeData = { 1, NumFrames, 512 };
		FTensorShape ExtractorOutputShape = FTensorShape::Make(ExtractorOutputShapeData);
		TArray<float> ExtractorOutputData;
		ExtractorOutputData.SetNumUninitialized(ExtractorOutputShape.Volume());

		TArray<FTensorBindingCPU, TInlineAllocator<1>> ExtractorInputBindings = { {(void*)(Samples.GetData() + SampleIndex), SamplesCount * sizeof(float)} };
		TArray<FTensorBindingCPU, TInlineAllocator<1>> ExtractorOutputBindings = { {(void*)ExtractorOutputData.GetData(), ExtractorOutputData.Num() * sizeof(float)} };
		if (AudioExtractor->RunSync(ExtractorInputBindings, ExtractorOutputBindings) != IModelInstanceCPU::ESetInputTensorShapesStatus::Ok)
		{
			UE_LOG(LogSpeech2FaceSolver, Error, TEXT("The audio extractor NNE model failed to execute"));
			return false;
		}

		OutAudioData.Append(ExtractorOutputData.GetData(), ExtractorOutputData.Num());
	}
	return true;
}

bool FSpeech2FaceInternal::RunPredictor(
	const uint32 InFaceControlNum,
	const uint32 InBlinkControlNum,
	const uint32 InSamplesNum,
	const TArray<float>& InAudioData,
	TArray<float>& OutRigLogicValues,
	TArray<float>& OutRigLogicBlinkValues,
	TArray<float>& OutRigLogicHeadValues
) const
{
	using namespace UE::NNE;

	const uint32 NumFrames = static_cast<uint32>(InSamplesNum / SamplesPerFrame);
	TArray<uint32, TInlineAllocator<2>> AudioShapeData = { 1, NumFrames, 512 };

	const int32 MoodIndex = GetModelMoodIndex();
	const TArray<int32, TInlineAllocator<1>> MoodIndexArray = { MoodIndex, };
	TArray<uint32, TInlineAllocator<1>> MoodIndexShapeData = { 1, };

	const TArray<float, TInlineAllocator<1>> MoodIntensityArray = { DesiredMoodIntensity, };
	TArray<uint32, TInlineAllocator<1>> MoodIntensityShapeData = { 1, };

	TArray<FTensorShape, TInlineAllocator<3>> InputTensorShapes = {
		FTensorShape::Make(AudioShapeData),
		FTensorShape::Make(MoodIndexShapeData),
		FTensorShape::Make(MoodIntensityShapeData)
	};

	check(RigLogicPredictor);

	if (RigLogicPredictor->SetInputTensorShapes(InputTensorShapes) != IModelInstanceCPU::ESetInputTensorShapesStatus::Ok)
	{
		return false;
	}

	// Bind the inputs

	// Tensor binding requires non-const void* - we're trusting it not to mutate the input data.
	void* AudioDataPtr = const_cast<void*>(static_cast<const void*>(InAudioData.GetData()));
	void* MoodIndexDataPtr = const_cast<void*>(static_cast<const void*>(MoodIndexArray.GetData()));
	void* MoodIntensityDataPtr = const_cast<void*>(static_cast<const void*>(MoodIntensityArray.GetData()));

	TArray<FTensorBindingCPU, TInlineAllocator<3>> InputBindings = {
		{AudioDataPtr, InAudioData.Num() * sizeof(float)},
		{MoodIndexDataPtr, MoodIndexArray.Num() * sizeof(float)},
		{MoodIntensityDataPtr, MoodIntensityArray.Num() * sizeof(float)}
	};

	// Bind the outputs
	TArray<float> FaceParameters;
	TArray<uint32, TInlineAllocator<3>> FaceParametersShapeData = { 1, NumFrames,  InFaceControlNum };
	TArray<FTensorShape, TInlineAllocator<1>> FaceParametersShape = { FTensorShape::Make(FaceParametersShapeData) };
	FaceParameters.SetNumUninitialized(FaceParametersShape[0].Volume());

	TArray<float> BlinkParameters;
	TArray<uint32, TInlineAllocator<3>> BlinkParametersShapeData = { 1, NumFrames,  InBlinkControlNum };
	TArray<FTensorShape, TInlineAllocator<1>> BlinkParametersShape = { FTensorShape::Make(BlinkParametersShapeData) };
	BlinkParameters.SetNumUninitialized(BlinkParametersShape[0].Volume());

	const uint32 NumOutputHeadControls = static_cast<uint32>(ModelHeadControls.Num());

	TArray<float> HeadParameters;
	TArray<uint32, TInlineAllocator<3>> HeadParametersShapeData = { 1, NumFrames,  NumOutputHeadControls };
	TArray<FTensorShape, TInlineAllocator<1>> HeadParametersShape = { FTensorShape::Make(HeadParametersShapeData) };
	HeadParameters.SetNumUninitialized(HeadParametersShape[0].Volume());

	void* FaceParametersPtr = static_cast<void*>(FaceParameters.GetData());
	void* BlinkParametersPtr = static_cast<void*>(BlinkParameters.GetData());
	void* HeadParametersPtr = static_cast<void*>(HeadParameters.GetData());

	TArray<FTensorBindingCPU, TInlineAllocator<2>> OutputBindings = {
		{FaceParametersPtr, FaceParameters.Num() * sizeof(float)},
		{BlinkParametersPtr, BlinkParameters.Num() * sizeof(float)},
		{HeadParametersPtr, HeadParameters.Num() * sizeof(float) }
	};

	if (RigLogicPredictor->RunSync(InputBindings, OutputBindings) != IModelInstanceCPU::ESetInputTensorShapesStatus::Ok)
	{
		UE_LOG(LogSpeech2FaceSolver, Error, TEXT("The rig logic model failed to execute"));
		return false;
	}

	OutRigLogicValues = MoveTemp(FaceParameters);
	OutRigLogicBlinkValues = MoveTemp(BlinkParameters);
	OutRigLogicHeadValues = MoveTemp(HeadParameters);

	return true;
}

int32 FSpeech2FaceInternal::GetModelMoodIndex() const
{
	if (DesiredMood == EAudioDrivenAnimationMood::AutoDetect)
	{
		// Special case for AutoDetect. The blueprintable UENUM requires it to be backed by a uint8, so we instead encode the
		// AutoDetect value as 255 in the UENUM and replace that value here with -1 (which is what the model expects)
		return -1;
	}

	return static_cast<uint8>(DesiredMood);
}

bool FSpeech2FaceInternal::GetFloatSamples(const TWeakObjectPtr<const USoundWave>& SoundWave, const TArray<uint8>& PcmData, uint32 SampleRate, bool bDownmixChannels, uint32 ChannelToUse, float SecondsToSkip, FloatSamples& OutSamples)
{
	int16 Sample;
	const uint32 TotalSampleCount = PcmData.Num() / sizeof(Sample);
	const uint32 TotalSamplesToSkip = SecondsToSkip * SampleRate * SoundWave->NumChannels;
	if (TotalSamplesToSkip >= TotalSampleCount)
	{
		UE_LOG(LogSpeech2FaceSolver, Error, TEXT("Could not get float samples with %d skipped samples from %d samples for SoundWave %s"), TotalSamplesToSkip, TotalSampleCount, *SoundWave->GetName());
		return false;
	}

	// Audio data is stored as 16 bit signed samples with channels interleaved so that must be taken into account
	const uint8* PcmDataPtr = PcmData.GetData() + TotalSamplesToSkip * sizeof(Sample);

	const uint32 SamplesToSkipPerChannel = SecondsToSkip * SampleRate;
	const uint32 SampleCountPerChannel = PcmData.Num() / (sizeof(Sample) * SoundWave->NumChannels) - SamplesToSkipPerChannel;
	OutSamples.SetNumUninitialized(SampleCountPerChannel);

	if (bDownmixChannels && SoundWave->NumChannels > 1)
	{
		const int32 SampleCount = TotalSampleCount - TotalSamplesToSkip;

		Audio::FAlignedFloatBuffer Buffer;
		Buffer.SetNumUninitialized(SampleCount);
		Audio::ArrayPcm16ToFloat(MakeArrayView((int16*)PcmDataPtr, SampleCount), Buffer);

		Audio::TSampleBuffer<float> FloatSampleBuffer(Buffer, SoundWave->NumChannels, SampleRate);
		FloatSampleBuffer.MixBufferToChannels(1);
		Audio::FAlignedFloatBuffer MonoBuffer;
		MonoBuffer.SetNumUninitialized(FloatSampleBuffer.GetNumSamples());
		MonoBuffer = FloatSampleBuffer.GetArrayView();
		const float MaxValue = Audio::ArrayMaxAbsValue(MonoBuffer);
		if (MaxValue > 1.f)
		{
			Audio::ArrayMultiplyByConstantInPlace(MonoBuffer, 1.f / MaxValue);
		}

		OutSamples = MonoBuffer;
	}
	else
	{
		for (uint32 SampleIndex = 0; SampleIndex < SampleCountPerChannel; SampleIndex++)
		{
			// Position ourselves at the sample of appropriate channel, taking into account the channel layout
			const uint8* SampleData = PcmDataPtr + ChannelToUse * sizeof(uint16);
			FMemory::Memcpy(&Sample, SampleData, sizeof(Sample));
			// Convert to range [-1.0, 1.0)
			OutSamples[SampleIndex] = Sample / 32768.0f;

			PcmDataPtr += sizeof(Sample) * SoundWave->NumChannels;
		}
	}

	if (SampleRate != AudioEncoderSampleRateHz)
	{
		FloatSamples ResampledAudio;
		if (!ResampleAudio(MoveTemp(OutSamples), SampleRate, AudioEncoderSampleRateHz, ResampledAudio))
		{
			UE_LOG(LogSpeech2FaceSolver, Error, TEXT("Could not resample audio from %d to %d for SoundWave %s"), SampleRate, AudioEncoderSampleRateHz, *SoundWave->GetName());
			return false;
		}
		OutSamples = MoveTemp(ResampledAudio);
	}

	return true;
}

bool FSpeech2FaceInternal::ResampleAudio(FloatSamples InSamples, int32 InSampleRate, int32 InResampleRate, FloatSamples& OutResampledSamples)
{
	const Audio::FResamplingParameters Params = {
		Audio::EResamplingMethod::Linear,
		1, // NumChannels
		static_cast<float>(InSampleRate),
		static_cast<float>(InResampleRate),
		InSamples
	};

	const int32 ExpectedSampleCount = GetOutputBufferSize(Params);
	OutResampledSamples.SetNumUninitialized(ExpectedSampleCount);

	Audio::FResamplerResults Result;
	Result.OutBuffer = &OutResampledSamples;

	const bool bIsSuccess = Audio::Resample(Params, Result);
	if (!bIsSuccess)
	{
		return false;
	}

	if (Result.OutputFramesGenerated != ExpectedSampleCount)
	{
		OutResampledSamples.SetNum(Result.OutputFramesGenerated, EAllowShrinking::No);
	}

	return true;
}

TArray<FSpeech2Face::FAnimationFrame> FSpeech2FaceInternal::ResampleAnimation(TArrayView<const float> InRawAnimation, TArrayView<const FString> InRigControlNames, uint32 ControlNum, float InOutputFps)
{
	const uint32 RawFrameCount = InRawAnimation.Num() / ControlNum;
	const float AnimationLengthSec = RawFrameCount * RigLogicPredictorFrameDuration;
	const uint32 ResampledFrameCount = FMath::FloorToInt32(AnimationLengthSec * InOutputFps);

	// Resample using linear interpolation
	TArray<FSpeech2Face::FAnimationFrame> ResampledAnimation;
	ResampledAnimation.AddDefaulted(ResampledFrameCount);

	for (uint32 ResampledFrameIndex = 0; ResampledFrameIndex < ResampledFrameCount; ++ResampledFrameIndex)
	{
		// Get corresponding raw frame time
		const float FrameStartSec = ResampledFrameIndex / InOutputFps;
		const float RawFrameIndex = FMath::Clamp(FrameStartSec * RigLogicPredictorOutputFps, 0, RawFrameCount - 1);

		// Get nearest full frames and distance between the two
		const uint32 PrevRawFrameIndex = FMath::FloorToInt32(RawFrameIndex);
		const uint32 NextRawFrameIndex = FMath::CeilToInt32(RawFrameIndex);
		const float RawFramesDelta = RawFrameIndex - PrevRawFrameIndex;

		// Add interpolated control values for the given frames
		ResampledAnimation[ResampledFrameIndex].Reserve(ControlNum);
		for (uint32 ControlIndex = 0; ControlIndex < ControlNum; ++ControlIndex)
		{
			const float PrevRawControlValue = InRawAnimation[PrevRawFrameIndex * ControlNum + ControlIndex];
			const float NextRawControlValue = InRawAnimation[NextRawFrameIndex * ControlNum + ControlIndex];
			const float ResampledValue = FMath::Lerp(PrevRawControlValue, NextRawControlValue, RawFramesDelta);

			ResampledAnimation[ResampledFrameIndex].Add(InRigControlNames[ControlIndex], ResampledValue);
		}
	}

	return ResampledAnimation;
}

#endif //WITH_EDITOR