// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"
#include "MetaHumanRealtimeSmoothing.h"



namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINECORE_API FHyprsenseRealtimeSmoothingNode : public FNode
{
public:

	FHyprsenseRealtimeSmoothingNode(const FString& InName);

	virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool End(const TSharedPtr<FPipelineData>& InPipelineData) override;

	TMap<FName, FMetaHumanRealtimeSmoothingParam> Parameters;
	double DeltaTime = 0;

	enum ErrorCode
	{
		SmoothingFailed = 0,
	};

private:

	TSharedPtr<FMetaHumanRealtimeSmoothing> Smoothing;
	TArray<FName> Keys;
};

}