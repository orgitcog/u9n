// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Math/Color.h"


namespace UE::CaptureManager 
{
struct FMediaTextureSample;

class VIDEOLIVELINKDEVICECOMMON_API FVideoDeviceThumbnailExtractor
{
public:
	FVideoDeviceThumbnailExtractor();
	TArray<FColor> ExtractThumbnail(FMediaTextureSample* Sample);
};

}