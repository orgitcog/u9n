// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandmarkConfigIdentityHelper.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityLog.h"
#include "MetaHumanContourDataVersion.h"

#include "Camera/CameraTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SceneView.h"


const FString FLandmarkConfigIdentityHelper::ConfigGroupFileName = TEXT("curve_groups.json");

FLandmarkConfigIdentityHelper::FLandmarkConfigIdentityHelper()
{
	NeutralPoseCurveExclusionList = {"pt_tooth_lower", "pt_tooth_upper", "pt_tooth_lower_2", "pt_tooth_upper_2", "pt_frankfurt_fl", "pt_frankfurt_fr", "pt_frankfurt_rl", "pt_frankfurt_rr"};
	NeutralPoseGroupExclusionList = {"teeth", "cheeks_l", "cheeks_r"};

	TeethPoseCurveExclusionList = {"brow_middle_line_l", "brow_middle_line_r", "crv_ear_outer_helix_l", "ear_outer_helix_r", "crv_ear_inner_helix_l",
		"crv_ear_inner_helix_r", "crv_ear_central_lower_l", "crv_ear_central_lower_r", "crv_ear_central_upper_l", "crv_ear_central_upper_r",
		"pt_frankfurt_fl", "pt_frankfurt_fr", "pt_frankfurt_rl", "pt_frankfurt_rr"};
	TeethPoseGroupExclusionList = {"cheeks_l", "cheeks_r", "ear_l", "ear_r"};

	// TODO: This is currently not used. Will be needed when performance gets the outliner
	PerformanceCurveGroups = { "teeth", "brow_l", "brow_r", "eye_l", "eye_r", "lip_upper", "lip_lower" };
	PerformanceCurveList = { "crv_eyelid_lower_l", "crv_eyelid_lower_r", "crv_eyelid_upper_l", "crv_eyelid_upper_r", "crv_iris_l", "crv_iris_r", "crv_lip_upper_outer_l", "crv_lip_upper_outer_r",
		"crv_lip_lower_outer_l", "crv_lip_lower_outer_r", "crv_lip_lower_inner_l", "crv_lip_lower_inner_r", "crv_lip_upper_inner_l", "crv_lip_upper_inner_r", "crv_lip_philtrum_l", "crv_lip_philtrum_r",
		"pt_tooth_lower", "crv_brow_upper_l", "crv_brow_lower_l", "crv_brow_intermediate_l", "crv_brow_upper_r", "crv_brow_lower_r", "crv_brow_intermediate_r", "crv_eyefold_l", "crv_eyefold_r",
		"pt_left_contact", "pt_right_contact"
	};

	if (!LoadCurvesAndLandmarksFromJson(TEXT("curves_config.json")) || !LoadGroupsFromJson(TEXT("curve_groups.json")))
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Failed to load curve data for the outliner"));
	}
}

FLandmarkConfigIdentityHelper::~FLandmarkConfigIdentityHelper()
{
}

TSharedPtr<FMarkerDefs> FLandmarkConfigIdentityHelper::GetMarkerDefs() const
{
	return MarkerDefs;
}

bool FLandmarkConfigIdentityHelper::LoadCurvesAndLandmarksFromJson(const FString& InFileName)
{
	const FString JsonFilePath = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME))->GetContentDir() + "/MeshFitting/Template/" + InFileName;

	FString JsonString;

	FFileHelper::LoadFileToString(JsonString, *JsonFilePath);

	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		TMap<FString, TSharedPtr<FJsonValue>> TopLevelValues = JsonObject->Values;

		if (TSharedPtr<FJsonValue>* ConfigDataEntry = TopLevelValues.Find("data"))
		{
			TMap<FString, TSharedPtr<FJsonValue>> ConfigCurveDataMap = (*ConfigDataEntry)->AsObject()->Values;
			PopulateMarkerDataFromConfig(ConfigCurveDataMap);
		}
		else
		{
			UE_LOG(LogMetaHumanIdentity, Error, TEXT("Unable to parse curves_config.json. The config does not contain data field"));
		}
	}
	else
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("%s"), *JsonReader->GetErrorMessage());
		return false;
	}

	return true;
}

bool FLandmarkConfigIdentityHelper::LoadGroupsFromJson(const FString& InFileName) const
{
	const FString JsonFilePath = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir() + "/MeshFitting/Template/" + InFileName;

	FString JsonString;

	FFileHelper::LoadFileToString(JsonString, *JsonFilePath);

	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject());
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(JsonReader, RootJsonObject) && RootJsonObject.IsValid())
	{
		TMap<FString, TSharedPtr<FJsonValue>> Values = RootJsonObject->Values;
		MarkerDefs->GroupNames.Empty();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Values)
		{
			FString GroupName = Pair.Key;
			MarkerDefs->GroupNames.Add(GroupName);

			const TSharedPtr<FJsonValue> JsonValue = Pair.Value;
			const TArray<TSharedPtr<FJsonValue>> CurvesArray = JsonValue->AsArray();

			for (int32 Index = 0; Index < CurvesArray.Num(); ++Index)
			{
				FString CurveName = CurvesArray[Index]->AsString();

				int32 FoundIndex = MarkerDefs->CurveDefs.IndexOfByPredicate
				(
					[&CurveName](const FMarkerCurveDef& Curve)
					{
						return CurveName == Curve.Name;
					}
				);

				if (FoundIndex >= 0)
				{
					MarkerDefs->CurveDefs[FoundIndex].GroupTagIDs.Add(GroupName);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("%s"), *JsonReader->GetErrorMessage());
		return false;
	}
	return true;
}

void FLandmarkConfigIdentityHelper::PopulateMarkerDataFromConfig(const TMap<FString, TSharedPtr<class FJsonValue>>& InConfigContourData)
{
	MarkerDefs = MakeShared<FMarkerDefs>();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : InConfigContourData)
	{
		FString Name = Pair.Key;
		TSharedPtr<FJsonValue> JsonValue = Pair.Value;
		TSharedPtr<FJsonObject> SubObject = JsonValue->AsObject();
		FString SubObjectType = SubObject->GetStringField(TEXT("type"));
		if (SubObjectType == TEXT("curve"))
		{
			FMarkerCurveDef MarkerCurveDef;
			MarkerCurveDef.Name = Name;

			//"start", "end" and "vIDs" have to be specified in the config
			SubObject->TryGetStringField(TEXT("start"), MarkerCurveDef.StartPointName);
			SubObject->TryGetStringField(TEXT("end"), MarkerCurveDef.EndPointName);

			const TArray<TSharedPtr<FJsonValue>>* VertexIDs;
			SubObject->TryGetArrayField(TEXT("vIDs"), VertexIDs);
			for (TSharedPtr<FJsonValue> ID : *VertexIDs)
			{
				MarkerCurveDef.VertexIDs.Add(ID->AsNumber());
			}

			if (!SubObject->TryGetStringField(TEXT("Mesh"), MarkerCurveDef.CurveMeshFromConfig))
			{
				MarkerCurveDef.CurveMeshFromConfig = "head";
			}

			// Get default 2D Screen position stored in the config
			const TArray<TSharedPtr<FJsonValue>>* ScreenPositions;
			SubObject->TryGetArrayField(TEXT("screen_default"), ScreenPositions);
			TArray<FVector2D> Positions;
			Positions.Init(FVector2D(UE_SMALL_NUMBER, UE_SMALL_NUMBER), ScreenPositions->Num() / 2);

			int32 ScreenPositionCtr = 0;
			for (TSharedPtr<FJsonValue> Pos : *ScreenPositions)
			{
				if (Positions[ScreenPositionCtr].X == UE_SMALL_NUMBER)
				{
					Positions[ScreenPositionCtr].X = Pos->AsNumber();
				}
				else
				{
					Positions[ScreenPositionCtr].Y = Pos->AsNumber();
					++ScreenPositionCtr;
				}
			}

			MarkerCurveDef.DefaultScreenPoints = Positions;

			MarkerDefs->CurveDefs.Add(MarkerCurveDef);
		}
		else if (SubObjectType == TEXT("landmark"))
		{
			FString VIDString = SubObject->GetStringField(TEXT("vID"));
			int32 VertexID = FCString::Atoi(*VIDString);

			const TArray<TSharedPtr<FJsonValue>>* ScreenPositions;
			SubObject->TryGetArrayField(TEXT("screen_default"), ScreenPositions);
			TArray<FVector2D> Positions = { FVector2D((*ScreenPositions)[0]->AsNumber(), ScreenPositions->Last()->AsNumber()) };

			if (SubObject->HasField(TEXT("point_curve")))
			{
				FMarkerCurveDef MarkerCurveDef;
				MarkerCurveDef.Name = Name;
				MarkerCurveDef.VertexIDs.Add(VertexID);

				MarkerCurveDef.DefaultScreenPoints = Positions;
				MarkerDefs->CurveDefs.Add(MarkerCurveDef);
			}
			else
			{
				MarkerDefs->Landmarks.Add(Name, VertexID);
				MarkerDefs->DefaultScreenPoints.Add(Name, Positions.Last());

				FString VertexMesh;
				if (!SubObject->TryGetStringField(TEXT("mesh"), VertexMesh))
				{
					VertexMesh = "head";
				}

				MarkerDefs->CurveMeshesForMarkers.Add(Name, VertexMesh);
			}
		}
	}
}

TArray<FMarkerCurveDef> FLandmarkConfigIdentityHelper::GetCurvesForPreset(const ECurvePresetType& InCurvePreset) const
{
	TArray<FMarkerCurveDef> PresetCurves = MarkerDefs->CurveDefs;

	if (InCurvePreset == ECurvePresetType::Identity_NeutralPose)
	{
		for (const FString& Curve : NeutralPoseCurveExclusionList)
		{
			int32 ItemToRemove = PresetCurves.IndexOfByPredicate([&Curve](const FMarkerCurveDef& CurveDef)
			{
				return CurveDef.Name == Curve;
			});
			
			if (ItemToRemove != INDEX_NONE)
			{
				PresetCurves.RemoveAt(ItemToRemove);
			}
		}
	}
	else if (InCurvePreset == ECurvePresetType::Identity_TeethPose)
	{
		for (const FString& Curve : TeethPoseCurveExclusionList)
		{
			int32 ItemToRemove = PresetCurves.IndexOfByPredicate([&Curve](const FMarkerCurveDef& CurveDef)
			{
				return CurveDef.Name == Curve;
			});

			if (ItemToRemove != INDEX_NONE)
			{
				PresetCurves.RemoveAt(ItemToRemove);
			}
		}
	}
	else if (InCurvePreset == ECurvePresetType::Performance)
	{
		TArray<FMarkerCurveDef> PerformanceCurves;
		for (const FMarkerCurveDef& Curve : PresetCurves)
		{
			if (PerformanceCurveList.Contains(Curve.Name))
			{
				PerformanceCurves.Add(Curve);
			}
		}

		PresetCurves = PerformanceCurves;
	}

	return PresetCurves;
}

EIdentityPartMeshes FLandmarkConfigIdentityHelper::GetMeshPartFromConfigName(const FString& InMeshName) const
{
	EIdentityPartMeshes MeshPart = EIdentityPartMeshes::Invalid;

	if (InMeshName == "head")
	{
		MeshPart = EIdentityPartMeshes::Head;
	}
	else if (InMeshName == "eye_right")
	{
		MeshPart = EIdentityPartMeshes::RightEye;
	}
	else if (InMeshName == "eye_left")
	{
		MeshPart = EIdentityPartMeshes::LeftEye;
	}

	return MeshPart;
}

TArray<FString> FLandmarkConfigIdentityHelper::GetGroupListForSelectedPreset(const ECurvePresetType& InSelectedPreset) const
{
	TArray<FString> GroupsForPose = MarkerDefs->GroupNames;

	if (InSelectedPreset == ECurvePresetType::Identity_NeutralPose)
	{
		for (const FString& Curve : NeutralPoseGroupExclusionList)
		{
			if (GroupsForPose.Contains(Curve))
			{
				GroupsForPose.Remove(Curve);
			}
		}
	}
	else if (InSelectedPreset == ECurvePresetType::Identity_TeethPose)
	{
		for (const FString& Curve : TeethPoseGroupExclusionList)
		{
			if (GroupsForPose.Contains(Curve))
			{
				GroupsForPose.Remove(Curve);
			}
		}
	}
	
	return GroupsForPose;
}

ECurvePresetType FLandmarkConfigIdentityHelper::GetCurvePresetFromIdentityPose(const EIdentityPoseType& InIdentityPoseType) const
{
	ECurvePresetType Preset = ECurvePresetType::Invalid;

	if (InIdentityPoseType == EIdentityPoseType::Neutral)
	{
		Preset = ECurvePresetType::Identity_NeutralPose;
	}
	else if (InIdentityPoseType == EIdentityPoseType::Teeth)
	{
		Preset = ECurvePresetType::Identity_TeethPose;
	}
	
	return Preset;
}

void FLandmarkConfigIdentityHelper::GetProjectedScreenCoordinates(const TArray<FVector>& InWorldPositions, 
	const FMinimalViewInfo& InViewInfo, TArray<FVector2d>& OutScreenPositions, const FIntRect& InViewRect) const
{
	TArray<bool> ValidFlags;
	TOptional<FMatrix> CustomProjectionMatrix;
	
	FMatrix CaptureViewMatrix, CaptureProjectionMatrix, CaptureViewProjectionMatrix;
	UGameplayStatics::CalculateViewProjectionMatricesFromMinimalView(InViewInfo, CustomProjectionMatrix,
		CaptureViewMatrix, CaptureProjectionMatrix, CaptureViewProjectionMatrix);

	FVector2d OutScreenPosition;
	OutScreenPositions.Init(FVector2D::ZeroVector, InWorldPositions.Num());
	
	// This can fail for certain input (e.g. footage capture data), so the fallback is zero vector
	for (int32 i = 0; i < InWorldPositions.Num(); i++)
	{
		if (FSceneView::ProjectWorldToScreen(InWorldPositions[i], InViewRect, CaptureViewProjectionMatrix, OutScreenPosition))
		{
			OutScreenPositions[i] = OutScreenPosition;
		}
	}
}

FFrameTrackingContourData FLandmarkConfigIdentityHelper::ProjectPromotedFrameCurvesOnTemplateMesh(const FMinimalViewInfo& InViewInfo,
	const TMap<EIdentityPartMeshes, TArray<FVector>>& InTemplateMeshVertices, const ECurvePresetType& InSelectedPreset, const FIntRect& InViewRect) const
{
	FFrameTrackingContourData Contours;	
	TArray<FMarkerCurveDef> CurvesForPose = GetCurvesForPreset(InSelectedPreset);
	
	for (const FMarkerCurveDef& Curve : CurvesForPose)
	{
		TArray<FVector> FeatureVerts;

		if (const TArray<FVector>* MeshVertices = InTemplateMeshVertices.Find(GetMeshPartFromConfigName(Curve.CurveMeshFromConfig)))
		{
			for (const int32 ID : Curve.VertexIDs)
			{
				FeatureVerts.Add((*MeshVertices)[ID]);
			}
		}

		TArray<FVector2d> ScreenCoordinates;
		GetProjectedScreenCoordinates(FeatureVerts, InViewInfo, ScreenCoordinates, InViewRect);

		FTrackingContour TrackingContour;
		TrackingContour.DensePoints = ScreenCoordinates;
		TrackingContour.StartPointName = Curve.StartPointName;
		TrackingContour.EndPointName = Curve.EndPointName;
		Contours.TrackingContours.Add(Curve.Name, TrackingContour);
	}

	for (const TPair<FString, int32>& Landmark : MarkerDefs->Landmarks)
	{
		TArray<FVector2d> ScreenCoordinates;
		if (FString* CurveMeshForMarker = MarkerDefs->CurveMeshesForMarkers.Find(Landmark.Key))
		{
			if (const TArray<FVector>* MeshVertices = InTemplateMeshVertices.Find(GetMeshPartFromConfigName(*CurveMeshForMarker)))
			{
				FVector Vert = (*MeshVertices)[Landmark.Value];
				GetProjectedScreenCoordinates({ Vert }, InViewInfo, ScreenCoordinates, InViewRect);
			}
		}

		FTrackingContour TrackingContour;
		TrackingContour.DensePoints = ScreenCoordinates;
		Contours.TrackingContours.Add(Landmark.Key, TrackingContour);
	}

	return Contours;
}

FFrameTrackingContourData FLandmarkConfigIdentityHelper::GetDefaultContourDataFromConfig(const FVector2D& InTexResolution, const ECurvePresetType& InSelectedPreset) const
{
	FFrameTrackingContourData Contours;
	FVector2D ConfigProjectedTex = FVector2D(2048, 2048);

	// I'm not sure if this math is technically correct, but it gives good results for initial position
	FVector2D ScreenOffset = (FVector2D(1, 1) - InTexResolution / ConfigProjectedTex) * InTexResolution;
	TArray<FMarkerCurveDef> CurvesForPose = GetCurvesForPreset(InSelectedPreset);

	for (const FMarkerCurveDef& Curve : CurvesForPose)
	{
		FTrackingContour TrackingContour;
		TArray<FVector2D> OffsetPoints;

		for (const FVector2D& Point : Curve.DefaultScreenPoints)
		{
			OffsetPoints.Add(Point * ConfigProjectedTex - ScreenOffset);
		}
		
		TrackingContour.DensePoints = OffsetPoints;
		TrackingContour.StartPointName = Curve.StartPointName;
		TrackingContour.EndPointName = Curve.EndPointName;
		Contours.TrackingContours.Add(Curve.Name, TrackingContour);

		// If Curve contains Start/End Keypoints, add them to Tracking Contours list as well
		if (!Curve.StartPointName.IsEmpty() && MarkerDefs->DefaultScreenPoints.Contains(Curve.StartPointName))
		{
			FTrackingContour TrackingContourKeyPoint;
			FVector2D KeyPoint = MarkerDefs->DefaultScreenPoints[Curve.StartPointName];
			TrackingContourKeyPoint.DensePoints.Add(KeyPoint * ConfigProjectedTex - ScreenOffset);
			Contours.TrackingContours.Add(Curve.StartPointName, TrackingContourKeyPoint);
		}
		if (!Curve.EndPointName.IsEmpty() && MarkerDefs->DefaultScreenPoints.Contains(Curve.EndPointName))
		{
			FTrackingContour TrackingContourKeyPoint;
			FVector2D KeyPoint = MarkerDefs->DefaultScreenPoints[Curve.EndPointName];
			TrackingContourKeyPoint.DensePoints.Add(KeyPoint * ConfigProjectedTex - ScreenOffset);
			Contours.TrackingContours.Add(Curve.EndPointName, TrackingContourKeyPoint);
		}
	}

	return Contours;
}
