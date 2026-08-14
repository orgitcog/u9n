// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadLiveLinkFaceCameraCalibration.h"

#include "MetaHumanCoreLog.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "JsonObjectConverter.h"
#include "LensFile.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CameraCalibration.h"
#include "Models/SphericalLensModel.h"
#include "EngineLogs.h"
#include "UObject/Package.h"


static bool CreateExtendedLensFile(UObject* InParent, FName InTakeName, const FString& InCameraName, EObjectFlags InFlags, bool InIsDepthCamera, const FLiveLinkFaceCalibrationData& InCalibData,
								   FExtendedLensFile& OutCameraCalibration, FString& OutParsingError)
{
	if (InCalibData.IntrinsicMatrix.Num() != 9)
	{
		OutParsingError = TEXT("IntrinsicMatrix must contain 9 values");
		return false;
	}

	if (InCalibData.IntrinsicMatrixReferenceDimensions.Height <= 0)
	{
		OutParsingError = TEXT("IntrinsicMatrixReferenceDimensions Height must be > 0");
		return false;
	}

	if (InCalibData.IntrinsicMatrixReferenceDimensions.Width <= 0)
	{
		OutParsingError = TEXT("IntrinsicMatrixReferenceDimensions Width must be > 0");
		return false;
	}

	const FString ObjectName = InIsDepthCamera ?
		FString::Printf(TEXT("%s_Depth_LensFile"), *InTakeName.ToString()) :
		FString::Printf(TEXT("%s_RGB_LensFile"), *InTakeName.ToString());
	FString ParentPath = FString::Printf(TEXT("%s/%s"), *FPackageName::GetLongPackagePath(*InParent->GetName()), *ObjectName);
	UObject* Parent = CreatePackage(*ParentPath);

	OutCameraCalibration.Name = InCameraName;
	OutCameraCalibration.IsDepthCamera = InIsDepthCamera;
	OutCameraCalibration.LensFile = NewObject<ULensFile>(Parent, ULensFile::StaticClass(), *ObjectName, InFlags);

	// These a for a non-FIZ camera.
	// Also check if it matters if the lens sensor dimensions and serial number
	// Are these completely arbitrary in this case ie just used to interpolate values on the various curves or do the actual values matter?
	const float Focus = 0.0f;
	const float Zoom = 0.0f;

	// LensInfo
	OutCameraCalibration.LensFile->LensInfo.LensModel = USphericalLensModel::StaticClass();
	OutCameraCalibration.LensFile->LensInfo.LensModelName = InIsDepthCamera ? FString::Printf(TEXT("%s_TrueDepthCamera"), *InCalibData.DeviceModel) :
		FString::Printf(TEXT("%s_RGBCamera"), *InCalibData.DeviceModel);
	// lens serial number is not needed

	// leave sensor dimensions with default values and de-normalize using VideoDimensions or DepthDimensions
	OutCameraCalibration.LensFile->LensInfo.ImageDimensions = InIsDepthCamera ? FIntPoint(InCalibData.DepthDimensions.Width,
																						  InCalibData.DepthDimensions.Height) : FIntPoint(InCalibData.VideoDimensions.Width, InCalibData.VideoDimensions.Height);

	// FocalLengthInfo
	FFocalLengthInfo FocalLengthInfo;
	FocalLengthInfo.FxFy = FVector2D(InCalibData.IntrinsicMatrix[0] / InCalibData.IntrinsicMatrixReferenceDimensions.Width,
									 InCalibData.IntrinsicMatrix[4] / InCalibData.IntrinsicMatrixReferenceDimensions.Height);

	// DistortionInfo
	// TODO WIP conversion of the distortion model will come as a separate ticket; for now we just set the distortion params to zero
	// When this is addressed the code below to account for rotation will need updating.
	FDistortionInfo DistortionInfo;
	FSphericalDistortionParameters SphericalParameters;

	SphericalParameters.K1 = 0.0f;
	SphericalParameters.K2 = 0.0f;
	SphericalParameters.P1 = 0.0f;
	SphericalParameters.P2 = 0.0f;
	SphericalParameters.K3 = 0.0f;

	USphericalLensModel::StaticClass()->GetDefaultObject<ULensModel>()->ToArray(
		SphericalParameters,
		DistortionInfo.Parameters
	);
	// END WIP

	// ImageCenterInfo
	// When comparing the calculation of RGB intrinsics in iOS it seems that the 0.5f pixel offset is added before
	// moving the principal point. This would mean that the principal point is in pixel coordinates (0,0) is center of top left pixel
	// and not the corner of top left pixel. This is rather confusing as all other coordinate systems
	// on iOS have the origin at the corner of the top left pixel, so unfortunately I don't know what the right values are.
	// This code is consistent with the code in titan repo.
	FImageCenterInfo ImageCenterInfo;
	float ScaleX = float(OutCameraCalibration.LensFile->LensInfo.ImageDimensions.X) / float(InCalibData.IntrinsicMatrixReferenceDimensions.Width);
	float ScaleY = float(OutCameraCalibration.LensFile->LensInfo.ImageDimensions.Y) / float(InCalibData.IntrinsicMatrixReferenceDimensions.Height);
	ImageCenterInfo.PrincipalPoint = FVector2D(((InCalibData.IntrinsicMatrix[2] + 0.5f) * ScaleX - 0.5f) / OutCameraCalibration.LensFile->LensInfo.ImageDimensions.X,
											   ((InCalibData.IntrinsicMatrix[5] + 0.5f) * ScaleY - 0.5f) / OutCameraCalibration.LensFile->LensInfo.ImageDimensions.Y);

	// NodalOffset: for the iPhone the nodal point offset is zero (default) for both RGB and depthmap cameras
	FNodalPointOffset NodalPointOffset;

	// Start of rotation handling
	// Account for the fact that imported images have been rotated (90 clockwise) wrt this calibration.
	// Update the calibration so that its correct for the imported images.
	// Will need to eventually consider distortion too. 
	Swap(OutCameraCalibration.LensFile->LensInfo.ImageDimensions.X, OutCameraCalibration.LensFile->LensInfo.ImageDimensions.Y);
	Swap(OutCameraCalibration.LensFile->LensInfo.SensorDimensions.X, OutCameraCalibration.LensFile->LensInfo.SensorDimensions.Y);
	Swap(FocalLengthInfo.FxFy.X, FocalLengthInfo.FxFy.Y);

	FVector2D UnrotatedPrincipalPoint = ImageCenterInfo.PrincipalPoint;
	ImageCenterInfo.PrincipalPoint.X = 1.0 - UnrotatedPrincipalPoint.Y;
	ImageCenterInfo.PrincipalPoint.Y = UnrotatedPrincipalPoint.X;
	// End of rotation handling

	OutCameraCalibration.LensFile->AddDistortionPoint(Focus, Zoom, DistortionInfo, FocalLengthInfo);
	OutCameraCalibration.LensFile->AddImageCenterPoint(Focus, Zoom, ImageCenterInfo);
	OutCameraCalibration.LensFile->AddNodalOffsetPoint(Focus, Zoom, NodalPointOffset);

	OutCameraCalibration.LensFile->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(OutCameraCalibration.LensFile);

	return true;
}

UCameraCalibration* LoadLiveLinkFaceCameraCalibration(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, const FString& InFilenameOrString, bool bIsFile)
{
	FString JsonString;
	FString ErrorFilename;

	if (bIsFile)
	{
		ErrorFilename = InFilenameOrString;

		if (!FFileHelper::LoadFileToString(JsonString, *InFilenameOrString))
		{
			JsonString = "";
		}
	}
	else
	{
		ErrorFilename = "[String]";

		JsonString = InFilenameOrString;
	}

	FString ErrorText;

	if (!JsonString.IsEmpty())
	{
		FLiveLinkFaceCalibrationData CalibData;

		if (FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &CalibData, 0, 0, true))
		{
			UCameraCalibration* MetaHumanSystemCalibration = NewObject<UCameraCalibration>(InParent, InClass, InName, InFlags);

			// Create the 2 lens files
			FExtendedLensFile RGBCameraCalib;
			FString ParsingErrorRGB;
			bool bValidRGBCalib = CreateExtendedLensFile(InParent, InName, TEXT("iPhone"), InFlags, false, CalibData, RGBCameraCalib, ParsingErrorRGB);
			if (!bValidRGBCalib)
			{
				ErrorText = FString::Printf(TEXT("Failed to parse iPhone camera calibration file '%s', for RGB camera calib; error encountered was: '%s'."), *ErrorFilename, *ParsingErrorRGB);
			}
			else
			{
				FExtendedLensFile DepthmapCameraCalib;
				FString ParsingErrorDepthmap;
				bool bValidDepthmapCalib = CreateExtendedLensFile(InParent, InName, TEXT("Depth"), InFlags, true, CalibData, DepthmapCameraCalib, ParsingErrorDepthmap);
				if (!bValidRGBCalib)
				{
					ErrorText = FString::Printf(TEXT("Failed to parse iPhone camera calibration file '%s', for depthmap camera calib; error encountered was: '%s'."), *ErrorFilename, *ParsingErrorDepthmap);
				}
				else
				{
					MetaHumanSystemCalibration->CameraCalibrations.Add(RGBCameraCalib);
					MetaHumanSystemCalibration->CameraCalibrations.Add(DepthmapCameraCalib);

					return MetaHumanSystemCalibration;
				}
			}
		}
		else
		{
			ErrorText = FString::Printf(TEXT("Failed to parse iPhone camera calibration file '%s'."), *ErrorFilename);
		}
	}
	else
	{
		ErrorText = FString::Printf(TEXT("Failed to read iPhone camera calibration file '%s'."), *ErrorFilename);
	}

	UE_LOG(LogMetaHumanCore, Error, TEXT("%s"), *ErrorText);

	return nullptr;
}
