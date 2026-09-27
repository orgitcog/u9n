// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Nodes/HyprsenseNodeBase.h"

namespace UE::MetaHuman::Pipeline
{
	class METAHUMANPIPELINE_API FHyprsenseSparseNode : public FHyprsenseNodeBase
	{
	public:
		FHyprsenseSparseNode(const FString& InName);

		virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
		virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
		
		bool SetTrackers(const TSharedPtr<UE::NNE::IModelInstanceGPU>& InFaceTracker, const TSharedPtr<UE::NNE::IModelInstanceGPU>& InFaceDetector);
	};


	// The managed node is a version of the above that take care of loading the correct NNE models
	// rather than these being specified by an externally.

	class METAHUMANPIPELINE_API FHyprsenseSparseManagedNode : public FHyprsenseSparseNode
	{
	public:
		FHyprsenseSparseManagedNode(const FString& InName);

	};
}
