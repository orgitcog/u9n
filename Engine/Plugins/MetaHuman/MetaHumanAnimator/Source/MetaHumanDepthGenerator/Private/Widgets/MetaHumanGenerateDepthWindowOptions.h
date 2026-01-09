// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "MetaHumanCaptureSource.h"

#include "CameraCalibration.h"

#include "MetaHumanGenerateDepthWindowOptions.Generated.h"

UCLASS(BlueprintType)
class UMetaHumanGenerateDepthWindowOptions
	: public UObject
{
public:

	static const FString ImageSequenceDirectoryName;

	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	FString AssetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options", meta = (ContentDir))
	FDirectoryPath PackagePath;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	FDirectoryPath ImageSequenceRootPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bAutoSaveAssets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bShouldExcludeDepthFilesFromImport = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bShouldCompressDepthFiles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	TObjectPtr<UCameraCalibration> ReferenceCameraCalibration = nullptr;

	/* 
	The minimum cm from the camera expected for valid depth information.
	Depth information closer than this will be ignored to help filter out noise.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth Options",
			  meta = (Units="Centimeters",
					  ClampMin = "0.0", ClampMax = "200.0",
					  UIMin = "0.0", UIMax = "200.0"))
	float MinDistance = 10.0;

	/* 
	The maximum cm from the camera expected for valid depth information.
	Depth information beyond this will be ignored to help filter out noise.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth Options",
			  meta = (Units="Centimeters",
					  ClampMin = "0.0", ClampMax = "200.0",
					  UIMin = "0.0", UIMax = "200.0"))
	float MaxDistance = 25.0;

	/* Precision of the calculated depth data. Full precision is more accurate, but requires more disk space to store. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth Options")
	EMetaHumanCaptureDepthPrecisionType DepthPrecision = EMetaHumanCaptureDepthPrecisionType::Eightieth;

	/* Resolution scaling applied to the calculated depth data. Full resolution is more accurate, but requires more disk space to store. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth Options")
	EMetaHumanCaptureDepthResolutionType DepthResolution = EMetaHumanCaptureDepthResolutionType::Full;

private:

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
};