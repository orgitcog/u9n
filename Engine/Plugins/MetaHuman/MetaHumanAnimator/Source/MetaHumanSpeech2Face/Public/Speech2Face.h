// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundWave.h"
#include "Containers/Map.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FSpeech2FaceInternal;

#if WITH_EDITOR

#include "AudioDrivenAnimationConfig.h"

/**
 * Class for generating face animation for animating RigLogic rigs using the provided speech recording.
 */
class METAHUMANSPEECH2FACE_API FSpeech2Face final
{
public:
	struct FAudioParams
	{
		explicit FAudioParams(TWeakObjectPtr<const USoundWave> InSpeechRecording, float InAudioStartOffsetSec = 0, bool bInMixChannels = true, int32 InAudioChannelIndex = 0) :
		SpeechRecording(MoveTemp(InSpeechRecording)), AudioStartOffsetSec(InAudioStartOffsetSec), bDownmixChannels(bInMixChannels), AudioChannelIndex(InAudioChannelIndex)
		{
		}

		TWeakObjectPtr<const USoundWave> SpeechRecording;
		float AudioStartOffsetSec = 0;
		bool bDownmixChannels = true;
		int32 AudioChannelIndex = 0;
	};

	using FAnimationFrame = TMap<FString, float>;

	static constexpr float AudioEncoderOutputFps = 100.0f;
	static constexpr float AudioEncoderWarmUpSec = 0.;

	~FSpeech2Face();

	/**
	 * Creates a FSpeech2Face instance. The instance can be reused and can be used to generate different animation in parallel. The necessary neural 
	 * network models are loaded during creation.
	 * 
	 * @return Returns the instance of FSpeech2Face if successful, nullptr otherwise.
	 */
	static TUniquePtr<FSpeech2Face> Create();

	/**
	 * Creates a FSpeech2Face instance, using the specified models. The instance can be reused and can be used to
	 * generate different animation in parallel. The necessary neural network models are loaded during creation.
	 *
	 * @return Returns the instance of FSpeech2Face if successful, nullptr otherwise.
	 */
	static TUniquePtr<FSpeech2Face> Create(const FAudioDrivenAnimationModels& InModels);

	/** 
	* Sets the desired mood for the resulting animation.
	*/
	void SetMood(const EAudioDrivenAnimationMood& InMood); 

	/**
	* Sets the desired mood intensity for the resulting animation.
	*/
	void SetMoodIntensity(float InMoodIntensity); 

	/**
	 * Generates RigLogic face animation based on the input audio. Generated animation uses the so called "face board" rig controls.
	 * 
	 * Raw animation is generated at 50 FPS which is then resampled to the specified FPS using the nearest neighbour algorithm.
	 *
	 * @param InAudioParams Parameters for the audio - USoundWave asset, start offset and channel from the asset that should be used.
	 * @param InOutputAnimationFps The FPS that the output animation should have.
	 * @param bInGenerateBlinks Option to generate blink animation
	 * @param OutAnimation Generated animation. Each AnimationFrame is map that maps a rig control to it's value for that frame.
	 * @param OutHeadAnimation Generated animation for the head pose. Each AnimationFrame is map that maps a rig control to it's value for that frame.
	 * @return Returns true if the operation was successful and false otherwise.
	 */
	bool GenerateFaceAnimation(const FAudioParams& InAudioParams,
							   float InOutputAnimationFps,
							   bool bInGenerateBlinks,
							   TFunction<bool()> InShouldCancelCallback,
							   TArray<FAnimationFrame>& OutAnimation,
							   TArray<FAnimationFrame>& OutHeadAnimation);


private:
	FSpeech2Face(TUniquePtr<FSpeech2FaceInternal> InPimpl);
	TUniquePtr<FSpeech2FaceInternal> Pimpl;
};

#endif //WITH_EDITOR

namespace UE::MetaHuman
{

UE_INTERNAL
METAHUMANSPEECH2FACE_API void ReplaceHeadGuiControlsWithRaw(TMap<FString, float>& OutControlMap);

UE_INTERNAL
METAHUMANSPEECH2FACE_API TSet<FString> GetMouthOnlyRawControls();

UE_INTERNAL
METAHUMANSPEECH2FACE_API FTransform GetHeadPoseTransformFromRawControls(const TMap<FString, float>& InAnimationData);

}
