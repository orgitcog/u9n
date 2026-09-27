// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Speech2Face.h"
#include "Containers/ArrayView.h"
#include "AudioResampler.h"
#include "NNERuntimeCPU.h"

class UNNEModelData;

DECLARE_LOG_CATEGORY_EXTERN(LogSpeech2FaceSolver, Log, All);

#if WITH_EDITOR
class FSpeech2FaceInternal final {
public:
	static TUniquePtr<FSpeech2FaceInternal> Create(const FAudioDrivenAnimationModels& InModels);

	bool GenerateFaceAnimation(const FSpeech2Face::FAudioParams& InAudioParams,
							   float InOutputAnimationFps,
							   bool bInGenerateBlinks,
							   TFunction<bool()> InShouldCancelCallback,
							   TArray<FSpeech2Face::FAnimationFrame>& OutAnimation,
							   TArray<FSpeech2Face::FAnimationFrame>& OutHeadAnimation) const;

	void SetMood(const EAudioDrivenAnimationMood& InMood);
	void SetMoodIntensity(float InMoodIntensity);

public:
	/** Predictor generates animation at 50fps*/
	static constexpr float RigLogicPredictorOutputFps = 50.0f;
	
private:
	using FloatSamples = Audio::VectorOps::FAlignedFloatBuffer;

	FSpeech2FaceInternal();
	bool Init(const FAudioDrivenAnimationModels& InModels);
	TSharedPtr<UE::NNE::IModelInstanceCPU> TryLoadModelData(const FSoftObjectPath& InModelAssetPath);

	static bool GetFloatSamples(const TWeakObjectPtr<const USoundWave>& SoundWave, const TArray<uint8>& PcmData, uint32 SampleRate, bool bDownmixChannels, uint32 ChannelToUse, float SecondsToSkip, FloatSamples& OutSamples);
	static bool ResampleAudio(FloatSamples InSamples, int32 InSampleRate, int32 InResampleRate, FloatSamples& OutResampledSamples);
	static bool ExtractAudioFeatures(const FloatSamples& Samples, const TSharedPtr<UE::NNE::IModelInstanceCPU>& AudioExtractor, TArray<float>& OutAudioData);

	bool RunPredictor(
		uint32 ControlNum, 
		uint32 BlinkControlNum, 
		uint32 SamplesNum, 
		const TArray<float>& AudioData, 
		TArray<float>& OutRigLogicValues, 
		TArray<float>& OutRigLogicBlinkValues,
		TArray<float>& OutRigLogicHeadValues
	) const;


	static TArray<FSpeech2Face::FAnimationFrame> ResampleAnimation(TArrayView<const float> InRawAnimation, TArrayView<const FString> InRigControlNames, uint32 ControlNum, float InOutputFps);
	int32 GetModelMoodIndex() const;

private:
	/** The model is expecting to process audio sampled at 16kHz */
	static constexpr uint32 AudioEncoderSampleRateHz = 16000;
	/** The model does not allow processing of more than 30 seconds of audio */
	static constexpr float RigLogicPredictorMaxAudioSamples = AudioEncoderSampleRateHz * 30;
	static constexpr float RigLogicPredictorFrameDuration = 1.f / RigLogicPredictorOutputFps;
	static constexpr float SamplesPerFrame = AudioEncoderSampleRateHz * RigLogicPredictorFrameDuration;

	TSharedPtr<UE::NNE::IModelInstanceCPU> AudioExtractor;
	TSharedPtr<UE::NNE::IModelInstanceCPU> RigLogicPredictor;

	// The order of these controls is specific and keyed to the model output, so this is not just a list of GUI controls for the head
	const TArray<FString> ModelHeadControls = {
		TEXT("HeadTranslationY"),
		TEXT("HeadTranslationZ"),
		TEXT("HeadRoll"),
		TEXT("HeadPitch"),
		TEXT("HeadYaw"),
	};

	// We default to safe (netural) mood values to ensure nodes which derive from FSpeechToAnimNode have sensible baseline behaviour
	EAudioDrivenAnimationMood DesiredMood = EAudioDrivenAnimationMood::Neutral;
	float DesiredMoodIntensity = 1.0f;
};

#endif //WITH_EDITOR