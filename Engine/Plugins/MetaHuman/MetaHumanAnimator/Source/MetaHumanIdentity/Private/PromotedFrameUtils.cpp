// Copyright Epic Games, Inc. All Rights Reserved.

#include "PromotedFrameUtils.h"
#include "CaptureData.h"
#include "LandmarkConfigIdentityHelper.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanContourDataVersion.h"
#include "CameraCalibration.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImgMediaSource.h"
#include "ImageSequenceUtils.h"

#include "UObject/UObjectGlobals.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"


// TODO: This is a copy-extract from Identity toolkit footage flow. Unify as part of scripting mesh flow
bool UPromotedFrameUtils::InitializeContourDataForFootageFrame(UMetaHumanIdentityPose* InPose, UMetaHumanIdentityFootageFrame* InFootageFrame)
{
	bool bInitalized = false;
	if (UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(InPose->GetCaptureData()))
	{
		if (InPose->GetIsFrameValid(InFootageFrame->FrameNumber) == UMetaHumanIdentityPose::ECurrentFrameValid::Valid)
		{
			EIdentityPoseType PoseType = InPose->PoseType;
			FLandmarkConfigIdentityHelper ConfigHelper;
			ECurvePresetType CurvePreset = ConfigHelper.GetCurvePresetFromIdentityPose(PoseType);

			const FIntPoint TextureResolution = FootageCaptureData->GetFootageColorResolution();
			FFrameTrackingContourData ContourData = ConfigHelper.GetDefaultContourDataFromConfig(FVector2D(TextureResolution.X, TextureResolution.Y), CurvePreset);

			if (!ContourData.TrackingContours.IsEmpty())
			{
				const FString ConfigVersion = FMetaHumanContourDataVersion::GetContourDataVersionString();
				InFootageFrame->InitializeMarkersFromParsedConfig(ContourData, ConfigVersion);
				bInitalized = true;
			}
		}
	}
	return bInitalized;
}

bool UPromotedFrameUtils::GetPromotedFrameAsPixelArrayFromDisk(const FString& InImagePath, FIntPoint& OutImageSize, TArray<FColor>& OutLocalSamples)
{
	if (UTexture2D* LoadedTex = GetBGRATextureFromFile(InImagePath))
	{
		FTexture2DMipMap& Mip0 = LoadedTex->GetPlatformData()->Mips[0];
		if (FColor* TextureData = (FColor*)Mip0.BulkData.Lock(LOCK_READ_ONLY))
		{
			for (int32 Index = 0; Index < LoadedTex->GetSizeX() * LoadedTex->GetSizeY(); ++Index)
			{
				OutLocalSamples.Add(TextureData[Index]);
			}

			Mip0.BulkData.Unlock();
			OutImageSize = FIntPoint{ LoadedTex->GetSizeX(), LoadedTex->GetSizeY() };
			return true;
		}
	}

	return false;
	
}

UTexture2D* UPromotedFrameUtils::GetBGRATextureFromFile(const FString& InFilePath)
{
	TArray<uint8> FileRawData;
	if (FFileHelper::LoadFileToArray(FileRawData, *InFilePath))
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		for (EImageFormat ImageFormat : {EImageFormat::PNG, EImageFormat::JPEG})
		{
			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
			if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(FileRawData.GetData(), FileRawData.Num()))
			{
				TArray<uint8> ImageWrapperData;
				// GetRaw will return the data in the format we request it in
				if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, ImageWrapperData))
				{
					UTexture2D* TransientTex = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight());
					FTexture2DMipMap& Mip0 = TransientTex->GetPlatformData()->Mips[0];
					if (FColor* TextureData = (FColor*)Mip0.BulkData.Lock(LOCK_READ_WRITE))
					{
						FMemory::Memcpy(TextureData, ImageWrapperData.GetData(), ImageWrapperData.Num());
						Mip0.BulkData.Unlock();
	
						TransientTex->UpdateResource();
						return TransientTex;
					}
				}
			}
		}
	}

	return nullptr;
}

UTexture2D* UPromotedFrameUtils::GetDepthTextureFromFile(const FString& InFilePath)
{
	TArray<uint8> RawFileData;
	if (FFileHelper::LoadFileToArray(RawFileData, *InFilePath))
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);
		if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
		{
			if (ImageWrapper->GetBitDepth() == 32 && ImageWrapper->GetFormat() == ERGBFormat::GrayF)
			{
				TArray<uint8> IntData;
				if (ImageWrapper->GetRaw(ERGBFormat::GrayF, 32, IntData))
				{
					TArray<float> DepthData;
					DepthData.SetNumUninitialized(ImageWrapper->GetWidth() * ImageWrapper->GetHeight());
					const float* FloatData = static_cast<const float*>((void*)IntData.GetData());

					FMemory::Memcpy(&DepthData[0], FloatData, ImageWrapper->GetWidth() * ImageWrapper->GetHeight() * sizeof(float));

					UTexture2D* Texture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), EPixelFormat::PF_R32_FLOAT);
					FTexture2DMipMap& DepthMip0 = Texture->GetPlatformData()->Mips[0];
					if (float* DepthTextureData = (float*)DepthMip0.BulkData.Lock(LOCK_READ_WRITE))
					{
						FMemory::Memcpy(DepthTextureData, DepthData.GetData(), DepthData.Num() * sizeof(float));
						DepthMip0.BulkData.Unlock();

						Texture->UpdateResource();
						return Texture;
					}
				}
			}
		}
	}

	return nullptr;
}

FString UPromotedFrameUtils::GetImagePathForFrame(const UFootageCaptureData* InFootageCaptureData, const FString& InCamera, const int32 InFrameId, bool bInIsImageSequence, ETimecodeAlignment InAlignment)
{
	check(InFootageCaptureData && InFootageCaptureData->IsInitialized(UCaptureData::EInitializedCheck::Full));
	
	int32 ViewIndex = -1;

	if (InFootageCaptureData)
	{
		ViewIndex = InFootageCaptureData->GetViewIndexByCameraName(InCamera);
	}

	check(ViewIndex >= 0 && ViewIndex < InFootageCaptureData->ImageSequences.Num() && ViewIndex < InFootageCaptureData->DepthSequences.Num());

	FString CurrentImagePath;
	UImgMediaSource* MediaSequence = bInIsImageSequence ? InFootageCaptureData->ImageSequences[ViewIndex]  : InFootageCaptureData->DepthSequences[ViewIndex];
	TArray<FString> FrameImageNames;
	FImageSequenceUtils::GetImageSequenceFilesFromPath(MediaSequence->GetFullPath(), FrameImageNames);
	
	const int32 FrameNumber = IdentityFrameNumberToImageSequenceFrameNumber(InFootageCaptureData, InCamera, InFrameId, bInIsImageSequence, InAlignment);
	
	if (FrameImageNames.IsValidIndex(FrameNumber))
	{
		CurrentImagePath = MediaSequence->GetFullPath() / FrameImageNames[FrameNumber];
	}
	
	return CurrentImagePath;
}

int32 UPromotedFrameUtils::IdentityFrameNumberToImageSequenceFrameNumber(const UFootageCaptureData* InFootageCaptureData, const FString& InCamera, const int32 InFrameId, bool bInIsImageSequence, ETimecodeAlignment InAlignment)
{
	check(InFootageCaptureData && InFootageCaptureData->IsInitialized(UCaptureData::EInitializedCheck::Full));

	int32 ViewIndex = -1;

	if (InFootageCaptureData)
	{
		ViewIndex = InFootageCaptureData->GetViewIndexByCameraName(InCamera);
	}

	check(ViewIndex >= 0 && ViewIndex < InFootageCaptureData->ImageSequences.Num() && ViewIndex < InFootageCaptureData->DepthSequences.Num());

	UImgMediaSource* MediaSequence = bInIsImageSequence ? InFootageCaptureData->ImageSequences[ViewIndex] : InFootageCaptureData->DepthSequences[ViewIndex];
	
	TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>> MediaFrameRanges;
	TRange<FFrameNumber> ProcessingLimitFrameRange, MaxFrameRange;
	InFootageCaptureData->GetFrameRanges(MediaSequence->FrameRateOverride, InAlignment, false, MediaFrameRanges, ProcessingLimitFrameRange, MaxFrameRange);
	
	return InFrameId - MediaFrameRanges[MediaSequence].GetLowerBoundValue().Value;
}
