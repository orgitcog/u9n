// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Algo/AnyOf.h"

#include "FrameTrackingContourData.generated.h"

USTRUCT()
struct METAHUMANCORETECH_API FMarkerCurveState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bVisible = false;

	UPROPERTY()
	bool bActive = false;

	UPROPERTY(Transient)
	bool bSelected = false;
};

USTRUCT()
struct METAHUMANCORETECH_API FTrackingContour
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "InternalPoints")
	TArray<FVector2D> DensePoints;

	UPROPERTY()
	TArray<float> DensePointsConfidence;

	UPROPERTY()
	FString StartPointName;

	UPROPERTY()
	FString EndPointName;

	UPROPERTY(EditAnywhere, Category = "Markers")
	FMarkerCurveState State;

	friend FArchive& operator<<(FArchive& Ar, FTrackingContour& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	void Serialize(FArchive& Ar)
	{
		Ar << DensePoints;
		Ar << DensePointsConfidence;
		Ar << StartPointName;
		Ar << EndPointName;
		Ar << State.bVisible;
		Ar << State.bActive;
	}
};

USTRUCT()
struct METAHUMANCORETECH_API FFrameTrackingContourData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Tracking", DisplayName = "Camera Name")
	FString Camera;

	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Tracking", DisplayName = "Markers")
	TMap<FString, FTrackingContour> TrackingContours;

	inline bool ContainsData() const
	{
		if (TrackingContours.Num() > 0)
		{
			for (const auto& Contour : TrackingContours)
			{
				if (Contour.Value.DensePoints.Num() > 0)
				{
					return true;
				}
			}
		}

		return false;
	}

	inline bool ContainsActiveData() const
	{
		return Algo::AnyOf(TrackingContours, [](const TPair<FString, FTrackingContour>& Contour)
		{
			return Contour.Value.State.bActive;
		});
	}

	friend FArchive& operator<<(FArchive& Ar, FFrameTrackingContourData& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	void Serialize(FArchive& Ar)
	{
		Ar << Camera;
		Ar << TrackingContours;
	}
};

USTRUCT()
struct METAHUMANCORETECH_API FTrackingContour3D
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Tracking")
	TArray<FVector3d> DensePoints;
};