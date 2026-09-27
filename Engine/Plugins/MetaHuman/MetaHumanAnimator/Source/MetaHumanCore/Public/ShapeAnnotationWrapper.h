// Copyright Epic Games, Inc. All Rights Reserved.

// The purpose of this file it to define an interface to rlibv functionality that can
// be called by UE. Dont use dlib etc types here since that complicated the compile.

#pragma once

#include "CoreMinimal.h"
#include "MetaHumanContourData.h"

enum class ECurveDisplayMode : uint8
{
	Visualization,
	Editing
};

class METAHUMANCORE_API FShapeAnnotationWrapper
{
public:
	FShapeAnnotationWrapper();
	~FShapeAnnotationWrapper();

	/** Returns a list of control vertices for a curve. Start and end points are not included */
	TArray<FVector2D> GetControlVerticesForCurve(const TArray<FVector2D>& InLandmarkData, const FString& InCurveName, ECurveDisplayMode InDisplayMode) const;

	/** Returns point data that represents a Catmull-Rom spline, generated from contour data */
	TMap<FString, TArray<FVector2D>> GetDrawingSplinesFromContourData(const TObjectPtr<class UMetaHumanContourData> InContourData);

private:

	/** Initializes keypoints and keypoint curves in the form that rlibv::shapeAnnotation requires to generate curves */
	void InitializeShapeAnnotation(const TObjectPtr<class UMetaHumanContourData> InContourData, bool bUseDensePoints);

	class FImpl;
	TPimplPtr<FImpl> Impl;
};
