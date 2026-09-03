// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "MetaHumanPerformance.h"
#include "MetaHumanPerformanceExportUtils.h"
#include "ContourDataComparisonHelper.h"
#include "MetaHumanPerformanceLog.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanPerformanceFactoryNew.h"
#include "CaptureData.h"

#include "UObject/UObjectGlobals.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ObjectTools.h"
#include "FileHelpers.h"
#include "Modules/ModuleManager.h"
#include "Runtime/Engine/Classes/Animation/Skeleton.h"
#include "Runtime/Engine/Classes/Animation/AnimSequence.h"


// The threshold is selected to address a hardware related difference
float UContourDataComparisonHelper::ContourComparisonTolerance = 2.4;

bool UContourDataComparisonHelper::ComparePerformanceContourData(const UMetaHumanPerformance* InOriginal, const UMetaHumanPerformance* InNew)
{
	bool bMatch = true;

	if (!InOriginal || !InNew)
	{
		UE_LOG(LogMetaHumanPerformance, Error, TEXT("Invalid performance asset was specified for contour data comparison"));
		return false;
	}

	if (InOriginal->ContourTrackingResults.Num() != InNew->ContourTrackingResults.Num())
	{
		UE_LOG(LogMetaHumanPerformance, Error, TEXT("Mismatch in number of frames for contour data"));
		return false;
	}

	const TArray64<FFrameTrackingContourData>& NewContours = InNew->ContourTrackingResults;

	// Check if curve names for contour data match

	uint32 FirstValidFrame = 0;
	for (const FFrameTrackingContourData& OriginalFrameContourData : InOriginal->ContourTrackingResults)
	{
		bool bGoldDataFrameProcessed = OriginalFrameContourData.ContainsData();
		bool bGeneratedDataFrameProcessed = NewContours[FirstValidFrame].ContainsData();
		if (bGoldDataFrameProcessed && bGeneratedDataFrameProcessed)
		{
			bool bSchemesMatch = true;
			FString MismatchedCurves;
			for (const TPair<FString, FTrackingContour>& PerCurveData : OriginalFrameContourData.TrackingContours)
			{
				bool bCurveNameMatches = NewContours[FirstValidFrame].TrackingContours.Contains(PerCurveData.Key);
				bSchemesMatch &= bCurveNameMatches;
				if (!bCurveNameMatches)
				{
					MismatchedCurves += FString(" " + PerCurveData.Key);
				}				
			}

			if (!bSchemesMatch)
			{
				UE_LOG(LogTemp, Error, TEXT("A mismatch for following contour names:%s"), *MismatchedCurves);
				return false;
			}

			// We only need to check curve names once on any frame that contains contour data
			break;
		}
		else if (bGoldDataFrameProcessed != bGeneratedDataFrameProcessed)
		{
			UE_LOG(LogTemp, Error, TEXT("A mismatch in contour data presence between gold and test data for frame %d"), FirstValidFrame);
			return false;
		}

		++FirstValidFrame;
	}

	// Check if dense points for performances match

	uint32 FrameNum = 0;
	for (const FFrameTrackingContourData& OriginalFrameContourData : InOriginal->ContourTrackingResults)
	{
		if (OriginalFrameContourData.ContainsData() != NewContours[FrameNum].ContainsData())
		{
			UE_LOG(LogTemp, Error, TEXT("Contour data presence mismatch for frame %i"), FrameNum);
			bMatch = false;
		}

		const TMap<FString, FTrackingContour>& NewFrameContours = NewContours[FrameNum].TrackingContours;
		for (const TPair<FString, FTrackingContour>& PerCurveData : OriginalFrameContourData.TrackingContours)
		{
			bool bDensePointsMatch = true;
			double MaximumCurveDelta = 0.0;

			for (int32 CurvePoint = 0; CurvePoint < PerCurveData.Value.DensePoints.Num(); ++CurvePoint)
			{
				const FVector2D& Original = PerCurveData.Value.DensePoints[CurvePoint];
				const FVector2D& New = NewFrameContours[PerCurveData.Key].DensePoints[CurvePoint];

				if (!Original.Equals(New, ContourComparisonTolerance))
				{
					double CurrentDelta = FVector2D::Distance(Original, New);
					MaximumCurveDelta = CurrentDelta > MaximumCurveDelta ? CurrentDelta : MaximumCurveDelta;
					bDensePointsMatch = false;
				}
			}

			if (!bDensePointsMatch)
			{
				UE_LOG(LogTemp, Error, TEXT("Contour data mismatch for frame %i curve %s. Maximum point delta for this curve was %f"), FrameNum, *PerCurveData.Key, MaximumCurveDelta);
				bMatch = false;
			}
		}

		++FrameNum;
	}

	return bMatch;
}


#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanPerformanceExportAnimationSequenceTest, "MetaHuman.Performance.Export Animation Sequence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMetaHumanPerformanceExportAnimationSequenceTest::RunTest(const FString& InParameters)
{
	USkeleton* FaceArchetypeSkeleton = LoadObject<USkeleton>(GetTransientPackage(), TEXT("/Script/Engine.Skeleton'/" UE_PLUGIN_NAME "/IdentityTemplate/Face_Archetype_Skeleton.Face_Archetype_Skeleton'"));
	UTEST_NOT_NULL("Face_Archetype_Skeleton should be valid", FaceArchetypeSkeleton);

	UMetaHumanPerformance* Performance = NewObject<UMetaHumanPerformance>(GetTransientPackage(), NAME_None, RF_Transient);
	UTEST_NOT_NULL("Failed to create Performance object", Performance);

	constexpr int32 NumFrames = 100;
	Performance->StartFrameToProcess = 0;
	Performance->EndFrameToProcess = NumFrames;
	Performance->AnimationData.AddDefaulted(NumFrames);
	const FFrameRate FrameRate = Performance->GetFrameRate();

	// Generate the reference data that will be written to the animation sequence curves so we can compare later if the data was altered
	TArray<float> ReferenceTimes, ReferenceValues;
	for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
	{
		const float DebugCurveValue = static_cast<float>(FrameIndex) / static_cast<float>(NumFrames);

		ReferenceTimes.Add(FrameIndex / FrameRate.AsDecimal());
		ReferenceValues.Add(DebugCurveValue);

		FFrameAnimationData FrameData;
		FrameData.Pose.SetLocation(FVector(DebugCurveValue));
		FrameData.Pose.SetRotation(FRotator::MakeFromEuler(FVector(DebugCurveValue)).Quaternion());
		FrameData.Pose.SetScale3D(FVector{ 1.0 });

		FaceArchetypeSkeleton->ForEachCurveMetaData([&FrameData, DebugCurveValue](const FName& InCurveName, const FCurveMetaData& InMetaData)
		{
			const FString CurveNameStr = InCurveName.ToString();
			if (CurveNameStr.StartsWith(TEXT("CTRL_")))
			{
				FrameData.AnimationData.FindOrAdd(CurveNameStr) = DebugCurveValue;
			}
		});

		Performance->AnimationData[FrameIndex] = MoveTemp(FrameData);
	}

	UMetaHumanPerformanceExportAnimationSettings* ExportAnimSettings = NewObject<UMetaHumanPerformanceExportAnimationSettings>();
	ExportAnimSettings->bAutoSaveAnimSequence = false;
	ExportAnimSettings->bShowExportDialog = false;
	ExportAnimSettings->TargetSkeletonOrSkeletalMesh = FaceArchetypeSkeleton;
	ExportAnimSettings->ExportRange = EPerformanceExportRange::ProcessingRange;

	// Export the animation sequence so we can compare the data in the Anim Sequence with the reference data
	UAnimSequence* ExportedAnimSequence = UMetaHumanPerformanceExportUtils::ExportAnimationSequence(Performance, ExportAnimSettings);

	UTEST_NOT_NULL("Exported Animation Sequence", ExportedAnimSequence);

	const FAnimationCurveData& CurveData = ExportedAnimSequence->GetDataModel()->GetCurveData();
	for (const FFloatCurve& Curve : CurveData.FloatCurves)
	{
		const FString CurveNameStr = Curve.GetName().ToString();

		if (CurveNameStr.StartsWith(TEXT("CTRL_")))
		{
			TArray<float> KeyTimes, KeyValues;
			Curve.GetKeys(KeyTimes, KeyValues);

			UTEST_EQUAL(*FString::Format(TEXT("Number of key times for curve {0}"), { CurveNameStr }), KeyTimes.Num(), ReferenceTimes.Num());
			UTEST_EQUAL(*FString::Format(TEXT("Number of key values for curve {0}"), { CurveNameStr }), KeyValues.Num(), ReferenceValues.Num());

			for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
			{
				UTEST_EQUAL(*FString::Format(TEXT("Key time for frame {0} in curve {1}"), { FrameIndex, CurveNameStr }), KeyTimes[FrameIndex], ReferenceTimes[FrameIndex]);
				UTEST_EQUAL(*FString::Format(TEXT("Key value for frame {0} in curve {1}"), { FrameIndex, CurveNameStr }), KeyValues[FrameIndex], ReferenceValues[FrameIndex]);
			}
		}
	}
	// Notify the asset registry
	FAssetRegistryModule::AssetDeleted(ExportedAnimSequence);
	
	// Rename the objects we created out of the way
	ExportedAnimSequence->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);

	ExportedAnimSequence->MarkAsGarbage();
	ObjectTools::DeleteAssets({ ExportedAnimSequence }, false);


	return true;
}

#endif