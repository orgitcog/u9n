// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Nodes/HyprsenseNodeBase.h"

namespace UE::MetaHuman::Pipeline
{
	class METAHUMANPIPELINE_API FHyprsenseNode : public FHyprsenseNodeBase
	{
	public:
		FHyprsenseNode(const FString& InName);

		virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
		virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

		bool SetTrackers(const TSharedPtr<UE::NNE::IModelInstanceGPU>& InFaceTracker, const TSharedPtr<UE::NNE::IModelInstanceGPU>& InFaceDetector, const TSharedPtr<UE::NNE::IModelInstanceGPU>& InEyebrowTracker, const TSharedPtr<UE::NNE::IModelInstanceGPU>& InEyeTracker, const TSharedPtr<UE::NNE::IModelInstanceGPU>& InLipsTracker, const TSharedPtr<UE::NNE::IModelInstanceGPU>& InLipZipTracker, const TSharedPtr<UE::NNE::IModelInstanceGPU>& InNasolabialNoseTracker, const TSharedPtr<UE::NNE::IModelInstanceGPU>& InChinTracker, const TSharedPtr<UE::NNE::IModelInstanceGPU>& InTeethTracker, const TSharedPtr<UE::NNE::IModelInstanceGPU>& InTeethConfidenceTracker);

		bool bAddSparseTrackerResultsToOutput = true;

	};


	// The managed node is a version of the above that take care of loading the correct NNE models
	// rather than these being specified by an externally.

	class METAHUMANPIPELINE_API FHyprsenseManagedNode : public FHyprsenseNode
	{
	public:
		FHyprsenseManagedNode(const FString& InName);

	};
}
