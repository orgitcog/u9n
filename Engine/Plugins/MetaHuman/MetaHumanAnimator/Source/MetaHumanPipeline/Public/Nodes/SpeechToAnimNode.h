// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"
#include "DNAAsset.h"
#include "Speech2Face.h"

namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINE_API FSpeechToAnimNode : public FNode
{
public:

	FSpeechToAnimNode(const FString& InName);
	FSpeechToAnimNode(const FString& InTypeName, const FString& InName);
	virtual ~FSpeechToAnimNode() override;

	virtual bool LoadModels();
	bool LoadModels(const FAudioDrivenAnimationModels& InModels);
	void SetMood(const EAudioDrivenAnimationMood& InMood);
	void SetMoodIntensity(float InMoodIntensity);
	void SetOutputControls(const EAudioDrivenAnimationOutputControls& InOutputControls);

	virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool End(const TSharedPtr<FPipelineData>& InPipelineData) override;

	void CancelModelSolve();

	TWeakObjectPtr<class USoundWave> Audio = nullptr;
	bool bDownmixChannels = true;
	uint32 AudioChannelIndex = 0;
	uint32 ProcessingStartFrameOffset = 0; // Index of the first frame to arrive through pipeline
	float OffsetSec = 0; // When in audio to start solving
	float FrameRate = 0;
	bool bClampTongueInOut = true;
	bool bGenerateBlinks = true;

	enum ErrorCode
	{
		InvalidAudio = 0,
		InvalidChannelIndex,
		FailedToSolveSpeechToAnimation,
		FailedToInitialize,
		InvalidFrame,
		FailedToModifyUiControls,
		FailedToModifyRawControls
	};

protected:
	TUniquePtr<FSpeech2Face> Speech2Face = nullptr;

	TArray<TMap<FString, float>> Animation;
	TArray<TMap<FString, float>> HeadAnimation;
	bool bCancelStart = false;

private:
	virtual bool PreConversionModifyUiControls(TMap<FString, float>& InOutAnimationFrame, FString& OutErrorMsg);
	virtual bool PostConversionModifyRawControls(TMap<FString, float>& InOutAnimationFrame, FString& OutErrorMsg);
	void PrepareFromOutputControls();

	EAudioDrivenAnimationOutputControls OutputControls = EAudioDrivenAnimationOutputControls::FullFace;
	EAudioProcessingMode ProcessingMode = EAudioProcessingMode::Undefined;
	TSet<FString> ActiveRawControls;
};

}
#endif // WITH_EDITOR