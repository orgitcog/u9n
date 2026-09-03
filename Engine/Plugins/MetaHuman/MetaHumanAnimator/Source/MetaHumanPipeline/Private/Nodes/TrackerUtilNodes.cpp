// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/TrackerUtilNodes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Pipeline/PipelineData.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace UE::MetaHuman::Pipeline
{

FJsonTitanTrackerNode::FJsonTitanTrackerNode(const FString& InName) : FNode("JsonTitanTracker", InName)
{
	Pins.Add(FPin("UE Image In", EPinDirection::Input, EPinType::UE_Image)); // Does not really take an image, but this makes it a drop in replacement for other tracker nodes
	Pins.Add(FPin("Contours Out", EPinDirection::Output, EPinType::Contours));
}

bool FJsonTitanTrackerNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	bool bIsOK = false;

	Contours.Reset();

	// Robustness of parsing could be better! ie handling unexpected data in fields

	TArray<FString> Lines;
	if (FFileHelper::LoadANSITextFileToStrings(*JsonFile, nullptr, Lines))
	{
		if (Lines.Num() == 1)
		{
			FString JsonRaw;
			TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Lines[0]);

			TSharedPtr<FJsonObject> JsonParsed;
			if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
			{
				const TArray<TSharedPtr<FJsonValue>>* Frames = nullptr;
				if (JsonParsed->TryGetArrayField(TEXT("frames"), Frames))
				{
					bIsOK = true;

					for (TSharedPtr<FJsonValue> Frame : *Frames)
					{
						const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
						const TSharedPtr<FJsonObject>* Curves = nullptr;
						const TSharedPtr<FJsonObject>* Landmarks = nullptr;

						bIsOK &= (bIsOK && Frame->AsObject()->TryGetArrayField(TEXT("points"), Points));
						bIsOK &= (bIsOK && Frame->AsObject()->TryGetObjectField(TEXT("curves"), Curves));
						bIsOK &= (bIsOK && Frame->AsObject()->TryGetObjectField(TEXT("landmarks"), Landmarks));

						if (bIsOK)
						{
							FFrameTrackingContourData Contour;

							TArray<FVector2D> PointList;
							for (const TSharedPtr<FJsonValue>& Point : *Points)
							{
								const TArray<TSharedPtr<FJsonValue>>* XY = nullptr;
								if (Point->TryGetArray(XY) && XY->Num() == 2)
								{
									float X = (*XY)[0]->AsNumber();
									float Y = (*XY)[1]->AsNumber();
									PointList.Add(FVector2D(X, Y));
								}
							}

							bIsOK &= (PointList.Num() == Points->Num());
							if (!bIsOK)
							{
								break;
							}

							for (const auto& Curve : (*Curves)->Values)
							{
								Contour.TrackingContours.Add(Curve.Key);

								const TArray<TSharedPtr<FJsonValue>>* Indices = nullptr;
								if (Curve.Value->TryGetArray(Indices))
								{
									for (const TSharedPtr<FJsonValue>& Index : *Indices)
									{
										int32 IndexInt = Index->AsNumber();
										Contour.TrackingContours[Curve.Key].DensePoints.Add(PointList[IndexInt]);
									}
								}
							}

							for (const auto& Landmark : (*Landmarks)->Values)
							{
								Contour.TrackingContours.Add(Landmark.Key);

								const TArray<TSharedPtr<FJsonValue>>* Indices = nullptr;
								if (Landmark.Value->TryGetArray(Indices))
								{
									for (const TSharedPtr<FJsonValue>& Index : *Indices)
									{
										int32 IndexInt = Index->AsNumber();
										Contour.TrackingContours[Landmark.Key].DensePoints.Add(PointList[IndexInt]);
									}
								}
							}

							Contours.Add(MoveTemp(Contour));
						}
						else
						{
							break;
						}
					}
				}
			}
		}
	}
	else
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToLoadJsonFile);
		InPipelineData->SetErrorNodeMessage(FString::Printf(TEXT("Failed to load JSON file %s"), *JsonFile));
		return false;
	}

	if (!bIsOK)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::InvalidData);
		InPipelineData->SetErrorNodeMessage(FString::Printf(TEXT("Invalid data in JSON file %s"), *JsonFile));
		return false;
	}

	return bIsOK;
}

bool FJsonTitanTrackerNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	int32 Frame = InPipelineData->GetFrameNumber();

	if (Frame < Contours.Num())
	{
		InPipelineData->SetData<FFrameTrackingContourData>(Pins[1], Contours[Frame]);
		return true;
	}
	else
	{
		return false;
	}
}

bool FJsonTitanTrackerNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Contours.Reset();

	return true;
}

FJsonTrackerNode::FJsonTrackerNode(const FString& InName) : FNode("JsonTracker", InName)
{
	Pins.Add(FPin("UE Image In", EPinDirection::Input, EPinType::UE_Image)); // Does not really take an image, but this makes it a drop in replacement for other tracker nodes
	Pins.Add(FPin("Contours Out", EPinDirection::Output, EPinType::Contours));
}

bool FJsonTrackerNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	bool bIsOK = false;
	Contours.Reset();

	// There are no checks for non-existing fields as this was added for node testing purpose
	FString FileData;

	if (FFileHelper::LoadFileToString(FileData, *JsonFile))
	{
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(FileData);
		TSharedPtr<FJsonObject> JsonParsed;
		if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
		{
			const TSharedPtr<FJsonObject>* Frames = nullptr;

			if (JsonParsed->TryGetObjectField(TEXT("Frames"), Frames))
			{
				bIsOK = true;

				// Values here are individual frames
				for (const TPair<FString, TSharedPtr<FJsonValue>>& JFrame : Frames->Get()->Values)
				{
					FFrameTrackingContourData FrameContours;
					auto FrameJObject = JFrame.Value.Get()->AsObject();

					// Values are the content of each frame
					for (const TPair<FString, TSharedPtr<FJsonValue>>& JFrameContent : FrameJObject->Values)
					{
						const FString& CurveName = JFrameContent.Key;
						auto Points = JFrameContent.Value->AsArray();
						FTrackingContour Curve;
						// All 2D points are written as a flat array with X value followed by Y per point. Convert that to Point2D
						for (int32 PointID = 0; PointID < Points.Num() / 2; ++PointID)
						{
							double XVal = Points[2 * PointID]->AsNumber();
							double YVal = Points[2 * PointID + 1]->AsNumber();
							const FVector2D CurrentPoint = FVector2D(XVal, YVal);
							Curve.DensePoints.Add(CurrentPoint);
						}

						FrameContours.TrackingContours.Add(CurveName, Curve);
					}

					Contours.Add(FrameContours);
				}
			}
		}
	}
	else
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToLoadJsonFile);
		InPipelineData->SetErrorNodeMessage(FString::Printf(TEXT("Failed to load JSON file %s"), *JsonFile));
		return false;
	}

	if (!bIsOK)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::InvalidData);
		InPipelineData->SetErrorNodeMessage(FString::Printf(TEXT("Invalid data in JSON file %s"), *JsonFile));
		return false;
	}

	return bIsOK;
}

bool FJsonTrackerNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	int32 Frame = InPipelineData->GetFrameNumber();

	if (Frame < Contours.Num())
	{
		InPipelineData->SetData<FFrameTrackingContourData>(Pins[1], Contours[Frame]);
		return true;
	}
	else
	{
		return false;
	}
}

bool FJsonTrackerNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Contours.Reset();

	return true;
}

FOffsetContoursNode::FOffsetContoursNode(const FString& InName) : FNode("OffsetContours", InName)
{
	Pins.Add(FPin("Contours In", EPinDirection::Input, EPinType::Contours));
	Pins.Add(FPin("Contours Out", EPinDirection::Output, EPinType::Contours));
}

bool FOffsetContoursNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	RandomOffsetMinX = -RandomOffset.X / 2.0f;
	RandomOffsetMaxX = RandomOffset.X / 2.0f;
	RandomOffsetMinY = -RandomOffset.Y / 2.0f;
	RandomOffsetMaxY = RandomOffset.Y / 2.0f;

	return true;
}

bool FOffsetContoursNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const FFrameTrackingContourData& Input = InPipelineData->GetData<FFrameTrackingContourData>(Pins[0]);

	FFrameTrackingContourData Output = Input;

	for (auto& Contour : Output.TrackingContours)
	{
		for (FVector2D& Point : Contour.Value.DensePoints)
		{
			Point += ConstantOffset;
			Point.X += FMath::RandRange(RandomOffsetMinX, RandomOffsetMaxX);
			Point.Y += FMath::RandRange(RandomOffsetMinY, RandomOffsetMaxY);
		}
	}

	InPipelineData->SetData<FFrameTrackingContourData>(Pins[1], MoveTemp(Output));

	return true;
}

FSaveContoursToJsonNode::FSaveContoursToJsonNode(const FString& InName) : FNode("SaveContoursJSon", InName)
{
	Pins.Add(FPin("Contours In", EPinDirection::Input, EPinType::Contours));
}

bool FSaveContoursToJsonNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	ContourDataJson = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> Object = MakeShareable(new FJsonObject());
	ContourDataJson->SetObjectField(TEXT("Frames"), Object);

	return true;
}

bool FSaveContoursToJsonNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	bool bFrameAdded = false;
	const FFrameTrackingContourData& Contours = InPipelineData->GetData<FFrameTrackingContourData>(Pins[0]);

	if (Contours.ContainsData())
	{
		auto FramesObject = ContourDataJson->GetObjectField(TEXT("Frames"));
		const int32 FrameNumber = InPipelineData->GetFrameNumber();
		FString CurrentFrame = "Frame" + FString::FromInt(FrameNumber);

		TSharedRef<FJsonObject> PerCurveJData = MakeShared<FJsonObject>();
		
		for (const TPair<FString, FTrackingContour>& PerCurveData : Contours.TrackingContours)
		{
			TArray<TSharedPtr<FJsonValue>> JPoints;
			for (const FVector2D& Point : PerCurveData.Value.DensePoints)
			{				
				JPoints.Add(MakeShared<FJsonValueNumber>(Point.X));
				JPoints.Add(MakeShared<FJsonValueNumber>(Point.Y));
			}

			PerCurveJData->SetArrayField(PerCurveData.Key, JPoints);
		}

		FramesObject->SetObjectField(CurrentFrame, PerCurveJData);
		bFrameAdded = true;
	}

	return bFrameAdded;
}

bool FSaveContoursToJsonNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	bool bContoursSaved = false;
	FString TrackingContoursString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&TrackingContoursString, 0);
	if (FJsonSerializer::Serialize(ContourDataJson.ToSharedRef(), JsonWriter, true))
	{
		bContoursSaved = FFileHelper::SaveStringToFile(TrackingContoursString, *FullSavePath);
	}
	
	ContourDataJson.Reset();

	return bContoursSaved;
}

}