// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/FaceTrackerPostProcessingFilterNode.h"
#include "Pipeline/PipelineData.h"
#include "MetaHumanFaceTrackerInterface.h"
#include "Features/IModularFeatures.h"


namespace UE::MetaHuman::Pipeline
{

FFaceTrackerPostProcessingFilterNode::FFaceTrackerPostProcessingFilterNode(const FString& InName) : FNode("FaceTrackerPostProcessingFilterNode", InName)
{
	Pins.Add(FPin("Animation Out", EPinDirection::Output, EPinType::Animation));
}

bool FFaceTrackerPostProcessingFilterNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	IFaceTrackerNodeImplFactory& FilterFactory = IModularFeatures::Get().GetModularFeature<IFaceTrackerNodeImplFactory>(IFaceTrackerNodeImplFactory::GetModularFeatureName());
	Filter = FilterFactory.CreateFaceTrackerPostProcessingFilterImplementor();

	if (!Filter->Init(TemplateData, ConfigData))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to load initialise the post processing filter");
		return false;
	}

	bool bLoadedDNA = false;

	if (!DNAAsset.IsExplicitlyNull())
	{
		if (DNAAsset.IsValid())
		{
			bLoadedDNA = Filter->LoadDNA(DNAAsset.Get(), bSolveForTweakers ? HierarchicalDefinitionsData : DefinitionsData);
		}
	}
	else
	{
		bLoadedDNA = Filter->LoadDNA(DNAFile, bSolveForTweakers ? HierarchicalDefinitionsData : DefinitionsData);
	}

	if (!bLoadedDNA)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to load dna file");
		return false;
	}

	if (!Filter->OfflineFilter(0, FrameData.Num(), FrameData, DebuggingFolder))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to run offline filter");
		return false;
	}

	FrameNumber = 0;

	return true;
}

bool FFaceTrackerPostProcessingFilterNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	// actual processing happened in startup but this pops the results out in the pipeline
	FrameData[FrameNumber].AnimationQuality = EFrameAnimationQuality::PostFiltered;
	InPipelineData->SetData<FFrameAnimationData>(Pins[0], MoveTemp(FrameData[FrameNumber]));
	FrameNumber++;
	return true;
}

bool FFaceTrackerPostProcessingFilterNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Filter.Reset();
	return true;
}



FFaceTrackerPostProcessingFilterManagedNode::FFaceTrackerPostProcessingFilterManagedNode(const FString& InName) : FFaceTrackerPostProcessingFilterNode(InName)
{
}

}