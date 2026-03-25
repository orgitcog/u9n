// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/HyprsenseTestNode.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"

#define LOCTEXT_NAMESPACE "MetaHuman"

namespace UE::MetaHuman::Pipeline
{
#if WITH_DEV_AUTOMATION_TESTS
	FHyprsenseTestNode::FHyprsenseTestNode(const FString& InName) : FNode("Hyprsense", InName)
	{
		Pins.Add(FPin("UE Image In", EPinDirection::Input, EPinType::UE_Image));
		Pins.Add(FPin("Contours In", EPinDirection::Input, EPinType::Contours));
		Pins.Add(FPin("Avg Diff Out", EPinDirection::Output, EPinType::Float));

	}

	bool FHyprsenseTestNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
	{
		FString JsonRaw;
		bool bIsOK = false;
		if (FFileHelper::LoadFileToString(JsonRaw, *InJsonFilePath))
		{
			TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(JsonRaw);
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

						ContourByFrame.Add(FrameContours);
					}
				}
			}
		}

		return bIsOK;
	}

	bool FHyprsenseTestNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
	{
		const FUEImageDataType& Image = InPipelineData->GetData<FUEImageDataType>(Pins[0]);
		const FFrameTrackingContourData& Contours = InPipelineData->GetData<FFrameTrackingContourData>(Pins[1]);

		FUEImageDataType Output = Image;
		auto InputContours = ContourByFrame[FrameCount];

		TMap<FString, float> ContourDiffAverage;
		float TotalAverage = 0.0f;
		int32 TotalContourCount = 0;

		for (const auto& Contour : Contours.TrackingContours)
		{
			float Average = 0.0f;
			auto Value = Contour.Value;

			// if we are allowing more curves in the tracking output data than in the reference data
			// carry on if the tracking output is not present in the reference data
			if (bAllowExtraCurvesInTrackingData && !InputContours.TrackingContours.Contains(Contour.Key))
			{
				continue;
			}

			auto InContour = InputContours.TrackingContours[Contour.Key];

			if (Value.DensePoints.Num() == InContour.DensePoints.Num())
			{
				int32 NumArray = Value.DensePoints.Num();
				for (int32 I = 0; I < NumArray; I++)
				{
					FVector2D Point1 = Value.DensePoints[I];
					FVector2D Point2 = InContour.DensePoints[I];
					float Distance = UKismetMathLibrary::Distance2D(Point1, Point2);
					Average += Distance;
				}
				TotalContourCount += NumArray;
				TotalAverage += Average;
				Average /= NumArray;
				MaxAverageDifference = (MaxAverageDifference < Average) ? Average : MaxAverageDifference;
				ContourDiffAverage.Add(Contour.Key, Average);
			}

		}
		TotalAverage /= TotalContourCount;
		TotalAverageInAllFrames += TotalAverage;
		TotalLandmarkDiffAverageByFrame.Add(TotalAverage);
		ContourDiffAverageByFrame.Add(ContourDiffAverage);

		InPipelineData->SetData<float>(Pins[2], MoveTemp(TotalAverage));

		FrameCount++;
		return true;
	}

	bool FHyprsenseTestNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
	{
		FString JsonString;
		TSharedRef<TJsonWriter<TCHAR>> JsonObject = TJsonWriterFactory<>::Create(&JsonString);
		JsonObject->WriteObjectStart();

		int32 FrameNum = ContourDiffAverageByFrame.Num();
		TotalAverageInAllFrames /= FrameNum;

		for (int32 I = 0; I < FrameNum; I++)
		{
			JsonObject->WriteObjectStart("Frame " + FString::FromInt(I));
			{
				auto ContourDiffAverages = ContourDiffAverageByFrame[I];
				auto LandmarkDiffAverage = TotalLandmarkDiffAverageByFrame[I];
				JsonObject->WriteObjectStart("Contours");
				for (auto ContourDiff : ContourDiffAverages)
				{
					JsonObject->WriteValue(ContourDiff.Key, ContourDiff.Value);
				}
				JsonObject->WriteObjectEnd();
				JsonObject->WriteValue("Average", LandmarkDiffAverage);
			}
			JsonObject->WriteObjectEnd();
		}
		JsonObject->WriteValue("Max Average Difference", MaxAverageDifference);

		JsonObject->WriteValue("Total Average", TotalAverageInAllFrames);

		JsonObject->WriteObjectEnd();
		JsonObject->Close();

		FFileHelper::SaveStringToFile(*JsonString, *OutJsonFilePath);
		return true;
	}
#endif
}
#undef LOCTEXT_NAMESPACE
