// Copyright Epic Games, Inc. All Rights Reserved.

#if AVCODECS_USE_METAL

#include "Video/Resources/Metal/VideoResourceMetal.h"

THIRD_PARTY_INCLUDES_START
#include <VideoToolbox/VideoToolbox.h>
#include <CoreMedia/CMSync.h>
THIRD_PARTY_INCLUDES_END

REGISTER_TYPEID(FVideoContextMetal);
REGISTER_TYPEID(FVideoResourceMetal);

static TAVResult<EVideoFormat> ConvertFormat(MTL::PixelFormat Format)
{
	switch (Format)
	{
		case MTL::PixelFormatBGRA8Unorm:
		case MTL::PixelFormatBGRA8Unorm_sRGB:
			return EVideoFormat::BGRA;
		case MTL::PixelFormatBGR10A2Unorm:
			return EVideoFormat::ABGR10;
		default:
			return FAVResult(EAVResult::ErrorUnsupported, FString::Printf(TEXT("MTL::PixelFormat format %d is not supported"), Format), TEXT("Metal"));
	}
}

FVideoContextMetal::FVideoContextMetal(MTL::Device* Device)
	: Device(Device)
{
}

FVideoDescriptor FVideoResourceMetal::GetDescriptorFrom(TSharedRef<FAVDevice> const& Device, MTL::Texture* Raw)
{
    uint32_t Width = Raw->width();
    uint32_t Height = Raw->height();
    TAVResult<EVideoFormat> ConvertedFormat = ConvertFormat(Raw->pixelFormat());
    
	return FVideoDescriptor(ConvertedFormat, Width, Height);
}

FVideoResourceMetal::FVideoResourceMetal(TSharedRef<FAVDevice> const& Device, MTL::Texture* Raw, FAVLayout const& Layout)
	: TVideoResource(Device, Layout, GetDescriptorFrom(Device, Raw))
	, Raw(Raw)
{
}

FVideoResourceMetal::~FVideoResourceMetal()
{
}

FAVResult FVideoResourceMetal::CopyFrom(CVPixelBufferRef Other)
{	
	if (const CVReturn Result = CVPixelBufferLockBaseAddress(Other, 0); Result != kCVReturnSuccess)
	{
		return FAVResult(EAVResult::Error, TEXT("Failed to lock input pixel buffer!"), TEXT("Metal"));
	}
	
	Raw->replaceRegion(MTL::Region(0, 0, GetDescriptor().Width, GetDescriptor().Height), 0, 0, CVPixelBufferGetBaseAddress(Other), CVPixelBufferGetBytesPerRow(Other), 0);
	
	CVPixelBufferUnlockBaseAddress(Other, 0);
	
	return EAVResult::Success;
}

FAVResult FVideoResourceMetal::Validate() const
{
	if (!Raw)
	{
		return FAVResult(EAVResult::ErrorInvalidState, TEXT("Raw resource is invalid"), TEXT("Metal"));
	}

	return EAVResult::Success;
}

#endif
