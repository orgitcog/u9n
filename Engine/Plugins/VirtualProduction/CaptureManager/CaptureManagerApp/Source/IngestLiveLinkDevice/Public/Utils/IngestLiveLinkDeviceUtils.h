// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureManagerTakeMetadata.h"
#include "Ingest/IngestCapability_Options.h"
#include "MediaSample.h"

namespace UE::CaptureManager
{
namespace Private
{
EMediaTexturePixelFormat ConvertPixelFormat(EIngestCapability_ImagePixelFormat InImagePixelFormat);
EMediaOrientation ConvertImageRotation(EIngestCapability_ImageRotation InImageRotation);
}

INGESTLIVELINKDEVICE_API FString ErrorOriginToString(FTakeMetadataParserError::EOrigin InOrigin);

}
