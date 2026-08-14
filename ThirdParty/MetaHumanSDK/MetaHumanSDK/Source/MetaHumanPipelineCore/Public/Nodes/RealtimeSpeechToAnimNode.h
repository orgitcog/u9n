// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"

namespace UE::NNE
{
	class IModelInstanceGPU;
}

namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINECORE_API FRealtimeSpeechToAnimNode : public FNode
{
public:

	FRealtimeSpeechToAnimNode(const FString& InName);

	virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool End(const TSharedPtr<FPipelineData>& InPipelineData) override;

	bool LoadModels();

	enum ErrorCode
	{
		FailedToInitialize = 0,
		UnsupportedNumberOfChannels,
		UnsupportedSampleRate,
		FailedToRun,
	};

private:

	TSharedPtr<UE::NNE::IModelInstanceGPU> Model = nullptr;

	TArray<float> AudioBuffer;
	TArray<float> CurveValues;
	TArray<float> KalmanBuffer;

	TArray<float> InputBuffer;
	TArray<float> FrameBuffer;

	FFrameAnimationData AnimOut;
};

}