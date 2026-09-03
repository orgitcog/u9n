// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FrameTrackingContourData.h"

enum class EIdentityPoseType : uint8;
enum class EIdentityPartMeshes : uint8;

enum class ECurvePresetType : uint8
{
	Invalid = 0,
	Identity_NeutralPose,
	Identity_TeethPose,
	Performance
};

/** persistent data for curves, loaded from json */
struct FMarkerCurveDef
{
	FString Name;
	FString StartPointName;
	FString EndPointName;

	TArray<int32> VertexIDs;
	TArray<FVector2D> DefaultScreenPoints;
	TArray<FString> GroupTagIDs;

	FString CurveMeshFromConfig;
};

struct FMarkerDefs
{
	TArray<FString> GroupNames;
	TArray<FMarkerCurveDef> CurveDefs;
	TMap<FString, int32> Landmarks;
	TMap<FString, FVector2D> DefaultScreenPoints;
	TMap<FString, FString> CurveMeshesForMarkers;
};

class METAHUMANIDENTITY_API FLandmarkConfigIdentityHelper
{
public:

	FLandmarkConfigIdentityHelper();
	~FLandmarkConfigIdentityHelper();

	/** Returns all the marker definitions as per config */
	TSharedPtr<struct FMarkerDefs> GetMarkerDefs() const;

	TArray<FString> GetGroupListForSelectedPreset(const ECurvePresetType& InSelectedPose) const;

	/** Projects the 2D points based on 3D position of vertex IDs of the archetype mesh */
	FFrameTrackingContourData ProjectPromotedFrameCurvesOnTemplateMesh(const struct FMinimalViewInfo& InViewInfo, 
		const TMap<EIdentityPartMeshes, TArray<FVector>>& InTemplateMeshVertices, const ECurvePresetType& InSelectedPreset, const FIntRect& InViewRect) const;

	/** Uses preset values for curves in the config */
	FFrameTrackingContourData GetDefaultContourDataFromConfig(const FVector2D& InTexResolution, const ECurvePresetType& InSelectedPreset) const;

	/** Convert Identity Pose Type enum into a curve preset type */
	ECurvePresetType GetCurvePresetFromIdentityPose(const EIdentityPoseType& InIdentityPoseType) const;

private:

	void GetProjectedScreenCoordinates(const TArray<FVector>& InWorldPositions, const struct FMinimalViewInfo& InViewInfo,
		TArray<FVector2d>& OutScreenPositions, const FIntRect& InViewRect) const;

	void PopulateMarkerDataFromConfig(const TMap<FString, TSharedPtr<class FJsonValue>>& InConfigContourData);

	bool LoadCurvesAndLandmarksFromJson(const FString& InFileName);
	bool LoadGroupsFromJson(const FString& InFileName) const;

	TArray<struct FMarkerCurveDef> GetCurvesForPreset(const ECurvePresetType& InSelectedPose) const;

	EIdentityPartMeshes GetMeshPartFromConfigName(const FString& InMeshName) const;

	/** A struct containing non-changing marker group and curve data */
	TSharedPtr<struct FMarkerDefs> MarkerDefs;

	TSet<FString> NeutralPoseCurveExclusionList;
	TSet<FString> NeutralPoseGroupExclusionList;

	TSet<FString> TeethPoseCurveExclusionList;
	TSet<FString> TeethPoseGroupExclusionList;

	TSet<FString> PerformanceCurveList;
	TSet<FString> PerformanceCurveGroups;

	const static FString ConfigGroupFileName;
};