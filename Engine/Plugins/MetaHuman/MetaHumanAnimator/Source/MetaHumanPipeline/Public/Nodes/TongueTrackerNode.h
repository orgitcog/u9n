// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"
#include "SpeechToAnimNode.h"

namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINE_API FTongueTrackerNode : public FSpeechToAnimNode
{
public:

	FTongueTrackerNode(const FString& InName);
	virtual ~FTongueTrackerNode() override;

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

private:
	bool PostConversionModifyRawControls(TMap<FString, float>& InOutAnimationFrame, FString& OutErrorMsg) override;

	static const TArray<FString> AffectedRawTongueControls;
};

}
#endif // WITH_EDITOR