// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/HyprsenseNode.h"
#include "MetaHumanTrace.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "MetaHuman"

namespace UE::MetaHuman::Pipeline
{
	FHyprsenseNode::FHyprsenseNode(const FString& InName) : FHyprsenseNodeBase("Hyprsense", InName)
	{
		Pins.Add(FPin("UE Image In", EPinDirection::Input, EPinType::UE_Image));
		Pins.Add(FPin("Contours Out", EPinDirection::Output, EPinType::Contours));
		ProcessPart = { true, true ,true, true, true, true, true, true, true, false, true };
		TrackerPartInputSizeX = { 256, 256, 512, 512, 512, 256, 256, 512, 256, 0, 256 };
		TrackerPartInputSizeY = { 256, 256, 512, 512, 512, 256, 256, 512, 256, 0, 256 };
	}

	bool FHyprsenseNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
	{
		if (!bIsInitialized)
		{
			EErrorCode = ErrorCode::InvalidTracker;
			ErrorMessage = "Not initialized.";
			InPipelineData->SetErrorNodeCode(EErrorCode);
			InPipelineData->SetErrorNodeMessage(ErrorMessage);
			return false;
		}

		NNEModels[1] = EyebrowTracker;
		NNEModels[3] = EyeTracker;
		NNEModels[4] = LipsTracker;
		NNEModels[5] = LipzipTracker;
		NNEModels[6] = NasolabialNoseTracker;
		NNEModels[7] = ChinTracker;
		NNEModels[8] = TeethTracker;
		NNEModels[10] = TeethConfidenceTracker;

		InitTransformLandmark131to159();

		bIsFaceDetected = false;
		ErrorMessage = "";
		LastTransform << 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f;

		return true;
	}

	bool FHyprsenseNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseNode::Process);

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
		bool bProcessedSuccessfully = ProcessLandmarks(Input, false, OutputArrayPerModelInversed, SparseTrackerPointsInversed, false);

		if (!bProcessedSuccessfully)
		{
			InPipelineData->SetErrorNodeCode(EErrorCode);
			InPipelineData->SetErrorNodeMessage(ErrorMessage);
			return false;
		}
		else
		{
			FFrameTrackingContourData Output;

			// Sparse tracker results
			if (bAddSparseTrackerResultsToOutput)
			{
				AddContourToOutput(SparseTrackerPointsInversed.Points, EmptyConfidences(SparseTrackerPointsInversed.Points.Num()),
					CurveSparseTrackerMap, LandmarkSparseTrackerMap, Output);
			}

			//Brow
			AddContourToOutput(OutputArrayPerModelInversed[1].Points, EmptyConfidences(OutputArrayPerModelInversed[1].Points.Num()),
				CurveBrowMap, LandmarkBrowMap, Output);

			//Eye-Iris
			AddContourToOutput(OutputArrayPerModelInversed[3].Points, EmptyConfidences(OutputArrayPerModelInversed[3].Points.Num()),
				CurveEyeIrisMap, LandmarkEyeIrisMap, Output);

			//Lip
			AddContourToOutput(OutputArrayPerModelInversed[4].Points, EmptyConfidences(OutputArrayPerModelInversed[4].Points.Num()),
				CurveLipMap, LandmarkLipMap, Output);

			//LipZip
			AddContourToOutput(OutputArrayPerModelInversed[5].Points, EmptyConfidences(OutputArrayPerModelInversed[5].Points.Num()),
				CurveLipzipMap, LandmarkLipzipMap, Output);

			TArray<float> NasoOutArray, NoseOutArray;
			if (!OutputArrayPerModelInversed[6].Points.IsEmpty())
			{
				NasoOutArray.SetNum(100); //The number of nasolabial landmarks (x,y) 50 * 2 = 10
				NoseOutArray.SetNum(98); //The number of nose landmarks (x,y) 49 * 2 = 98

				//Since nose & nasolabial are combined in the tracker, you need to separate output result
				FMemory::Memcpy(NasoOutArray.GetData(), OutputArrayPerModelInversed[6].Points.GetData(), 100 * sizeof(float));
				FMemory::Memcpy(NoseOutArray.GetData(), OutputArrayPerModelInversed[6].Points.GetData() + 100, 98 * sizeof(float));
			}

			//Nasolab
			AddContourToOutput(NasoOutArray, EmptyConfidences(NasoOutArray.Num()), CurveNasolabMap, LandmarkNasolabMap, Output);

			//Nose
			AddContourToOutput(NoseOutArray, EmptyConfidences(NoseOutArray.Num()), CurveNoseMap, LandmarkNoseMap, Output);

			//Chin
			AddContourToOutput(OutputArrayPerModelInversed[7].Points, EmptyConfidences(OutputArrayPerModelInversed[7].Points.Num()), CurveChinMap, LandmarkChinMap, Output);

			//Teeth
			AddContourToOutput(OutputArrayPerModelInversed[8].Points, OutputArrayPerModelInversed[10].Points, CurveTeethMap, LandmarkTeethMap, Output);

			InPipelineData->SetData<FFrameTrackingContourData>(Pins[1], MoveTemp(Output));
			return true;
		}
	}

	bool FHyprsenseNode::SetTrackers(const TSharedPtr<UE::NNE::IModelInstanceGPU>& InFaceTracker,
									 const TSharedPtr<UE::NNE::IModelInstanceGPU>& InFaceDetector,
									 const TSharedPtr<UE::NNE::IModelInstanceGPU>& InEyebrowTracker,
									 const TSharedPtr<UE::NNE::IModelInstanceGPU>& InEyeTracker,
									 const TSharedPtr<UE::NNE::IModelInstanceGPU>& InLipsTracker,
									 const TSharedPtr<UE::NNE::IModelInstanceGPU>& InLipZipTracker,
									 const TSharedPtr<UE::NNE::IModelInstanceGPU>& InNasolabialNoseTracker,
									 const TSharedPtr<UE::NNE::IModelInstanceGPU>& InChinTracker,
									 const TSharedPtr<UE::NNE::IModelInstanceGPU>& InTeethTracker,
									 const TSharedPtr<UE::NNE::IModelInstanceGPU>& InTeethConfidenceTracker)
	{
		using namespace UE::NNE;

		FaceTracker = InFaceTracker;
		FaceDetector = InFaceDetector;
		EyebrowTracker = InEyebrowTracker;
		EyeTracker = InEyeTracker;
		LipsTracker = InLipsTracker;
		LipzipTracker = InLipZipTracker;
		NasolabialNoseTracker = InNasolabialNoseTracker;
		ChinTracker = InChinTracker;
		TeethTracker = InTeethTracker;
		TeethConfidenceTracker = InTeethConfidenceTracker;

		const TMap<TSharedPtr<IModelInstanceGPU>, ETrackerType> TrackerTypeMap = { {FaceTracker, ETrackerType::FaceTracker},
																						{FaceDetector, ETrackerType::FaceDetector},
																						{EyebrowTracker, ETrackerType::EyebrowTracker},
																						{EyeTracker, ETrackerType::EyeTracker},
																						{LipsTracker, ETrackerType::LipsTracker},
																						{LipzipTracker, ETrackerType::LipzipTracker},
																						{NasolabialNoseTracker, ETrackerType::NasoLabialTracker},
																						{ChinTracker, ETrackerType::ChinTracker},
																						{TeethTracker, ETrackerType::TeethTracker},
																						{TeethConfidenceTracker, ETrackerType::TeethConfidenceTracker},
		};

		const TMap<ETrackerType, UE::NNE::FTensorShape> InputValidationMap = { { ETrackerType::FaceDetector, FTensorShape::Make({1,3, (unsigned int)DetectorInputSizeY, (unsigned int)DetectorInputSizeX})},
																 {ETrackerType::FaceTracker, FTensorShape::Make({1,3, (unsigned int)TrackerInputSizeY, (unsigned int)TrackerInputSizeX})},
																 {ETrackerType::EyebrowTracker, FTensorShape::Make({2, 3, (unsigned int)TrackerInputSizeY, (unsigned int)TrackerInputSizeX })},
																 {ETrackerType::EyeTracker, FTensorShape::Make({2, 3, 512, 512})},
																 {ETrackerType::LipsTracker, FTensorShape::Make({1, 3, 512, 512})},
																 {ETrackerType::LipzipTracker,FTensorShape::Make({1,3, (unsigned int)TrackerInputSizeY, (unsigned int)TrackerInputSizeX})},
																 {ETrackerType::NasoLabialTracker, FTensorShape::Make({1,3, (unsigned int)TrackerInputSizeY, (unsigned int)TrackerInputSizeX})},
																 {ETrackerType::ChinTracker, FTensorShape::Make({1, 3, 512, 512})},
																 {ETrackerType::TeethTracker, FTensorShape::Make({1,3, (unsigned int)TrackerInputSizeY, (unsigned int)TrackerInputSizeX})},
																 {ETrackerType::TeethConfidenceTracker,FTensorShape::Make({1,3, (unsigned int)TrackerInputSizeY, (unsigned int)TrackerInputSizeX})} };

		// note that we represent expected scalars as empty tensor shapes
		const TMap<ETrackerType, TArray<UE::NNE::FTensorShape>> OutputValidationMap = { { ETrackerType::FaceDetector, { FTensorShape::Make({1, 4212, 2}), FTensorShape::Make({1, 4212, 4})} },
																 {ETrackerType::FaceTracker, { FTensorShape::Make({1, 131, 2}), FTensorShape::Make({1, 1})} },
																 {ETrackerType::EyebrowTracker, { FTensorShape::Make({2, 48, 2}), FTensorShape{} } },
																 {ETrackerType::EyeTracker, { FTensorShape::Make({2, 64, 2}), FTensorShape{} } },
																 {ETrackerType::LipsTracker, { FTensorShape::Make({1, 216, 2}), FTensorShape{} } },
																 {ETrackerType::LipzipTracker, { FTensorShape::Make({1, 2, 2}), FTensorShape{} } },
																 {ETrackerType::NasoLabialTracker, { FTensorShape::Make({1, 99, 2}), FTensorShape{} } },
																 {ETrackerType::ChinTracker, { FTensorShape::Make({1, 49, 2}), FTensorShape{} } },
																 {ETrackerType::TeethTracker, { FTensorShape::Make({1, 4, 2}), FTensorShape{} } },
																 {ETrackerType::TeethConfidenceTracker,{ FTensorShape::Make({1, 4}) } } };

		return CheckTrackers(InputValidationMap, OutputValidationMap, TrackerTypeMap);
	}

	FHyprsenseManagedNode::FHyprsenseManagedNode(const FString& InName) : FHyprsenseNode(InName)
	{
		using namespace UE::NNE;

		UNNEModelData* FaceTrackerModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/" UE_PLUGIN_NAME "/GenericTracker/FaceTracker.FaceTracker"));
		UNNEModelData* FaceDetectorModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/MetaHumanCoreTech/GenericTracker/FaceDetector.FaceDetector"));
		UNNEModelData* EyebrowTrackerModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/" UE_PLUGIN_NAME "/GenericTracker/LeftBrowWholeFace.LeftBrowWholeFace"));
		UNNEModelData* EyeTrackerModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/" UE_PLUGIN_NAME "/GenericTracker/LeftEye.LeftEye"));
		UNNEModelData* LipsTrackerModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/" UE_PLUGIN_NAME "/GenericTracker/Lips.Lips"));
		UNNEModelData* LipZipTrackerModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/" UE_PLUGIN_NAME "/GenericTracker/LipZip.LipZip"));
		UNNEModelData* NasolabialNoseTrackerModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/" UE_PLUGIN_NAME "/GenericTracker/NasolabialNose.NasolabialNose"));
		UNNEModelData* ChinTrackerModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/" UE_PLUGIN_NAME "/GenericTracker/Chin.Chin"));
		UNNEModelData* TeethTrackerModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/" UE_PLUGIN_NAME "/GenericTracker/Teeth.Teeth"));
		UNNEModelData* TeethConfidenceModelData = LoadObject<UNNEModelData>(GetTransientPackage(), *FString("/" UE_PLUGIN_NAME "/GenericTracker/TeethConfidence.TeethConfidence"));

		TWeakInterfacePtr<INNERuntimeGPU> Runtime = GetRuntime<INNERuntimeGPU>("NNERuntimeORTDml");
		if (!Runtime.IsValid())
		{
			return;
		}

		TSharedPtr<IModelInstanceGPU> FaceTrackerModel(Runtime->CreateModelGPU(FaceTrackerModelData)->CreateModelInstanceGPU());
		TSharedPtr<IModelInstanceGPU> FaceDetectorModel(Runtime->CreateModelGPU(FaceDetectorModelData)->CreateModelInstanceGPU());
		TSharedPtr<IModelInstanceGPU> EyebrowTrackerModel(Runtime->CreateModelGPU(EyebrowTrackerModelData)->CreateModelInstanceGPU());
		TSharedPtr<IModelInstanceGPU> EyeTrackerModel(Runtime->CreateModelGPU(EyeTrackerModelData)->CreateModelInstanceGPU());
		TSharedPtr<IModelInstanceGPU> LipsTrackerModel(Runtime->CreateModelGPU(LipsTrackerModelData)->CreateModelInstanceGPU());
		TSharedPtr<IModelInstanceGPU> LipZipTrackerModel(Runtime->CreateModelGPU(LipZipTrackerModelData)->CreateModelInstanceGPU());
		TSharedPtr<IModelInstanceGPU> NasolabialNoseTrackerModel(Runtime->CreateModelGPU(NasolabialNoseTrackerModelData)->CreateModelInstanceGPU());
		TSharedPtr<IModelInstanceGPU> ChinTrackerModel(Runtime->CreateModelGPU(ChinTrackerModelData)->CreateModelInstanceGPU());
		TSharedPtr<IModelInstanceGPU> TeethTrackerModel(Runtime->CreateModelGPU(TeethTrackerModelData)->CreateModelInstanceGPU());
		TSharedPtr<IModelInstanceGPU> TeethConfidenceModel(Runtime->CreateModelGPU(TeethConfidenceModelData)->CreateModelInstanceGPU());

		verify(SetTrackers(FaceTrackerModel, FaceDetectorModel, EyebrowTrackerModel, EyeTrackerModel, LipsTrackerModel, LipZipTrackerModel, NasolabialNoseTrackerModel, ChinTrackerModel, TeethTrackerModel, TeethConfidenceModel));
	}
}
#undef LOCTEXT_NAMESPACE
