// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "UObject/SoftObjectPath.h"

#include "Templates/ValueOrError.h"

#include "MetaHumanCalibrationGeneratorOptions.generated.h"

/** Options that will used as part of the camera calibration process */
UCLASS(BlueprintType, Blueprintable)
class UMetaHumanCalibrationGeneratorOptions
	: public UObject
{
public:

	GENERATED_BODY()

	/** Name of the Camera Calibration asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options", meta = (ContentDir))
	FString AssetName = TEXT("CC_Calibration");

	/** Content Browser path where the Lens Files and Camera Calibration assets will be created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options", meta = (ContentDir))
	FDirectoryPath PackagePath;

	/** Automatically save created assets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bAutoSaveAssets = true;

	/** Rate at which the camera calibration process will sample frames.
	* 
	* Example: 30 will use every 30th frame.
	* 
	* Note: Low sample rates will take longer for processing to complete.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	int32 SampleRate = 30;

	/** The width of the checkerboard used to record the calibration footage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	int32 BoardPatternWidth = 15;

	/** The width of the checkerboard used to record the calibration footage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	int32 BoardPatternHeight = 10;

	/** The square size of the checkerboard used to record the calibration footage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options", meta=(Units = "Centimeters"))
	float BoardSquareSize = 0.75f;

	/** Value represents the allowed blurriness (in pixels) of the frame that will be used for calibration process.
	* If the frame has estimated blurriness higher than this threshold, the frame will be discarded.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	float SharpnessThreshold = 5.0f;

	TValueOrError<void, FString> CheckOptionsValidity() const;
};