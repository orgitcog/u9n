// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/HyprsenseNodeBase.h"
#include "MetaHumanTrace.h"

#include "RenderGraphBuilder.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Async/ParallelFor.h"

#define LOCTEXT_NAMESPACE "MetaHuman"

DEFINE_LOG_CATEGORY_STATIC(LogHyprsenseNodeBase, Verbose, All);

namespace UE::MetaHuman::Pipeline
{
	FHyprsenseNodeBase::FHyprsenseNodeBase(const FString& InTypeName, const FString& InName) : FNode(InTypeName, InName)
	{
	}

	void FHyprsenseNodeBase::AddContourToOutput(const TArray<float>& InPoints, const TArray<float>& InConfidences, const TMap<FString, Interval>& InCurveMap, const TMap<FString, int32>& InLandmarkMap, FFrameTrackingContourData& OutResult)
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseNodeBase::AddContourToOutput);

		if (!InPoints.IsEmpty())
		{
			for (const TPair<FString, Interval>& Curve : InCurveMap)
			{
				int32 Size = FMath::Abs(Curve.Value.End - Curve.Value.Start) + 1 + Curve.Value.AdditionalIndices.Num();
				FTrackingContour Contour;
				Contour.DensePoints.SetNum(Size);
				Contour.DensePointsConfidence.SetNum(Size);

				int32 Step = Curve.Value.End > Curve.Value.Start ? 1 : -1;

				int32 K{};
				for (int32 I = Curve.Value.Start; Step > 0 ? I <= Curve.Value.End : I >= Curve.Value.End; I += Step, ++K)
				{
					Contour.DensePoints[K].X = InPoints[2 * I];
					Contour.DensePoints[K].Y = InPoints[2 * I + 1];
					Contour.DensePointsConfidence[K] = InConfidences[I];
				}
				for (int32 I = 0; I < Curve.Value.AdditionalIndices.Num(); ++I, ++K)
				{
					const int32 Index = Curve.Value.AdditionalIndices[I];
					Contour.DensePoints[K].X = InPoints[2 * Index];
					Contour.DensePoints[K].Y = InPoints[2 * Index + 1];
					Contour.DensePointsConfidence[K] = InConfidences[Index];
				}
				OutResult.TrackingContours.Add(Curve.Key, Contour);
			}

			for (const TPair<FString, int32>& Landmark : InLandmarkMap)
			{
				FTrackingContour Contour;
				Contour.DensePoints.SetNum(1);
				Contour.DensePointsConfidence.SetNum(1);
				Contour.DensePoints[0].X = InPoints[2 * Landmark.Value];
				Contour.DensePoints[0].Y = InPoints[2 * Landmark.Value + 1];
				Contour.DensePointsConfidence[0] = InConfidences[Landmark.Value];

				OutResult.TrackingContours.Add(Landmark.Key, Contour);
			}
		}
	}

	bool FHyprsenseNodeBase::CheckTrackers(const TMap<ETrackerType, UE::NNE::FTensorShape>& InputValidationMap, const TMap<ETrackerType, TArray<UE::NNE::FTensorShape>>& OutputValidationMap, const TMap<TSharedPtr<UE::NNE::IModelInstanceGPU>, ETrackerType>& TrackerTypeMap)
	{
		// TODO not that this function may be costly to call. It is currently only called in SetTrackers() but we may want to re-organize the code slightly if it is to
		// be called separately
		using namespace UE::NNE;
		
		// first check if any of the trackers are custom trackers in input size and check validity
		const TMap<ETrackerType, TArray<FacePart>> InputSizeMap = { {ETrackerType::EyebrowTracker, {RightEyeBrow,LeftEyeBrow}},
													{ETrackerType::EyeTracker, {RightEye,LeftEye}},
													{ETrackerType::LipsTracker, {Lips}},
													{ETrackerType::LipzipTracker, {Lipzip}},
													{ETrackerType::NasoLabialTracker, {NasolabialNose}},
													{ETrackerType::ChinTracker, {Chin}},
													{ETrackerType::TeethTracker, {Teeth}},
													{ETrackerType::TeethConfidenceTracker, {TeethConfidence}} };

		// inputs may be different if a custom tracker has been trained, but outputs remain the same
		TMap<ETrackerType, UE::NNE::FTensorShape> NewInputValidationMap;

		for (auto Tuple : TrackerTypeMap)
		{
			TSharedPtr<IModelInstanceGPU>& Model = Tuple.Key;
			FTensorShape ModelInputInfo = InputValidationMap[Tuple.Value];
			FString TrackerName = TrackerNames[Tuple.Value];

			if (!Model.IsValid())
			{
				EErrorCode = ErrorCode::InvalidTracker;
				ErrorMessage = TrackerName + TEXT(" is invalid.");
				return false;
			}

			TConstArrayView<FTensorDesc> InputTensorDesc = Model->GetInputTensorDescs();
			if (InputTensorDesc.Num() == 1)
			{
				FSymbolicTensorShape TensorShape = InputTensorDesc[0].GetShape();
				TConstArrayView<int32> ShapeData = TensorShape.GetData();
				if (ShapeData.Num() == 4)
				{
					TConstArrayView<uint32> OrigShapeData = ModelInputInfo.GetData();
					bool bFlag = false;
					for (int32 I = 0; I < ShapeData.Num(); I++)
					{
						if (ShapeData[I] != OrigShapeData[I])
						{
							bFlag = true;
						}
					}
					if (bFlag)
					{
						// check that the input data size is the same for x and y
						if (ShapeData[2] != ShapeData[3])
						{
							EErrorCode = ErrorCode::InvalidTracker;
							ErrorMessage = TrackerName + TEXT(" tracker expects a non-square input image which is not allowed.");
							return false;
						}
						UE_LOG(LogHyprsenseNodeBase, Warning, TEXT("%s"), *FString::Printf(TEXT("Using custom tracker model of input resolution %d x %d for part: %s"), ShapeData[2], ShapeData[3], *TrackerName));
					}

					FTensorShape NewTensorShape = FTensorShape::Make({ (unsigned int)ShapeData[0],(unsigned int)ShapeData[1], (unsigned int)ShapeData[2], (unsigned int)ShapeData[3] });
					NewTensorShape.MakeFromSymbolic(TensorShape);
					NewInputValidationMap.Add(Tuple.Value, NewTensorShape);

					if (Tuple.Value == ETrackerType::FaceDetector)
					{
						DetectorInputSizeX = ShapeData[2];
						DetectorInputSizeY = ShapeData[3];
					}
					else if (Tuple.Value == ETrackerType::FaceTracker)
					{
						TrackerInputSizeX = ShapeData[2];
						TrackerInputSizeY = ShapeData[3];
					}
					else
					{
						for (int32 I : InputSizeMap[Tuple.Value])
						{
							TrackerPartInputSizeX[I] = ShapeData[2];
							TrackerPartInputSizeY[I] = ShapeData[3];
						}
					}
				}
				else
				{
					EErrorCode = ErrorCode::InvalidTracker;
					ErrorMessage = TrackerName + TEXT(" tracker expects a single input with shape data of length 4.");
					return false;
				}
			}
			else
			{
				EErrorCode = ErrorCode::InvalidTracker;
				ErrorMessage = TrackerName + TEXT(" tracker expects a single input.");
				return false;
			}

			// check the output tensor sizes
			TConstArrayView<FTensorDesc> OutputTensorDesc = Model->GetOutputTensorDescs();
			const TArray<FTensorShape>* Shapes = OutputValidationMap.Find(Tuple.Value);
			check(Shapes != nullptr);
			if (OutputTensorDesc.Num() == Shapes->Num())
			{
				for (int32 ShapeInd = 0; ShapeInd < Shapes->Num(); ++ShapeInd)
				{
					TConstArrayView<uint32> ShapeDataExpected = (*Shapes)[static_cast<uint32>(ShapeInd)].GetData();
					FSymbolicTensorShape TensorShape = OutputTensorDesc[ShapeInd].GetShape();
					TConstArrayView<int32> ShapeData = TensorShape.GetData();

					if (TensorShape.Rank() == 0) // we represent scalars by an empty array
					{
						if (!ShapeDataExpected.IsEmpty())
						{
							EErrorCode = ErrorCode::InvalidTracker;
							ErrorMessage = TrackerName + TEXT(" tracker number of outputs is incorrect");
							return false;
						}
					}
					else
					{

						if (ShapeData.Num() == ShapeDataExpected.Num())
						{
							for (int32 ShapeDataInd = 0; ShapeDataInd < ShapeData.Num(); ++ShapeDataInd)
							{
								if (static_cast<int32>(ShapeDataExpected[ShapeDataInd]) != ShapeData[ShapeDataInd])
								{
									EErrorCode = ErrorCode::InvalidTracker;
									ErrorMessage = TrackerName + TEXT(" tracker number of outputs is incorrect");
									return false;

								}
							}
						}
						else
						{
							EErrorCode = ErrorCode::InvalidTracker;
							ErrorMessage = TrackerName + TEXT(" tracker number of outputs is incorrect");
							return false;
						}
					}
				}
			}
			else
			{
				EErrorCode = ErrorCode::InvalidTracker;
				ErrorMessage = TrackerName + TEXT(" tracker number of outputs is incorrect");
				return false;
			}

		}


		bIsInitialized = false;
		for (auto Tuple : TrackerTypeMap)
		{
			FString TrackerName = TrackerNames[Tuple.Value];
			TSharedPtr<IModelInstanceGPU>& Model = Tuple.Key;
			FTensorShape ModelInfo = NewInputValidationMap[Tuple.Value];
			if (!Model.IsValid())
			{
				EErrorCode = ErrorCode::InvalidTracker;
				ErrorMessage = TrackerName + " is invalid";
				return false;
			}
			if (Model->SetInputTensorShapes({ ModelInfo }) != IModelInstanceGPU::ESetInputTensorShapesStatus::Ok)
			{
				EErrorCode = ErrorCode::InvalidTracker;
				ErrorMessage = TrackerName + " is invalid";
				return false;
			}
		}
		bIsInitialized = true;
		return true;
	}
	
	bool FHyprsenseNodeBase::ProcessLandmarks(const FUEImageDataType& Input, bool isRealtime, TArray<FHyprsenseNodeBase::PartPoints>& OutDenseTrackerPointsPerModelInversed, FHyprsenseNodeBase::PartPoints& OutSparseTrackerPointsInversed, bool bInRunSparseTrackerOnly)
	{
		using namespace UE::NNE;

		bool bIsFaceTracked = true;
		TArray<float> DetectorInputArray;
		const uint8* OrgImg = Input.Data.GetData();

		OutDenseTrackerPointsPerModelInversed.Empty();

		// if face is not detected in previous frame, we need to run face detector to find the face box 
		if (!bIsFaceDetected)
		{
			Bbox FullBox = { 0, 0, 1.f, 1.f };
			Matrix23f  iTransform = GetTransformFromBbox(FullBox, Input.Width, Input.Height, DetectorInputSizeX, 0.0f, false, PartType::FaceDetector);

			// resize image for detector input size		
			DetectorInputArray = WarpAffineBilinear(OrgImg, Input.Width, Input.Height, iTransform, DetectorInputSizeX, DetectorInputSizeY, true);

			// Output
			const int32 OutSize = 4212;
			TArray<float> Scores{}, Boxes{};

			if (FaceDetector.IsValid())
			{
				Scores.SetNumUninitialized(1 * OutSize * 2);
				Boxes.SetNumUninitialized(1 * OutSize * 4);
				TArray<FTensorBindingCPU> Inputs = { {(void*)DetectorInputArray.GetData(), DetectorInputArray.Num() * sizeof(float)} };
				TArray<FTensorBindingCPU> Outputs = { {(void*)Scores.GetData(), Scores.Num() * sizeof(float)}, {(void*)Boxes.GetData(), Boxes.Num() * sizeof(float)} };
				if (FaceDetector->RunSync(Inputs, Outputs) != IModelInstanceGPU::ERunSyncStatus::Ok)
				{
					EErrorCode = ErrorCode::FailedToTrack;
					ErrorMessage = TEXT("Failed to run Face Detector model");
					return false;
				}
			}
			else
			{
				EErrorCode = ErrorCode::InvalidTracker;
				ErrorMessage = TEXT("Face Detector model is invalid");
				return false;
			}

			const float IouThreshold = 0.45f;
			const float ProbThreshold = 0.3f;
			const int32 TopK = 20;

			// calculate the most accurate face by score
			TArray<Bbox> ResultBoxes = HardNMS(Scores, Boxes, IouThreshold, ProbThreshold, OutSize, TopK);
			if (ResultBoxes.IsEmpty())
			{
				bIsFaceTracked = false;
				bIsFaceDetected = false;
			}
			else
			{
				// face detected
				LastTransform = GetTransformFromBbox(ResultBoxes[0], Input.Width, Input.Height, TrackerInputSizeX, 0.0f, false, PartType::SparseTracker);
				bIsFaceDetected = true;
			}
		}
		
		TArray<float>& OutputArrayInversed = OutSparseTrackerPointsInversed.Points;
		TArray<PartPoints> OutputArrayPartInversed;
		OutputArrayInversed.Empty();
		OutputArrayPartInversed.SetNum(FacePart::Num);
		OutDenseTrackerPointsPerModelInversed.SetNum(FacePart::Num);

		// detector found a face already or face is still tracked from last frame
		if (bIsFaceTracked)
		{
			// resize original image to face tracker input size
			TArray<float> ResizedNNInput = WarpAffineBilinear(OrgImg, Input.Width, Input.Height, LastTransform, TrackerInputSizeX, TrackerInputSizeY, false);

			const int32 Landmarks = 131;
			TArray<float> OutputArrayCropped{}, Score{};

			if (FaceTracker.IsValid())
			{
				MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseNodeBase::FaceTracker_Run);

				OutputArrayCropped.SetNumUninitialized(1 * Landmarks * 2);
				Score.SetNumUninitialized(1);

				TArray<FTensorBindingCPU> Inputs = { {(void*)ResizedNNInput.GetData(), ResizedNNInput.Num() * sizeof(float)} };
				TArray<FTensorBindingCPU> Outputs = { {(void*)OutputArrayCropped.GetData(), OutputArrayCropped.Num() * sizeof(float)}, {(void*)Score.GetData(), Score.Num() * sizeof(float)} };
				if (FaceTracker->RunSync(Inputs, Outputs) != IModelInstanceGPU::ERunSyncStatus::Ok)
				{
					EErrorCode = ErrorCode::FailedToTrack;
					ErrorMessage = TEXT("Failed to run Face Tracker model");
					return false;
				}
			}
			else
			{
				EErrorCode = ErrorCode::InvalidTracker;
				ErrorMessage = TEXT("Face Tracker model is invalid");
				return false;
			}

			// if there's no output from face tracker or face tracking score is too low, it means face is lost 
			if (OutputArrayCropped.IsEmpty() || Score[0] < FaceScoreThreshold)
			{
				bIsFaceDetected = false;
			}
			else
			{
				// make 159 number of landmarks from 131 number of landmarks with interpolation
				OutputArrayCropped = GetLandmark131to159(OutputArrayCropped);

				// make inverse transform from cropped image(for face tracker input) coordinate to full/original image coordinate
				OutputArrayInversed = GetInversedPoints(OutputArrayCropped.GetData(), OutputArrayCropped.Num(), LastTransform);
				float Rotation = GetRotationToUpright(OutputArrayInversed);

				// save transform (to crop the face for next frame) based on current landmarks 
				LastTransform = GetTransformFromLandmarkPart(Input.Width, Input.Height, TrackerInputSizeX, OutputArrayInversed, Rotation, false, PartType::SparseTracker);

				if (!bInRunSparseTrackerOnly)
				{
					TArray<TArray<float>> ResizedNNInputPart;
					alignas(64) TArray<Matrix23f> TransformPart;

					ResizedNNInputPart.SetNum(FacePart::Num);
					TransformPart.SetNum(FacePart::Num);

					// check once if models are valid
					for (int32 I = 0; I < FacePart::Num; ++I)
					{
						if (ProcessPart[I])
						{
							if (NNEModels[I] != nullptr)
							{
								if (!NNEModels[I].IsValid())
								{
									EErrorCode = ErrorCode::InvalidTracker;
									ErrorMessage = TrackerNames[ETrackerType(I)] + TEXT(" is invalid");
									return false;
								}
							}
						}
					}

					auto ProcessParts = [&](const int32 InModelIndex, const TArray<int32>& InModels) -> bool
					{
						ParallelFor(InModels.Num(), [&](int32 InModelArrayIndex)
							{
								const int32 ModelIndex = InModels[InModelArrayIndex];
								const int32 InputX = TrackerPartInputSizeX[ModelIndex];
								const int32 InputY = TrackerPartInputSizeY[ModelIndex];

								// get bounding box from landmarks and get transform out of the bounding box with flip(for right eye+iris, eyebrow to make it "left-looking"					
								TransformPart[ModelIndex] = GetTransformFromLandmarkFacePart(Input.Width, Input.Height, InputX, OutputArrayInversed, static_cast<FHyprsenseNodeBase::FacePart>(ModelIndex), Rotation, ImageFlipPart[ModelIndex], isRealtime);

								// crop image and prepare input to inject to each partwise tracker
								ResizedNNInputPart[ModelIndex] = WarpAffineBilinear(OrgImg, Input.Width, Input.Height, TransformPart[ModelIndex], InputX, InputY, false);
							});

						int32 PointCount = 0;
						for (int32 ModelIndex : InModels)
						{
							PointCount += ResizedNNInputPart[ModelIndex].Num();
						}

						TArray<float> NNInput;
						NNInput.Reserve(PointCount);

						for (int32 ModelIndex : InModels)
						{
							NNInput += ResizedNNInputPart[ModelIndex];
						}

						TArray<FTensorBindingCPU> Inputs = { {(void*)NNInput.GetData(), NNInput.Num() * sizeof(float)} };

						TConstArrayView<FTensorDesc> OutputDescs = NNEModels[InModelIndex]->GetOutputTensorDescs();
						TArray<TArray<float>> OutputArrays;
						TArray<FTensorBindingCPU> Outputs;

						OutputArrays.SetNum(OutputDescs.Num());
						Outputs.SetNum(OutputDescs.Num());
						for (int i = 0; i < OutputDescs.Num(); i++)
						{
							int Volume = FTensorShape::MakeFromSymbolic(OutputDescs[i].GetShape()).Volume();
							OutputArrays[i].SetNumUninitialized(Volume > 0 ? Volume : 1);
							Outputs[i] = { (void*)OutputArrays[i].GetData(), OutputArrays[i].Num() * sizeof(float) };
						}
						if (NNEModels[InModelIndex]->RunSync(Inputs, Outputs) != IModelInstanceGPU::ERunSyncStatus::Ok)
						{
							return false;
						}

						TArrayView<float> OutputArrayPart = OutputArrays[0];

						// size and offset will be different depends on the part (eye+iris, eyebrow != lips, Nasolabial)
						int32 Size = OutputArrayPart.Num();
						Size = (CombineDataPart[InModelIndex]) ? Size / 2 : Size;
						const int32 Offset = (CombineDataPart[InModelIndex]) ? Size : 0;

						// whether the output of the model is the points or scores
						if (isScore[InModelIndex])
						{
							OutputArrayPartInversed[InModelIndex].Points.SetNum(Size);
							FMemory::Memcpy(OutputArrayPartInversed[InModelIndex].Points.GetData(), OutputArrayPart.GetData() + Offset, Size * sizeof(float));
							OutDenseTrackerPointsPerModelInversed[InModelIndex].Points += OutputArrayPartInversed[InModelIndex].Points;
						}
						else
						{
							// for eye+iris or eyebrow part, output will be attached to hard-wire for refinement tracker (Right -> Left order)
							if (CombineDataPart[InModelIndex])
							{
								OutputArrayPartInversed[InModelIndex - 1].Points = GetInversedPoints(OutputArrayPart.GetData(), Size, TransformPart[InModelIndex - 1]);
								OutDenseTrackerPointsPerModelInversed[InModelIndex].Points = OutputArrayPartInversed[InModelIndex - 1].Points;
							}

							OutputArrayPartInversed[InModelIndex].Points = GetInversedPoints(OutputArrayPart.GetData() + Offset, Size, TransformPart[InModelIndex]);
							OutDenseTrackerPointsPerModelInversed[InModelIndex].Points += OutputArrayPartInversed[InModelIndex].Points;
						}

						return true;
					};

					TArray<bool> ProcessedSuccessfully;
					TArray<FString> ErrorMessages;
					ProcessedSuccessfully.SetNum(ProcessPart.Num());
					ErrorMessages.SetNum(ProcessPart.Num());

					ParallelFor(FacePart::Num, [&](int32 InModelIdx)
						{
							if (ProcessPart[InModelIdx])
							{
								ProcessedSuccessfully[InModelIdx] = true;
								if (NNEModels[InModelIdx] != nullptr) // discard threads so that modelIdx is only models not face parts
								{

									switch (InModelIdx) // basic omp parallel sections
									{
									case FacePart::RightEyeBrow: // we dont know which is the model, but we know theres only one for both
									case FacePart::LeftEyeBrow:
									{
										ProcessedSuccessfully[InModelIdx] = ProcessParts(InModelIdx, { FacePart::RightEyeBrow, FacePart::LeftEyeBrow });
										break;
									}
									case FacePart::RightEye: // we dont know which is the model, but we know theres only one for both
									case FacePart::LeftEye:
									{
										ProcessedSuccessfully[InModelIdx] = ProcessParts(InModelIdx, { FacePart::RightEye, FacePart::LeftEye });
										break;
									}
									default: // all other batchsize 1 models go here
									{
										ProcessedSuccessfully[InModelIdx] = ProcessParts(InModelIdx, { InModelIdx });
										break;
									}
									}
								}

								if (!ProcessedSuccessfully[InModelIdx])
								{
									const FString* Name = TrackerNames.Find(ETrackerType(InModelIdx));
									ErrorMessages[InModelIdx] = *Name + TEXT(" failed to track");
								}

							}
						});

					// check if any failure codes and set the error message(s) and return false
					ErrorMessage = FString{};
					bool bTrackingError = false;
					for (int32 ModelIndex : ProcessPart)
					{
						if (ProcessPart[ModelIndex] && !ProcessedSuccessfully[ModelIndex])
						{
							EErrorCode = ErrorCode::FailedToTrack;
							bTrackingError = true;
							if (ErrorMessage.IsEmpty())
							{
								ErrorMessage = ErrorMessages[ModelIndex];
							}
							else
							{
								ErrorMessage = ErrorMessage + "\n" + ErrorMessages[ModelIndex];
							}
						}
					}
					if (bTrackingError)
					{
						return false;
					}
				}
			}
		}
		return true;
	}

	FString FHyprsenseNodeBase::GetErrorMessage() const
	{
		return ErrorMessage;
	}

	FHyprsenseNodeBase::ErrorCode FHyprsenseNodeBase::GetErrorCode() const
	{
		return EErrorCode;
	}

	void FHyprsenseNodeBase::InitTransformLandmark131to159()
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseNodeBase::InitTransformLandmark131to159);

		Index131to159.SetNum(159);

		const TArray<int32> Target = { 18, 20, 22, 24, 27, 29, 31, 33, 35, 37, 39, 41, 44, 46, 48, 50, 71, 73, 75, 77, 79, 81, 83, 85, 87, 89, 91, 93 , -1 };
		int32 TargetIdx = 0;
		int32 LandmarkIdx = 0;
		int32 Idx = 0;

		while (Idx < 159)
		{
			if (Target[TargetIdx] == Idx)
			{
				Index131to159[Idx++] = InvalidMarker;
				Index131to159[Idx++] = LandmarkIdx;
				TargetIdx++;
			}
			else
			{
				Index131to159[Idx++] = LandmarkIdx;
			}
			LandmarkIdx++;
		}
	}

	TArray<float> FHyprsenseNodeBase::GetLandmark131to159(const TArray<float>& InLandmarks131)
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseNodeBase::GetLandmark131to159);

		const TArray<int32> Source = { 17, 18,  18, 19,  19, 20,  20, 21,  22, 23,  23, 24,  24, 25,  25, 17,
										26, 27,  27, 28,  28, 29,  29, 30,  31, 32,  32, 33,  33, 34,  34, 26,
										54, 55,  55, 56,  56, 57,  57, 58,  58, 59,  59, 54,
										60, 61,  61, 62,  62, 63,  63, 64,  64, 65,  65, 60 };
		int32 SourceIdx = 0;

		TArray<float> Landmarks159;
		Landmarks159.SetNum(159 * 2);

		for (int32 I = 0; I < 159; ++I)
		{
			if (Index131to159[I] == InvalidMarker)
			{
				Landmarks159[I * 2 + 0] = (InLandmarks131[Source[SourceIdx * 2] * 2 + 0] + InLandmarks131[Source[SourceIdx * 2 + 1] * 2 + 0]) / 2.0f;
				Landmarks159[I * 2 + 1] = (InLandmarks131[Source[SourceIdx * 2] * 2 + 1] + InLandmarks131[Source[SourceIdx * 2 + 1] * 2 + 1]) / 2.0f;
				SourceIdx++;
			}
			else
			{
				Landmarks159[I * 2 + 0] = InLandmarks131[Index131to159[I] * 2 + 0];
				Landmarks159[I * 2 + 1] = InLandmarks131[Index131to159[I] * 2 + 1];
			}
		}
		return Landmarks159;
	}

	TArray<float> FHyprsenseNodeBase::SelectLandmarksToCrop(const TArray<float>& InLandmarks, const TArray<int32>& InLandmarkIndices, const TArray<int32>& LandmarkRangeIdxNormal, const TArray<int32>& InLandmarkIdxRangeExtra, const TArray<int32>& InLandmarkIdxCenter, const TArray<int32>& InLandmarkIdxCenterExtra, const TArray<int32>& InLandmarkIdxExtra)
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseNodeBase::SelectLandmarksToCrop);

		TArray<float> LandmarkSelections;

		for (int32 I = 0; I < InLandmarkIndices.Num(); ++I)
		{
			LandmarkSelections.Add(InLandmarks[InLandmarkIndices[I] * 2 + 0]);
			LandmarkSelections.Add(InLandmarks[InLandmarkIndices[I] * 2 + 1]);
		}

		if (LandmarkRangeIdxNormal.Num() == 2)
		{
			for (int32 I = LandmarkRangeIdxNormal[0]; I < LandmarkRangeIdxNormal[1]; ++I)
			{
				LandmarkSelections.Add(InLandmarks[I * 2 + 0]);
				LandmarkSelections.Add(InLandmarks[I * 2 + 1]);
			}
		}

		if (InLandmarkIdxRangeExtra.Num() == 3)
		{
			for (int32 I = InLandmarkIdxRangeExtra[0]; I < InLandmarkIdxRangeExtra[1]; ++I)
			{
				LandmarkSelections.Add(3 * InLandmarks[I * 2 + 0] - 2 * InLandmarks[(InLandmarkIdxRangeExtra[2] - I) * 2 + 0]);
				LandmarkSelections.Add(3 * InLandmarks[I * 2 + 1] - 2 * InLandmarks[(InLandmarkIdxRangeExtra[2] - I) * 2 + 1]);
			}
		}
		else if (InLandmarkIdxRangeExtra.Num() == 2)
		{
			LandmarkSelections.Add(3 * InLandmarks[InLandmarkIdxRangeExtra[0] * 2 + 0] - 2 * InLandmarks[InLandmarkIdxRangeExtra[1] * 2 + 0]);
			LandmarkSelections.Add(3 * InLandmarks[InLandmarkIdxRangeExtra[0] * 2 + 1] - 2 * InLandmarks[InLandmarkIdxRangeExtra[1] * 2 + 1]);

		}

		if (InLandmarkIdxCenter.Num() == 4)
		{
			LandmarkSelections.Add(0.5f * (InLandmarks[InLandmarkIdxCenter[0] * 2 + 0] + InLandmarks[InLandmarkIdxCenter[1] * 2 + 0]));
			LandmarkSelections.Add(0.5f * (InLandmarks[InLandmarkIdxCenter[0] * 2 + 1] + InLandmarks[InLandmarkIdxCenter[1] * 2 + 1]));
			LandmarkSelections.Add(2.0f * (InLandmarks[InLandmarkIdxCenter[2] * 2 + 0]) - InLandmarks[InLandmarkIdxCenter[3] * 2 + 0]);
			LandmarkSelections.Add(2.0f * (InLandmarks[InLandmarkIdxCenter[2] * 2 + 1]) - InLandmarks[InLandmarkIdxCenter[3] * 2 + 1]);
		}

		if (InLandmarkIdxCenterExtra.Num() > 0)
		{
			for (int32 I = 0; I < InLandmarkIdxCenterExtra.Num(); I += 2)
			{
				LandmarkSelections.Add(2 * InLandmarks[InLandmarkIdxCenterExtra[I] * 2 + 0] - InLandmarks[InLandmarkIdxCenterExtra[I + 1] * 2 + 0]);
				LandmarkSelections.Add(2 * InLandmarks[InLandmarkIdxCenterExtra[I] * 2 + 1] - InLandmarks[InLandmarkIdxCenterExtra[I + 1] * 2 + 1]);
			}
		}

		if (InLandmarkIdxExtra.Num() == 3)
		{
			LandmarkSelections.Add(2.0f * InLandmarks[InLandmarkIdxExtra[0] * 2 + 0] - 0.5f * (InLandmarks[InLandmarkIdxExtra[1] * 2 + 0] + InLandmarks[InLandmarkIdxExtra[2] * 2 + 0]));
			LandmarkSelections.Add(2.0f * InLandmarks[InLandmarkIdxExtra[0] * 2 + 1] - 0.5f * (InLandmarks[InLandmarkIdxExtra[1] * 2 + 1] + InLandmarks[InLandmarkIdxExtra[2] * 2 + 1]));
		}

		return LandmarkSelections;
	}

	FHyprsenseNodeBase::Matrix23f FHyprsenseNodeBase::GetTransformFromLandmarkFacePart(int32 InImageWidth, int32 InImageHeight, int32 InCropBoxSize, const TArray<float>& InLandmarks, FacePart InPartName, float InRotation, bool bFlip, bool isRealtime)
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseNodeBase::GetTransformFromLandmarkFacePart);

		TArray<int32> LandmarkIndices = {};
		TArray<int32> LandmarkIdxRangeNormal = {};
		TArray<int32> LandmarkIdxRangeExtra = {};
		TArray<int32> LandmarkIdxCenter = {};
		TArray<int32> LandmarkIdxCenterExtra = {};
		TArray<int32> LandmarkIdxExtra = {};

		TArray<float> LandmarkSelections = {};

		if (InPartName == FacePart::LipsNasoNoseTeeth)
		{
			LandmarkIndices = { 12, 53 };
			LandmarkIdxRangeNormal = { 4, 13 };
			LandmarkIdxCenterExtra = { 154, 94, 158, 106 };
		}
		else if (InPartName == FacePart::LeftEyeBrow)
		{
			if (isRealtime)
			{
				LandmarkIndices = { 51, 52 };
				LandmarkIdxRangeExtra = { 35, 43, 85 };
				LandmarkIdxCenter = { 34, 16, 34, 82 };
			}
			else
			{
				LandmarkIdxCenterExtra = { 21, 141, 38, 150, 0, 70, 16, 82, 8, 112 };
				LandmarkIdxRangeExtra = { 62, 51 };
			}
		}
		else if (InPartName == FacePart::RightEyeBrow)
		{
			if (isRealtime)
			{
				LandmarkIndices = { 51, 52 };
				LandmarkIdxRangeExtra = { 18, 26, 51 };
				LandmarkIdxCenter = { 17, 0, 17, 70 };
			}
			else
			{
				LandmarkIdxCenterExtra = { 21, 141, 38, 150, 0, 70, 16, 82, 8, 112 };
				LandmarkIdxRangeExtra = { 62, 51 };
			}
		}
		else if (InPartName == FacePart::Chin)
		{
			LandmarkIdxCenterExtra = { 21, 141, 38, 150, 0, 70, 16, 82, 8, 112 };
			LandmarkIdxRangeExtra = { 62, 51 };
		}
		else if (InPartName == FacePart::Teeth)
		{
			LandmarkIndices = { 154, 62, 158, 8 };
		}
		else if (InPartName == FacePart::Lips || InPartName == FacePart::Lipzip)
		{
			LandmarkIndices = { 154, 62, 158, 8 };

			LandmarkIdxCenterExtra = { 154, 94, 158, 106 };
			LandmarkIdxExtra = { 112, 94, 106 };
		}
		else if (InPartName == FacePart::TeethConfidence)
		{
			LandmarkIndices = { 98, 100, 102, 110, 112, 114 };
		}
		else if (InPartName == FacePart::NasolabialNose)
		{
			LandmarkIndices = { 4, 8, 12, 53, 57, 67 };
		}
		else if (InPartName == FacePart::LeftEye)
		{
			LandmarkIndices = { 51, 52 };
			LandmarkIdxRangeNormal = { 34, 43 };
		}
		else if (InPartName == FacePart::RightEye)
		{
			LandmarkIndices = { 51, 52 };
			LandmarkIdxRangeNormal = { 17, 26 };
		}

		LandmarkSelections = SelectLandmarksToCrop(InLandmarks, LandmarkIndices, LandmarkIdxRangeNormal, LandmarkIdxRangeExtra, LandmarkIdxCenter, LandmarkIdxCenterExtra, LandmarkIdxExtra);

		return GetTransformFromLandmarkPart(InImageWidth, InImageHeight, InCropBoxSize, LandmarkSelections, InRotation, bFlip, PartType::PartwiseTracker);
	}

	float FHyprsenseNodeBase::GetRotationToUpright(const TArray<float>& InLandmarks)
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseNodeBase::GetRotationToUpright);

		const int32 REyesIdx = 70;
		const int32 LEyesIdx = 82;

		const int32 X1 = InLandmarks[REyesIdx * 2];
		const int32 Y1 = InLandmarks[REyesIdx * 2 + 1];
		const int32 X2 = InLandmarks[LEyesIdx * 2];
		const int32 Y2 = InLandmarks[LEyesIdx * 2 + 1];

		return UKismetMathLibrary::Atan2(Y2 - Y1, X2 - X1);
	}

	FHyprsenseNodeBase::Matrix23f FHyprsenseNodeBase::GetTransformFromLandmarkPart(int32 InImageWidth, int32 InImageHeight, int32 InCropBoxSize, const TArray<float>& InLandmarks, float InRotation, bool bFlip, PartType InPartType)
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseNodeBase::GetTransformFromLandmarkPart);

		float MinX = InImageWidth, MinY = InImageHeight;
		float MaxX = 0, MaxY = 0;

		FCriticalSection Mutex;
		const int32 LandmarkSize = InLandmarks.Num() / 2;
		ParallelFor(LandmarkSize, [&](int32 I)
		{
			Matrix33f TransformSrcToDst;
			Eigen::Vector3f landmarkMatrix, RotatedLandmark;

			landmarkMatrix << InLandmarks[I * 2 + 0], InLandmarks[I * 2 + 1], 1;
			TransformSrcToDst << std::cos(InRotation), std::sin(InRotation), 0, -std::sin(InRotation), std::cos(InRotation), 0, 0, 0, 1;
			RotatedLandmark = TransformSrcToDst * landmarkMatrix;

			Mutex.Lock();
			{
				MinX = (MinX > RotatedLandmark[0]) ? RotatedLandmark[0] : MinX;
				MaxX = (MaxX < RotatedLandmark[0]) ? RotatedLandmark[0] : MaxX;
				MinY = (MinY > RotatedLandmark[1]) ? RotatedLandmark[1] : MinY;
				MaxY = (MaxY < RotatedLandmark[1]) ? RotatedLandmark[1] : MaxY;
			}
			Mutex.Unlock();
		});

		Bbox LandmarkBox;
		LandmarkBox.X1 = MinX / InImageWidth;
		LandmarkBox.X2 = MaxX / InImageWidth;
		LandmarkBox.Y1 = MinY / InImageHeight;
		LandmarkBox.Y2 = MaxY / InImageHeight;

		return GetTransformFromBbox(LandmarkBox, InImageWidth, InImageHeight, InCropBoxSize, InRotation, bFlip, InPartType);
	}


	FHyprsenseNodeBase::Bbox FHyprsenseNodeBase::GetInversedBbox(Bbox& InBbox, const Matrix23f& InTransform)
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseNodeBase::GetInversedBbox);

		Eigen::Affine2f Affine(InTransform);

		Eigen::Vector2f CroppedBoxPt1 = Eigen::Vector2f(InBbox.X1, InBbox.Y1);
		Eigen::Vector2f TransformedBoxPt1 = Affine * CroppedBoxPt1;
		InBbox.X1 = TransformedBoxPt1.x();
		InBbox.Y1 = TransformedBoxPt1.y();

		Eigen::Vector2f  CroppedBoxPt2 = Eigen::Vector2f(InBbox.X2, InBbox.Y2);
		Eigen::Vector2f TransformedBoxPt2 = Affine * CroppedBoxPt2;
		InBbox.X2 = TransformedBoxPt2.x();
		InBbox.Y2 = TransformedBoxPt2.y();

		return InBbox;
	}

	TArray<float> FHyprsenseNodeBase::GetInversedPoints(float* InLandmarks, int32 InNum, const Matrix23f& InTransform)
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseNodeBase::GetInversedPoints);

		TArray<float> InversedLandmarks;
		InversedLandmarks.SetNum(InNum);
		Eigen::Affine2f Affine(InTransform);

		const int32 NumPoints = InNum / 2;
		ParallelFor(NumPoints, [&](int32 I)
		{
			Eigen::Vector2f CroppedImgPt = Eigen::Vector2f(InLandmarks[I * 2 + 0], InLandmarks[I * 2 + 1]);
			Eigen::Vector2f OrigImgPt = Affine * CroppedImgPt;
			InversedLandmarks[I * 2 + 0] = OrigImgPt.x();
			InversedLandmarks[I * 2 + 1] = OrigImgPt.y();
		});

		return InversedLandmarks;
	}

}

#undef LOCTEXT_NAMESPACE
