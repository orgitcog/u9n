// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/IngestLiveLinkDeviceUtils.h"

namespace UE::CaptureManager
{

namespace Private
{

EMediaTexturePixelFormat ConvertPixelFormat(EIngestCapability_ImagePixelFormat InImagePixelFormat)
{
	switch (InImagePixelFormat)
	{
		case EIngestCapability_ImagePixelFormat::U8_RGB:
			return EMediaTexturePixelFormat::U8_RGB;
		case EIngestCapability_ImagePixelFormat::U8_BGR:
			return EMediaTexturePixelFormat::U8_BGR;
		case EIngestCapability_ImagePixelFormat::U8_RGBA:
			return EMediaTexturePixelFormat::U8_RGBA;
		case EIngestCapability_ImagePixelFormat::U8_BGRA:
			return EMediaTexturePixelFormat::U8_BGRA;
		case EIngestCapability_ImagePixelFormat::U8_I444:
			return EMediaTexturePixelFormat::U8_I444;
		case EIngestCapability_ImagePixelFormat::U8_I420:
			return EMediaTexturePixelFormat::U8_I420;
		case EIngestCapability_ImagePixelFormat::U8_NV12:
			return EMediaTexturePixelFormat::U8_NV12;
		case EIngestCapability_ImagePixelFormat::U8_Mono:
			return EMediaTexturePixelFormat::U8_Mono;
		case EIngestCapability_ImagePixelFormat::U16_Mono:
			return EMediaTexturePixelFormat::U16_Mono;
		case EIngestCapability_ImagePixelFormat::F_Mono:
			return EMediaTexturePixelFormat::F_Mono;
		case EIngestCapability_ImagePixelFormat::Undefined:
		default:
			check(false);
			return EMediaTexturePixelFormat::Undefined;
	}
}

EMediaOrientation ConvertImageRotation(EIngestCapability_ImageRotation InImageRotation)
{
	switch (InImageRotation)
	{
		case EIngestCapability_ImageRotation::CW_90:
			return EMediaOrientation::CW90;
		case EIngestCapability_ImageRotation::CW_180:
			return EMediaOrientation::CW180;
		case EIngestCapability_ImageRotation::CW_270:
			return EMediaOrientation::CW270;
		case EIngestCapability_ImageRotation::None:
		default:
			return EMediaOrientation::Original;
	}
}

}

FString ErrorOriginToString(FTakeMetadataParserError::EOrigin InOrigin)
{
	switch (InOrigin)
	{
		case FTakeMetadataParserError::Reader:
			return TEXT("Reader");
		case FTakeMetadataParserError::Validator:
			return TEXT("Validator");
		case FTakeMetadataParserError::Parser:
		default:
			return TEXT("Parser");
	}
}

}