// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/VideoDeviceThumbnailExtractor.h"
#include "ImageUtils.h"
#include "MediaSample.h"
#include "Engine/Texture2D.h"
#include "CaptureManagerMediaRWModule.h"
#include "VideoLiveLinkDeviceLog.h"

#include "Utils/MediaPixelFormatConversions.h"

DEFINE_LOG_CATEGORY(LogVideoLiveLinkDevice);


namespace UE::CaptureManager
{


FVideoDeviceThumbnailExtractor::FVideoDeviceThumbnailExtractor()
{
}

TArray<FColor> FVideoDeviceThumbnailExtractor::ExtractThumbnail(UE::CaptureManager::FMediaTextureSample* InSample)
{
	EMediaTexturePixelFormat SampleFormat = InSample->CurrentFormat;
	TArray<uint8>& Buffer = InSample->Buffer;

	TArray<FColor> ThumbnailRawColorData;
	switch (SampleFormat)
	{
	case EMediaTexturePixelFormat::U8_Mono:
	{
		ThumbnailRawColorData.Reserve(Buffer.Num());
		for (uint8& Value : Buffer)
		{
			ThumbnailRawColorData.Add(FColor(Value, Value, Value));
		}
		break;
	}
	case EMediaTexturePixelFormat::U8_RGB:
	{
		ThumbnailRawColorData.Reserve(Buffer.Num() / 3);
		for (int32 ValueIndex = 0; ValueIndex < Buffer.Num(); ValueIndex += 3)
		{
			ThumbnailRawColorData.Add(FColor(Buffer[ValueIndex], Buffer[ValueIndex + 1], Buffer[ValueIndex + 2]));
		}
		break;
	}
	case EMediaTexturePixelFormat::U8_BGR:
	{
		ThumbnailRawColorData.Reserve(Buffer.Num() / 3);
		for (int32 ValueIndex = 0; ValueIndex < Buffer.Num(); ValueIndex += 3)
		{
			ThumbnailRawColorData.Add(FColor(Buffer[ValueIndex + 2], Buffer[ValueIndex + 1], Buffer[ValueIndex]));
		}
		break;
	}
	case EMediaTexturePixelFormat::U8_RGBA:
	{
		ThumbnailRawColorData.Reserve(Buffer.Num() / 4);
		for (int32 ValueIndex = 0; ValueIndex < Buffer.Num(); ValueIndex += 4)
		{
			ThumbnailRawColorData.Add(FColor(Buffer[ValueIndex], Buffer[ValueIndex + 1], Buffer[ValueIndex + 2], Buffer[ValueIndex + 3]));
		}
		break;
	}
	case EMediaTexturePixelFormat::U8_BGRA:
	{
		ThumbnailRawColorData.Reserve(Buffer.Num() / 4);
		for (int32 ValueIndex = 0; ValueIndex < Buffer.Num(); ValueIndex += 4)
		{
			ThumbnailRawColorData.Add(FColor(Buffer[ValueIndex + 2], Buffer[ValueIndex + 1], Buffer[ValueIndex], Buffer[ValueIndex + 3]));
		}
		break;
	}
	case EMediaTexturePixelFormat::U8_I420:
		ThumbnailRawColorData = UE::CaptureManager::UEConvertI420ToBGRA(InSample);
		break;
	case EMediaTexturePixelFormat::U8_NV12:
		ThumbnailRawColorData = UE::CaptureManager::UEConvertNV12ToBGRA(InSample);
		break;
	case EMediaTexturePixelFormat::U8_YUY2:
		ThumbnailRawColorData = UE::CaptureManager::UEConvertYUY2ToBGRA(InSample);
		break;
	default:
	{
		UE_LOG(LogVideoLiveLinkDevice, Warning, TEXT("Unsupported image format"));
	}
	}

	InSample->Buffer.Empty();
	return ThumbnailRawColorData;
}

}