// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"
#include "MetaHumanLocalLiveLinkSubject.h"



namespace UE::MetaHuman::Pipeline
{

class FVideoSourceNode : public FNode
{
public:

	FVideoSourceNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool End(const TSharedPtr<FPipelineData>& InPipelineData) override;

	enum ErrorCode
	{
		GeneralError = 0,
	};

	class FVideoSample
	{
	public:
		FUEImageDataType Image;
		FQualifiedFrameTime Time;
		FMetaHumanLocalLiveLinkSubject::ETimeSource TimeSource = FMetaHumanLocalLiveLinkSubject::ETimeSource::NotSet;
	};

	void AddVideoSample(FVideoSample&& InVideoSample);
	void SetError(const FString& InErrorMessage);

private:

	FCriticalSection Mutex;
	TArray<FVideoSample> VideoSamples;
	FString ErrorMessage;
};

}
