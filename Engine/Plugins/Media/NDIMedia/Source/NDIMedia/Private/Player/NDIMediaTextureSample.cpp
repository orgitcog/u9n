// Copyright Epic Games, Inc. All Rights Reserved.

#include "NDIMediaTextureSample.h"

#include "NDIMediaAPI.h"
#include "NDIMediaTextureSampleConverter.h"

bool FNDIMediaTextureSample::Initialize(const NDIlib_video_frame_v2_t& InVideoFrame, const UE::MediaIOCore::FColorFormatArgs& InColorFormatArgs, const FTimespan& InTime, const TOptional<FTimecode>& InTimecode)
{
	bIsCustomFormat = false;
	bIsProgressive = true;
	FieldIndex = 0;
	
	int32 FrameBufferSize;
	EMediaTextureSampleFormat FrameSampleFormat;

	if (InVideoFrame.FourCC == NDIlib_FourCC_video_type_UYVY)
	{
		FrameBufferSize = InVideoFrame.line_stride_in_bytes * InVideoFrame.yres;
		FrameSampleFormat = EMediaTextureSampleFormat::CharUYVY;
	}
	else if (InVideoFrame.FourCC == NDIlib_FourCC_video_type_BGRA)
	{
		FrameBufferSize = InVideoFrame.line_stride_in_bytes * InVideoFrame.yres;
		FrameSampleFormat = EMediaTextureSampleFormat::CharBGRA;
	}
	else if (InVideoFrame.FourCC == NDIlib_FourCC_video_type_RGBA
		|| InVideoFrame.FourCC == NDIlib_FourCC_video_type_RGBX)
	{
		FrameBufferSize = InVideoFrame.line_stride_in_bytes * InVideoFrame.yres;
		FrameSampleFormat = EMediaTextureSampleFormat::CharRGBA;
	}
	else if (InVideoFrame.FourCC == NDIlib_FourCC_video_type_UYVA)
	{
		// UYVA format needs a custom converter.
		bIsCustomFormat = true;
		FrameBufferSize = InVideoFrame.line_stride_in_bytes * InVideoFrame.yres + InVideoFrame.xres*InVideoFrame.yres;
		FrameSampleFormat = EMediaTextureSampleFormat::CharRGBA; // Resulting texture needs to be RGBA.
	}
	else
	{
		return false;
	}

	// Allocate a custom sample converter if needed.
	if (!CustomConverter && bIsCustomFormat)
	{
		CustomConverter = MakeShared<FNDIMediaTextureSampleConverter>();
	}

	switch (InVideoFrame.frame_format_type)
	{
		case NDIlib_frame_format_type_progressive:
			bIsProgressive = true;
			return Super::Initialize(InVideoFrame.p_data
				, FrameBufferSize
				, InVideoFrame.line_stride_in_bytes
				, InVideoFrame.xres
				, InVideoFrame.yres
				, FrameSampleFormat
				, InTime
				, FFrameRate(InVideoFrame.frame_rate_N, InVideoFrame.frame_rate_D)
				, InTimecode
				, InColorFormatArgs);

		case NDIlib_frame_format_type_field_0:
		case NDIlib_frame_format_type_field_1:
			bIsProgressive = false;
			FieldIndex = InVideoFrame.frame_format_type == NDIlib_frame_format_type_field_0 ? 0 : 1;
		
		return Super::InitializeWithEvenOddLine(InVideoFrame.frame_format_type == NDIlib_frame_format_type_field_0
			, InVideoFrame.p_data
			, FrameBufferSize
			, InVideoFrame.line_stride_in_bytes
			, InVideoFrame.xres
			, InVideoFrame.yres
			, FrameSampleFormat
			, InTime
			, FFrameRate(InVideoFrame.frame_rate_N, InVideoFrame.frame_rate_D)
			, InTimecode
			, InColorFormatArgs);

		default:
			return false;
	}
}

IMediaTextureSampleConverter* FNDIMediaTextureSample::GetMediaTextureSampleConverter()
{
	if (bIsCustomFormat)
	{
		return CustomConverter.Get();
	}
	return Super::GetMediaTextureSampleConverter();
}
