// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "LoadLiveLinkFaceCameraCalibration.generated.h"

// simple structs allowing us to use FJsonObjectConverter::JsonObjectStringToUStruct to load in a camera calibration json file
USTRUCT()
struct FDimensions
{
	GENERATED_BODY()

public:

	UPROPERTY()
	int32 Width = 0;

	UPROPERTY()
	int32 Height = 0;

};

USTRUCT()
struct FLiveLinkFaceCalibrationData
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FString Version;

	UPROPERTY()
	FString DeviceModel;

	UPROPERTY()
	FDimensions VideoDimensions;

	UPROPERTY()
	FDimensions DepthDimensions;

	UPROPERTY()
	FVector2D LensDistortionCenter = FVector2D::ZeroVector;

	UPROPERTY()
	TArray<double> IntrinsicMatrix;

	UPROPERTY()
	TArray<double> LensDistortionLookupTable;

	UPROPERTY()
	TArray<double> InverseLensDistortionLookupTable;

	UPROPERTY()
	double PixelSize = 0.0;

	UPROPERTY()
	FDimensions IntrinsicMatrixReferenceDimensions;
};

METAHUMANCORE_API class UCameraCalibration* LoadLiveLinkFaceCameraCalibration(UClass* InClass,
																					   UObject* InParent,
																					   FName InName,
																					   EObjectFlags InFlags,
																					   const FString& InFilenameOrString,
																					   bool bIsFile = true);