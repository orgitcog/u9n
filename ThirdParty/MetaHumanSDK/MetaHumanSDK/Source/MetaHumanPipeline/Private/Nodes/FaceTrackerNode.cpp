// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/FaceTrackerNode.h"
#include "Pipeline/PipelineData.h"
#include "Misc/FileHelper.h"

#include "HAL/PlatformFileManager.h"
#include "HAL/IConsoleManager.h"
#include "MetaHumanConformer.h"

#include "MetaHumanFaceTrackerInterface.h"
#include "Features/IModularFeatures.h"

#include "Pipeline/Log.h"


namespace UE::MetaHuman::Pipeline
{
FFaceTrackerStereoNode::FFaceTrackerStereoNode(const FString& InName) : FNode("FaceTrackerStereo", InName)
{
	Pins.Add(FPin("UE Image 0 In", EPinDirection::Input, EPinType::UE_Image, 0));
	Pins.Add(FPin("Contours 0 In", EPinDirection::Input, EPinType::Contours, 0));
	Pins.Add(FPin("UE Image 1 In", EPinDirection::Input, EPinType::UE_Image, 1));
	Pins.Add(FPin("Contours 1 In", EPinDirection::Input, EPinType::Contours, 1));
	Pins.Add(FPin("Animation Out", EPinDirection::Output, EPinType::Animation));
}

bool FFaceTrackerStereoNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IFaceTrackerNodeImplFactory::GetModularFeatureName()))
	{
		IFaceTrackerNodeImplFactory& FaceTrackerImplFactory = IModularFeatures::Get().GetModularFeature<IFaceTrackerNodeImplFactory>(IFaceTrackerNodeImplFactory::GetModularFeatureName());
		Tracker = FaceTrackerImplFactory.CreateFaceTrackerImplementor();
	}

	if (!Tracker.IsValid() || !Tracker->Init(SolverTemplateData, SolverConfigData, {}, InPipelineData->GetUseGPU()))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to initialize the tracker");
		return false;
	}
	
	if (Calibrations.Num() != 2)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Must have 2 cameras");
		return false;
	}

	bool bLoadedDNA = false;

	if (!DNAAsset.IsExplicitlyNull())
	{
		if (DNAAsset.IsValid())
		{
			bLoadedDNA = Tracker->LoadDNA(DNAAsset.Get());
		}
	}
	else
	{
		bLoadedDNA = Tracker->LoadDNA(DNAFile);
	}

	if (!bLoadedDNA)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to load dna file");
		return false;
	}

	if (!Tracker->SetCameras(Calibrations))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set cameras");
		return false;
	}

	if (!Tracker->ResetTrack(0, 2000, {})) // FIX THIS - HARDWIRED MAX LIMIT OF 2000 FRAMES TO SOLVE, DONE FOR IPHONE (MONO) CASE
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to reset track");
		return false;
	}

	TMap<FString, TPair<float, float>> Ranges;
	TArray<TPair<FString, FString>> Pairs;

	Ranges.Add(Calibrations[0].CameraId, TPair<float, float>(10.0, 25.0));
	Ranges.Add(Calibrations[1].CameraId, TPair<float, float>(10.0, 25.0));
	Pairs.Add(TPair<FString, FString>(Calibrations[0].CameraId, Calibrations[1].CameraId));

	if (!Tracker->SetCameraRanges(Ranges))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set camera range");
		return false;
	}

	if (!Tracker->SetStereoCameraPairs(Pairs))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set stereo pairs");
		return false;
	}

	// TODO need to sort out brow tracking in this node if we use it again

	return true;
}

bool FFaceTrackerStereoNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const FUEImageDataType& Image0 = InPipelineData->GetData<FUEImageDataType>(Pins[0]);
	const FFrameTrackingContourData& Contours0 = InPipelineData->GetData<FFrameTrackingContourData>(Pins[1]);
	const FUEImageDataType& Image1 = InPipelineData->GetData<FUEImageDataType>(Pins[2]);
	const FFrameTrackingContourData& Contours1 = InPipelineData->GetData<FFrameTrackingContourData>(Pins[3]);

	TMap<FString, const unsigned char*> ImageDataMap;
	TMap<FString, const FFrameTrackingContourData*> LandmarkMap;

	ImageDataMap.Add(Calibrations[0].CameraId, Image0.Data.GetData());
	ImageDataMap.Add(Calibrations[1].CameraId, Image1.Data.GetData());

	LandmarkMap.Add(Calibrations[0].CameraId, &Contours0);
	LandmarkMap.Add(Calibrations[1].CameraId, &Contours1);

	if (Tracker->SetInputData(ImageDataMap, LandmarkMap))
	{
		int32 FrameNumber = InPipelineData->GetFrameNumber();

		if (Tracker->Track(FrameNumber, {}, false, {}, bSkipPredictiveSolver, bSkipPerVertexSolve))
		{
			FTransform HeadPose;
			TMap<FString, float> Controls;
			TMap<FString, float> RawControls;
			TArray<float> FaceMeshVertData, TeethMeshVertData, LeftEyeMeshVertData, RightEyeMeshVertData;
			TArray<float> HeadPoseRaw;

			if (!Tracker->GetTrackingState(FrameNumber, HeadPose, HeadPoseRaw, Controls, RawControls, FaceMeshVertData, TeethMeshVertData, LeftEyeMeshVertData, RightEyeMeshVertData))
			{
				InPipelineData->SetErrorNodeCode(ErrorCode::FailedToTrack);
				InPipelineData->SetErrorNodeMessage("Failed to get state");
				return false;
			}

			FFrameAnimationData Animation;
			Animation.Pose = HeadPose;
			Animation.RawPoseData = HeadPoseRaw;
			Animation.AnimationData = Controls;
			Animation.RawAnimationData = RawControls;
			Animation.MeshData = { FaceMeshVertData, TeethMeshVertData, LeftEyeMeshVertData, RightEyeMeshVertData };
			Animation.AnimationQuality = EFrameAnimationQuality::Preview;
			InPipelineData->SetData<FFrameAnimationData>(Pins[4], MoveTemp(Animation));
		}
		else
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToTrack);
			InPipelineData->SetErrorNodeMessage("Failed to track");
			return false;
		}
	}
	else
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToTrack);
		InPipelineData->SetErrorNodeMessage("Failed to set input data");
		return false;
	}

	return true;
}

bool FFaceTrackerStereoNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Tracker.Reset();

	return true;
}


FFaceTrackerIPhoneNode::FFaceTrackerIPhoneNode(const FString& InName) : FNode("FaceTrackerIPhone", InName)
{
	Pins.Add(FPin("UE Image In", EPinDirection::Input, EPinType::UE_Image));
	Pins.Add(FPin("Contours In", EPinDirection::Input, EPinType::Contours));
	Pins.Add(FPin("Depth In", EPinDirection::Input, EPinType::Depth));
	Pins.Add(FPin("Flow In", EPinDirection::Input, EPinType::FlowOutput));
	Pins.Add(FPin("Animation Out", EPinDirection::Output, EPinType::Animation));
	Pins.Add(FPin("Scale Diagnostics Out", EPinDirection::Output, EPinType::Float));
}

bool FFaceTrackerIPhoneNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	bIsFirstPass = true;

	if (IModularFeatures::Get().IsModularFeatureAvailable(IFaceTrackerNodeImplFactory::GetModularFeatureName()))
	{
		IFaceTrackerNodeImplFactory& FaceTrackerImplFactory = IModularFeatures::Get().GetModularFeature<IFaceTrackerNodeImplFactory>(IFaceTrackerNodeImplFactory::GetModularFeatureName());
		Tracker = FaceTrackerImplFactory.CreateFaceTrackerImplementor();
	}

	if (!Tracker.IsValid() || !Tracker->Init(SolverTemplateData, SolverConfigData, OptFlowConfig, InPipelineData->GetUseGPU()))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to initialize the tracker");
		return false;
	}

	bool bLoadedDNA = false;

	if (!DNAAsset.IsExplicitlyNull())
	{
		if (DNAAsset.IsValid())
		{
			bLoadedDNA = Tracker->LoadDNA(DNAAsset.Get());
		}
	}
	else
	{
		bLoadedDNA = Tracker->LoadDNA(DNAFile);
	}

	if (!bLoadedDNA)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to load dna file");
		return false;
	}

	if (!DebuggingFolder.IsEmpty())
	{
		// create a debugging folder
		// note that we don't check whether saving debugging data has been successful
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		if (!PlatformFile.DirectoryExists(*DebuggingFolder))
		{
			PlatformFile.CreateDirectory(*DebuggingFolder);
		}
	}

	if (PCARigMemoryBuffer.IsEmpty())
	{
		if (!UE::Wrappers::FMetaHumanConformer::CalculatePcaModelFromDnaRig(SolverPCAFromDNAData, DNAFile, PCARigMemoryBuffer))
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToCalculatePCA);
			InPipelineData->SetErrorNodeMessage("Failed to calculate PCA model");
			return false;
		}
	}

	bool bSetPCARig = Tracker->SetPCARig(PCARigMemoryBuffer);
	if (!bSetPCARig)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set the PCA rig for face-tracking");
		return false;
	}

	bool bSetBrowMeshLandmarks = false;
	if (!BrowJSONData.IsEmpty())
	{
		BrowJSONData.Add(0); // null terminate byte array as we are treating it as an ascii string
		FString BrowJSONString(UTF8_TO_TCHAR(BrowJSONData.GetData()));
		bSetBrowMeshLandmarks = Tracker->AddBrowMeshLandmarks(BrowJSONString);
	}

	if (!bSetBrowMeshLandmarks)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set the brow landmarks for face-tracking");
		return false;
	}

	// train the predictive solvers if needed; note that this is code path is currently only used by the pipeline tests and is not in general use
	if (PredictiveWithoutTeethSolver.IsEmpty() || (PredictiveSolvers.IsEmpty() && !bSkipPredictiveSolver))
	{	
		// load and/or train the predictive solvers. 
		// The predictive solver files below are the predictive solver training data 
		// The predictive solver data files below are training data, from which any other predictive solver can be trained.

		// use the synchronous version of the training as we are already in a thread
		bool bTrainedPredictiveSolver = Tracker->TrainSolverModelsSync(PredictiveSolverGlobalTeethTrainingData, PredictiveSolverTrainingData);
		if (!bTrainedPredictiveSolver)
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
			InPipelineData->SetErrorNodeMessage("Failed to train predictive solvers");
			return false;
		}
		// set the solver models in the original node
		if (!bSkipPredictiveSolver)
		{
			bool bGotPredictiveSolvers = Tracker->GetPredictiveSolvers(PredictiveSolvers);
			if (!bGotPredictiveSolvers)
			{
				InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
				InPipelineData->SetErrorNodeMessage("Failed to train predictive solvers");
				return false;
			}
		}

		bool bGotGlobalTeethSolver = Tracker->GetGlobalTeethPredictiveSolver(PredictiveWithoutTeethSolver);
		if (!bGotGlobalTeethSolver)
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
			InPipelineData->SetErrorNodeMessage("Failed to train global teeth predictive solver");
			return false;
		}
	}

	if (!bSkipPredictiveSolver && !Tracker->SetPredictiveSolvers(PredictiveSolvers))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set preview solve predictive solvers");
		return false;
	}

	if (!Tracker->SetGlobalTeethPredictiveSolver(PredictiveWithoutTeethSolver))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set global teeth predictive solver");
		return false;
	}

	FrameNumber = 0;

	return true;
}


bool FFaceTrackerIPhoneNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	if (bIsFirstPass)
	{

		if (Calibrations.Num() != 2 && Calibrations.Num() != 3)
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToTrack);
			InPipelineData->SetErrorNodeMessage("Must have 2 or 3 cameras");
			return false;
		}

		if (!Tracker->SetCameras(Calibrations))
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToTrack);
			InPipelineData->SetErrorNodeMessage("Failed to set cameras");
			return false;
		}

		if (!Tracker->ResetTrack(0, NumberOfFrames, OptFlowConfig))
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
			InPipelineData->SetErrorNodeMessage("Failed to reset track");
			return false;
		}
	}

	const FUEImageDataType& Image = InPipelineData->GetData<FUEImageDataType>(Pins[0]);
	const FFrameTrackingContourData& Contours = InPipelineData->GetData<FFrameTrackingContourData>(Pins[1]);
	const FDepthDataType& Depth = InPipelineData->GetData<FDepthDataType>(Pins[2]);

	TMap<FString, const unsigned char*> ImageDataMap;
	TMap<FString, const FFrameTrackingContourData*> LandmarkMap;
	TMap<FString, const float*> DepthDataMap;

	if (!Contours.ContainsData())
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::NoContourData);
		InPipelineData->SetErrorNodeMessage("Processed frame contains no tracked face contour data.");
		return false;
	}

	ImageDataMap.Add(Camera, Image.Data.GetData());
	LandmarkMap.Add(Camera, &Contours);
	const FCameraCalibration* DepthCalibrationPtr = Calibrations.FindByPredicate([](const FCameraCalibration& InCalibration)
	{
		return InCalibration.CameraType == FCameraCalibration::Depth;
	});

	if (!DepthCalibrationPtr)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToFindCalibration);
		InPipelineData->SetErrorNodeMessage("Failed to find the calibration for the depth camera");
		return false;
	}

	DepthDataMap.Add(DepthCalibrationPtr->CameraId, Depth.Data.GetData());
	bool bEstimatedScale = false;

	if (Tracker->SetInputData(ImageDataMap, LandmarkMap, DepthDataMap))
	{
		// if first frame and diagnostics on, estimate the rig scale
		if (bIsFirstPass && !bSkipDiagnostics)
		{
			// estimate scale
			float Scale = 1.0f;
			bEstimatedScale = Tracker->EstimateScale(FrameNumber, Scale);
			if (!bEstimatedScale)
			{
				// we don't want the pipeline to fail because of a diagnostics error, so just log this as a warning
				UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to calculate head scale diagnostics"));
			}
			InPipelineData->SetData<float>(Pins[5], Scale);
		}

		// See FaceTrackerAPI.h for more info on how flow is passed.
		TMap<FString,									// Camera name to flow info map
			TPair<										// Flow info - a pair for both ways in which flow can be specified 
			TPair<const float*, const float*>,		// Flow specified as an image pair - can be null
			TPair<									// Flow specified as precomputed results, which is a pair of:
			TPair<const float*, const float*>,	//		flow data and confidence - can be null
			TPair<const float*, const float*>	//		flow image cameras - can be null
			>>> FlowInfo;
		TPair<const float*, const float*> FlowImages = { nullptr, nullptr };
		TPair<TPair<const float*, const float*>, TPair<const float*, const float*>> FlowResults;
		TPair<const float*, const float*> FlowDataAndConfidence;
		TPair<const float*, const float*> FlowCamera;
		if (OptFlowConfig.bUseOpticalFlow)
		{
			const FFlowOutputDataType& Flow = InPipelineData->GetData<FFlowOutputDataType>(Pins[3]);
			FlowDataAndConfidence = { Flow.Flow.GetData(), Flow.Confidence.GetData() };
			FlowCamera = { Flow.SourceCamera.GetData(), Flow.TargetCamera.GetData() };
		}
		else
		{
			FlowDataAndConfidence = { nullptr, nullptr };
			FlowCamera = { nullptr, nullptr };
		}
		FlowResults = { FlowDataAndConfidence, FlowCamera };
		FlowInfo.Add(Camera, { FlowImages, FlowResults });

		if (Tracker->Track(FrameNumber, FlowInfo, false, DebuggingFolder, bSkipPredictiveSolver, bSkipPerVertexSolve))
		{
			FTransform HeadPose;
			TArray<float> HeadPoseRaw;
			TMap<FString, float> Controls;
			TMap<FString, float> RawControls;
			TArray<float> FaceMeshVertData, TeethMeshVertData, LeftEyeMeshVertData, RightEyeMeshVertData;

			if (!Tracker->GetTrackingState(FrameNumber, HeadPose, HeadPoseRaw, Controls, RawControls, FaceMeshVertData, TeethMeshVertData, LeftEyeMeshVertData, RightEyeMeshVertData))
			{
				InPipelineData->SetErrorNodeCode(ErrorCode::FailedToTrack);
				InPipelineData->SetErrorNodeMessage("Failed to get state");
				return false;
			}

			FFrameAnimationData Animation;
			Animation.Pose = HeadPose;
			Animation.RawPoseData = HeadPoseRaw;
			Animation.AnimationData = Controls;
			Animation.RawAnimationData = RawControls;
			Animation.MeshData = { FaceMeshVertData, TeethMeshVertData, LeftEyeMeshVertData, RightEyeMeshVertData };
			Animation.AnimationQuality = EFrameAnimationQuality::Preview;
			InPipelineData->SetData<FFrameAnimationData>(Pins[4], MoveTemp(Animation));
		}
		else
		{
			if (bTrackingFailureIsError)
			{
				InPipelineData->SetErrorNodeCode(ErrorCode::FailedToTrack);
				InPipelineData->SetErrorNodeMessage("Failed to track");
				return false;
			}
			else
			{
				FFrameAnimationData Animation;
				InPipelineData->SetData<FFrameAnimationData>(Pins[4], MoveTemp(Animation));
			}
		}
	}
	else
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToTrack);
		InPipelineData->SetErrorNodeMessage("Failed to set input data");
		return false;
	}

	bIsFirstPass = false; 
	FrameNumber++;
	return true;
}

bool FFaceTrackerIPhoneNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Tracker.Reset();
	return true;
}

FFaceTrackerIPhoneManagedNode::FFaceTrackerIPhoneManagedNode(const FString& InName) : FFaceTrackerIPhoneNode(InName)
{
}



FDepthGenerateNode::FDepthGenerateNode(const FString& InName) : FNode("DepthGenerate", InName)
{
	Pins.Add(FPin("UE Image 0 In", EPinDirection::Input, EPinType::UE_Image, 0));
	Pins.Add(FPin("UE Image 1 In", EPinDirection::Input, EPinType::UE_Image, 1));
	Pins.Add(FPin("Depth Out", EPinDirection::Output, EPinType::Depth));
}

bool FDepthGenerateNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IFaceTrackerNodeImplFactory::GetModularFeatureName()))
	{
		IFaceTrackerNodeImplFactory& DepthGenerationImplFactory = IModularFeatures::Get().GetModularFeature<IFaceTrackerNodeImplFactory>(IFaceTrackerNodeImplFactory::GetModularFeatureName());
		Reconstructer = DepthGenerationImplFactory.CreateDepthGeneratorImplementor();
	}

	if (!Reconstructer.IsValid())
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Make sure Depth Generation plugin is enabled");
		return false;
	}

	if (Calibrations.Num() != 2)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Must have 2 cameras");
		return false;
	}

	if (!Reconstructer->Init())
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to initialize the stereo reconstructer");
		return false;
	}

	if (!Reconstructer->SetCameras(Calibrations))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set cameras");
		return false;
	}

	TMap<FString, TPair<float, float>> Ranges;
	TArray<TPair<FString, FString>> Pairs;

	// Same ranges for both cameras
	Ranges.Add(Calibrations[0].CameraId, TPair<float, float>(DistanceRange.GetLowerBoundValue(), DistanceRange.GetUpperBoundValue()));
	Ranges.Add(Calibrations[1].CameraId, TPair<float, float>(DistanceRange.GetLowerBoundValue(), DistanceRange.GetUpperBoundValue()));
	Pairs.Add(TPair<FString, FString>(Calibrations[0].CameraId, Calibrations[1].CameraId));

	if (!Reconstructer->SetCameraRanges(Ranges))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set camera range");
		return false;
	}

	if (!Reconstructer->SetStereoCameraPairs(Pairs))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to set stereo pairs");
		return false;
	}

	return true;
}

bool FDepthGenerateNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const FUEImageDataType& Image0 = InPipelineData->GetData<FUEImageDataType>(Pins[0]);
	const FUEImageDataType& Image1 = InPipelineData->GetData<FUEImageDataType>(Pins[1]);

	TMap<FString, const unsigned char*> ImageDataMap;

	ImageDataMap.Add(Calibrations[0].CameraId, Image0.Data.GetData());
	ImageDataMap.Add(Calibrations[1].CameraId, Image1.Data.GetData());

	if (Reconstructer->SetInputData(ImageDataMap))
	{
		int32 Width, Height;
		const float* Data = nullptr;
		const float* Intrinsics = nullptr;
		const float* Extrinsics = nullptr;
		if (Reconstructer->GetDepthMap(0, Width, Height, Data, Intrinsics, Extrinsics))
		{
			Calibrations[1].ImageSize.X = Width;
			Calibrations[1].ImageSize.Y = Height;
			Calibrations[1].FocalLength = FVector2D(Intrinsics[0], Intrinsics[4]);
			Calibrations[1].PrincipalPoint = FVector2D(Intrinsics[6], Intrinsics[7]);

			for (int32 I = 0; I < 4; ++I)
			{
				for (int32 J = 0; J < 4; ++J)
				{
					Calibrations[1].Transform.M[I][J] = Extrinsics[I * 4 + J];
				}
			}

			// depth map (rectified) camera has no distortion
			Calibrations[1].P1 = 0.0;
			Calibrations[1].P2 = 0.0;
			Calibrations[1].K1 = 0.0;
			Calibrations[1].K2 = 0.0;
			Calibrations[1].K3 = 0.0;

			FDepthDataType Output;

			Output.Width = Width;
			Output.Height = Height;

			int32 Size = Output.Width * Output.Height;
			Output.Data.SetNumUninitialized(Size);

			float* OutputPointer = Output.Data.GetData();
			for (int32 Index = 0; Index < Size; ++Index, ++OutputPointer, Data += 4)
			{
				*OutputPointer = *Data;
			}

			InPipelineData->SetData<FDepthDataType>(Pins[2], MoveTemp(Output));
		}
		else
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToGenerateDepth);
			InPipelineData->SetErrorNodeMessage("Failed to generate depth");
			return false;
		}
	}
	else
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToGenerateDepth);
		InPipelineData->SetErrorNodeMessage("Failed to generate depth");
		return false;
	}

	return true;
}

bool FDepthGenerateNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Reconstructer.Reset();
	return true;
}



FFlowNode::FFlowNode(const FString& InName) : FNode("Flow", InName)
{
	Pins.Add(FPin("UE Image In", EPinDirection::Input, EPinType::UE_Image));
	Pins.Add(FPin("Flow Out", EPinDirection::Output, EPinType::FlowOutput));
}

bool FFlowNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	FString UseGPU = InPipelineData->GetUseGPU();

	if (UE::MetaHuman::Pipeline::CVarBalancedGPUSelection.GetValueOnAnyThread())
	{
		// Balanced GPU selection will do the following:
		// For this optical flow node we'll choose non-UE GPU and
		// for all other nodes we'll let titan decide what GPU to use.
		FString UEGPU;
		TArray<FString> AllGPUs;

		FPipeline::GetPhysicalDeviceLUIDs(UEGPU, AllGPUs);

		bool bFoundGPU = false;

		// Find GPU not used by UE
		for (const FString& GPU : AllGPUs)
		{
			if (GPU != UEGPU)
			{
				bFoundGPU = true;
				UseGPU = GPU;
				break;
			}
		}

		if (bFoundGPU)
		{
			UE_LOG(LogMetaHumanPipeline, Display, TEXT("Flow node is using GPU '%s'"), *UseGPU);
		}
		else
		{
			UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to find GPU not used by UE, falling back to default behavior (GPU='%s')"), *UseGPU);
		}
	}

	if (IModularFeatures::Get().IsModularFeatureAvailable(IFaceTrackerNodeImplFactory::GetModularFeatureName()))
	{
		IFaceTrackerNodeImplFactory& OpticalFlowImplFactory = IModularFeatures::Get().GetModularFeature<IFaceTrackerNodeImplFactory>(IFaceTrackerNodeImplFactory::GetModularFeatureName());
		Flow = OpticalFlowImplFactory.CreateOpticalFlowImplementor();
	}

	if (!Flow.IsValid() || !Flow->Init(SolverConfigData, UseGPU))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage("Failed to initialize");
		return false;
	}

	Flow->SetCameras(Calibrations);
	
	return true;
}

bool FFlowNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	if (bEnableFlow)
	{
		FUEImageDataType UEImage = InPipelineData->GetData<FUEImageDataType>(Pins[0]);
		FFlowOutputDataType Output;

		TArray<float> Image;
		bool bConverted = Flow->ConvertImageWrapper(UEImage.Data, UEImage.Width, UEImage.Height, /*bIssRGB*/ true, Image);
		if (!bConverted)
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToGenerateFlow);
			InPipelineData->SetErrorNodeMessage("Failed to convert image data for flow node");
			return false;
		}

		if (!Image.IsEmpty() && !PreviousImage.IsEmpty())
		{
			if (!Flow->CalculateFlow(Camera, bUseConfidence, PreviousImage, Image, Output.Flow, Output.Confidence, Output.SourceCamera, Output.TargetCamera))
			{
				InPipelineData->SetErrorNodeCode(ErrorCode::FailedToGenerateFlow);
				InPipelineData->SetErrorNodeMessage("Failed to generate flow");
				return false;
			}
		}

		PreviousImage = MoveTemp(Image);
		InPipelineData->SetData<FFlowOutputDataType>(Pins[1], MoveTemp(Output));
	}

	return true;
}

bool FFlowNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Flow.Reset();
	PreviousImage.Reset();

	return true;
}

}
