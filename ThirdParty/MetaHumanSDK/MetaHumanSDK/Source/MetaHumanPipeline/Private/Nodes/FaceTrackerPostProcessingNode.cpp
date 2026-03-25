// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/FaceTrackerPostProcessingNode.h"
#include "Pipeline/PipelineData.h"

#include "MetaHumanFaceTrackerInterface.h"
#include "Features/IModularFeatures.h"

namespace UE::MetaHuman::Pipeline
{

FFaceTrackerPostProcessingNode::FFaceTrackerPostProcessingNode(const FString& InName) : FNode("FaceTrackerPostProcessingNode", InName)
{
	Pins.Add(FPin("Animation Out", EPinDirection::Output, EPinType::Animation));
}

bool FFaceTrackerPostProcessingNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const FName& FeatureName = IFaceTrackerNodeImplFactory::GetModularFeatureName();
	if (IModularFeatures::Get().IsModularFeatureAvailable(FeatureName))
	{
		IFaceTrackerNodeImplFactory& TrackerPostProcessingFactory = IModularFeatures::Get().GetModularFeature<IFaceTrackerNodeImplFactory>(FeatureName);
		Tracker = TrackerPostProcessingFactory.CreateFaceTrackerPostProcessingImplementor();
	}

	if (!Tracker->Init(TemplateData, ConfigData))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to initialize the post processing tracker");
		return false;
	}

	bool bLoadedDNA = false;

	if (!DNAAsset.IsExplicitlyNull())
	{
		if (DNAAsset.IsValid())
		{
			bLoadedDNA = Tracker->LoadDNA(DNAAsset.Get(), bSolveForTweakers ? HierarchicalDefinitionsData : DefinitionsData);
		}
	}
	else
	{
		bLoadedDNA = Tracker->LoadDNA(DNAFile, bSolveForTweakers ? HierarchicalDefinitionsData : DefinitionsData);
	}

	if (!bLoadedDNA)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to load dna file");
		return false;
	}

	if (!Tracker->SetCameras(Calibrations, Camera))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set cameras");
		return false;
	}

	if (!Tracker->SetGlobalTeethPredictiveSolver(PredictiveWithoutTeethSolver))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set global teeth predictive solver");
		return false;
	}

	Tracker->SetDisableGlobalSolves(bDisableGlobalSolves);
	if (!Tracker->OfflineSolvePrepare(0, TrackingData.Num(), TrackingData, FrameData, DebuggingFolder))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to prepare offline solver");
		return false;
	}

	FrameNumber = 0;

	return true;
}

bool FFaceTrackerPostProcessingNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	TArray<int32> UpdatedFrames;
	if (!Tracker->OfflineSolveProcessFrame(FrameNumber, 0, TrackingData.Num(), FrameData, UpdatedFrames))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToTrack);
		InPipelineData->SetErrorNodeMessage("Failed to track");
		return false;
	}

	if (UpdatedFrames.Num() > 0)
	{
		AnimationWindow.Reset();

		for (int32 UpdatedFrame : UpdatedFrames)
		{
			FFrameAnimationData Animation = FrameData[UpdatedFrame];
			if (DebuggingFolder.IsEmpty()) // if we have debugging turned on, save the mesh data to the asset
			{
				Animation.MeshData = FMetaHumanMeshData();
			}
			Animation.AnimationQuality = EFrameAnimationQuality::Final;

			AnimationWindow.Add(UpdatedFrame, MoveTemp(Animation));
		}
	}

	if (AnimationWindow.Contains(FrameNumber))
	{
		InPipelineData->SetData<FFrameAnimationData>(Pins[0], MoveTemp(AnimationWindow[FrameNumber]));
		AnimationWindow.Remove(FrameNumber);
	}
	else
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::BadFrame);
		InPipelineData->SetErrorNodeMessage("Bad frame");
		return false;
	}

	FrameNumber++;

	return true;
}

bool FFaceTrackerPostProcessingNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Tracker->SaveDebuggingData(0, TrackingData.Num(), TrackingData, FString(TEXT("post_final_solve_states.bin")), DebuggingFolder);
	Tracker = nullptr;

	AnimationWindow.Reset();

	return true;
}


FFaceTrackerPostProcessingManagedNode::FFaceTrackerPostProcessingManagedNode(const FString& InName) : FFaceTrackerPostProcessingNode(InName)
{
}

}