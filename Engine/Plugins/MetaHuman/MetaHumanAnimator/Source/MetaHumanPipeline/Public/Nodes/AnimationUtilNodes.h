// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"

namespace UE::MetaHuman::Pipeline
{

// Merge two sets of AnimationData together. The node takes two FFrameAnimationData's as input.
// The output is a copy of the first FFrameAnimationData but with its AnimationData updated 
// using the controls present in the second FFrameAnimationData's AnimationData. If the second
// set of AnimationData contains a key not present in the first, then an error occurs. However,
// the second set of AnimationData can be a sub-set of the first.
 
class METAHUMANPIPELINE_API FAnimationMergeNode : public FNode
{
public:

	FAnimationMergeNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	enum ErrorCode
	{
		UnknownControlValue = 0
	};
};

}
