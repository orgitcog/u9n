// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/HyprsenseSparseNode.h"
#include "MetaHumanTrace.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "MetaHuman"

namespace UE::MetaHuman::Pipeline
{
	FHyprsenseSparseNode::FHyprsenseSparseNode(const FString& InName) : FHyprsenseNodeBase("HyprsenseSparse", InName)
	{
		Pins.Add(FPin("UE Image In", EPinDirection::Input, EPinType::UE_Image));
		Pins.Add(FPin("Contours Out", EPinDirection::Output, EPinType::Contours));
		TrackerPartInputSizeX = { 256, 256, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		TrackerPartInputSizeY = { 256, 256, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		ProcessPart = { true, true, false, false, false, false, false, false, false, false, false };
	}

	bool FHyprsenseSparseNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
	{
		if (!bIsInitialized)
		{
			EErrorCode = ErrorCode::InvalidTracker;
			ErrorMessage = "Not initialized.";
			InPipelineData->SetErrorNodeCode(EErrorCode);
			InPipelineData->SetErrorNodeMessage(ErrorMessage);
			return false;
		}

		InitTransformLandmark131to159();

		bIsFaceDetected = false;
		ErrorMessage = "";
		LastTransform << 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f;

		return true;
	}

	bool FHyprsenseSparseNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseSparseNode::Process);

		if (!bIsInitialized)
		{
			EErrorCode = ErrorCode::InvalidTracker;
			ErrorMessage = "Not initialized.";
			InPipelineData->SetErrorNodeCode(EErrorCode);
			InPipelineData->SetErrorNodeMessage(ErrorMessage);
			return false;
		}

		const FUEImageDataType& Input = InPipelineData->GetData<FUEImageDataType>(Pins[0]);
		PartPoints SparseTrackerPointsInversed;
		TArray<PartPoints> OutputArrayPerModelInversed;
		bool bProcessedSuccessfully = ProcessLandmarks(Input, false, OutputArrayPerModelInversed, SparseTrackerPointsInversed, true);


		if (!bProcessedSuccessfully)
		{
			InPipelineData->SetErrorNodeCode(EErrorCode);
			InPipelineData->SetErrorNodeMessage(ErrorMessage);
			return false;
		}
		else
		{
			FFrameTrackingContourData Output;

			//Sparse Landmarks
			AddContourToOutput(SparseTrackerPointsInversed.Points, EmptyConfidences(SparseTrackerPointsInversed.Points.Num()),
				CurveSparseTrackerMap, LandmarkSparseTrackerMap, Output);

			InPipelineData->SetData<FFrameTrackingContourData>(Pins[1], MoveTemp(Output));
			return true;
		}
	}
	
	bool FHyprsenseSparseNode::SetTrackers(	const TSharedPtr<UE::NNE::IModelInstanceGPU>& InFaceTracker,
										const TSharedPtr<UE::NNE::IModelInstanceGPU>& InFaceDetector)
	{
		using namespace UE::NNE;

		FaceTracker = InFaceTracker;
		FaceDetector = InFaceDetector;

		const TMap<TSharedPtr<IModelInstanceGPU>, ETrackerType> TrackerTypeMap = { {FaceTracker, ETrackerType::FaceTracker},
			{FaceDetector, ETrackerType::FaceDetector} };

		const TMap<ETrackerType, UE::NNE::FTensorShape> InputValidationMap = { { ETrackerType::FaceDetector, FTensorShape::Make({1,3, (unsigned int)DetectorInputSizeY, (unsigned int)DetectorInputSizeX})},
																 {ETrackerType::FaceTracker, FTensorShape::Make({1,3, (unsigned int)TrackerInputSizeY, (unsigned int)TrackerInputSizeX})}};

		const TMap<ETrackerType, TArray<UE::NNE::FTensorShape>> OutputValidationMap = { { ETrackerType::FaceDetector, { FTensorShape::Make({1, 4212, 2}), FTensorShape::Make({1, 4212, 4})} },
																 {ETrackerType::FaceTracker, { FTensorShape::Make({1, 131, 2}), FTensorShape::Make({1, 1})} } };

		return CheckTrackers(InputValidationMap, OutputValidationMap, TrackerTypeMap);
	}

	FHyprsenseSparseManagedNode::FHyprsenseSparseManagedNode(const FString& InName) : FHyprsenseSparseNode(InName)
	{
		using namespace UE::NNE;

		UNNEModelData* FaceTrackerModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/" UE_PLUGIN_NAME "/GenericTracker/FaceTracker.FaceTracker"));
		UNNEModelData* FaceDetectorModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/MetaHumanCoreTech/GenericTracker/FaceDetector.FaceDetector"));

		TWeakInterfacePtr<INNERuntimeGPU> Runtime = GetRuntime<INNERuntimeGPU>("NNERuntimeORTDml");
		if (!Runtime.IsValid())
		{
			return;
		}
		TSharedPtr<IModelInstanceGPU> FaceTrackerModel(Runtime->CreateModelGPU(FaceTrackerModelData)->CreateModelInstanceGPU());
		TSharedPtr<IModelInstanceGPU> FaceDetectorModel(Runtime->CreateModelGPU(FaceDetectorModelData)->CreateModelInstanceGPU());
		verify(SetTrackers(FaceTrackerModel, FaceDetectorModel));
	}
}
#undef LOCTEXT_NAMESPACE
