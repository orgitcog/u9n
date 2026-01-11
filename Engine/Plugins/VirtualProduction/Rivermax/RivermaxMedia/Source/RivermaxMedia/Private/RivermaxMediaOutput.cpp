// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaOutput.h"

#include "IRivermaxCoreModule.h"
#include "MediaOutput.h"
#include "Misc/FileHelper.h"
#include "RivermaxMediaCapture.h"
#include "RivermaxMediaLog.h"
#include "RivermaxMediaUtils.h"

/* URivermaxMediaOutput
*****************************************************************************/

bool URivermaxMediaOutput::Validate(FString& OutFailureReason) const
{
	if (!Super::Validate(OutFailureReason))
	{
		return false;
	}
	
	return true;
}

FIntPoint URivermaxMediaOutput::GetRequestedSize() const
{
	if (VideoStream.bOverrideResolution)
	{
		return VideoStream.Resolution;
	}

	return UMediaOutput::RequestCaptureSourceSize;
}

EPixelFormat URivermaxMediaOutput::GetRequestedPixelFormat() const
{
	// All output types go through buffer conversion
	EPixelFormat Result = EPixelFormat::PF_A2B10G10R10;
	return Result;
}

EMediaCaptureConversionOperation URivermaxMediaOutput::GetConversionOperation(EMediaCaptureSourceType InSourceType) const
{
	EMediaCaptureConversionOperation Result = EMediaCaptureConversionOperation::CUSTOM;
	switch (VideoStream.PixelFormat)
	{
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_YUV422:
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_YUV422:
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_12BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_FLOAT16_RGB:
	default:
		Result = EMediaCaptureConversionOperation::CUSTOM; //We handle all conversion for rivermax since it's really tied to endianness of 2110
		break;
	}
	return Result;
}

void URivermaxMediaOutput::ExportSDP(const FString& InPath)
{
	TArray<char> SDP;
	UE::RivermaxCore::FRivermaxOutputOptions OutputOptions = GenerateStreamOptions();

	bool bSaved = false;
	if (UE::RivermaxMediaUtils::Private::StreamOptionsToSDPDescription(OutputOptions, SDP))
	{
		FString SdpString(ANSI_TO_TCHAR(SDP.GetData()));

		if (FFileHelper::SaveStringToFile(SdpString, *InPath))
		{
			bSaved = true;
		}
	}

	if (bSaved)
	{
		UE_LOG(LogTemp, Log, TEXT("Saved SDP successfully to: %s"), *InPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save SDP file: %s"), *InPath);
	}
}

UE::RivermaxCore::FRivermaxOutputOptions URivermaxMediaOutput::GenerateStreamOptions() const
{
	using namespace UE::RivermaxCore;
	using namespace UE::RivermaxMediaUtils::Private;

	UE::RivermaxCore::FRivermaxOutputOptions OutOutputOptions;

	// Video configuration
	if (bOutputVideo)
	{
		TSharedPtr<FRivermaxVideoOutputOptions> VideoOptions
			= StaticCastSharedPtr<FRivermaxVideoOutputOptions>(OutOutputOptions.StreamOptions[static_cast<uint8>(ERivermaxStreamType::VIDEO_2110_20_STREAM)]);

		// Sanity check. Making sure that Video Options are not intialized prior.
		check(!VideoOptions.IsValid());
		VideoOptions = MakeShared<FRivermaxVideoOutputOptions>();
		OutOutputOptions.StreamOptions[static_cast<uint8>(ERivermaxStreamType::VIDEO_2110_20_STREAM)] = VideoOptions;

		VideoOptions->InterfaceAddress = VideoStream.InterfaceAddress;
		VideoOptions->StreamAddress = VideoStream.StreamAddress;
		VideoOptions->Port = VideoStream.Port;
		VideoOptions->Resolution = GetRequestedSize();

		VideoOptions->FrameRate = VideoStream.FrameRate;
		OutOutputOptions.NumberOfBuffers = PresentationQueueSize;
		VideoOptions->bUseGPUDirect = VideoStream.bUseGPUDirect;
		OutOutputOptions.AlignmentMode = MediaOutputAlignmentToRivermaxAlignment(AlignmentMode);
		OutOutputOptions.FrameLockingMode = UE::RivermaxMediaUtils::Private::MediaOutputFrameLockingToRivermax(FrameLockingMode);

		// Setup alignment dependent configs
		OutOutputOptions.bDoContinuousOutput = OutOutputOptions.AlignmentMode == ERivermaxAlignmentMode::AlignmentPoint ? bDoContinuousOutput : false;
		OutOutputOptions.bDoFrameCounterTimestamping = OutOutputOptions.AlignmentMode == ERivermaxAlignmentMode::FrameCreation ? bDoFrameCounterTimestamping : false;

		VideoOptions->PixelFormat = MediaOutputPixelFormatToRivermaxSamplingType(VideoStream.PixelFormat);
		const FVideoFormatInfo Info = FStandardVideoFormat::GetVideoFormatInfo(VideoOptions->PixelFormat);
		VideoOptions->AlignedResolution = GetAlignedResolution(Info, VideoOptions->Resolution);
	}

	if (bOutputAnc)
	{
		TSharedPtr<FRivermaxAncOutputOptions> AncOptions
			= StaticCastSharedPtr<FRivermaxAncOutputOptions>(OutOutputOptions.StreamOptions[static_cast<uint8>(ERivermaxStreamType::ANC_2110_40_STREAM)]);
		
		// Sanity check. Making sure that Anc Options are not intialized prior.
		check(!AncOptions.IsValid());
		AncOptions = MakeShared<FRivermaxAncOutputOptions>();
		OutOutputOptions.StreamOptions[static_cast<uint8>(ERivermaxStreamType::ANC_2110_40_STREAM)] = AncOptions;

		AncOptions->InterfaceAddress = AncStream.InterfaceAddress;
		AncOptions->StreamAddress = AncStream.StreamAddress;
		AncOptions->Port = AncStream.Port;
	}

	return MoveTemp(OutOutputOptions);
}

UMediaCapture* URivermaxMediaOutput::CreateMediaCaptureImpl()
{
	UMediaCapture* Result = NewObject<URivermaxMediaCapture>();
	if (Result)
	{
		// The following is preparation for UE5.7
		{
			VideoStream.bOverrideResolution = bOverrideResolution;
			VideoStream.Resolution = Resolution;
			VideoStream.FrameRate = FrameRate;
			VideoStream.PixelFormat = PixelFormat;
			VideoStream.InterfaceAddress = InterfaceAddress;
			VideoStream.StreamAddress = StreamAddress;
			VideoStream.Port = Port;
			VideoStream.bUseGPUDirect = bUseGPUDirect;
		}
		Result->SetMediaOutput(this);
	}
	return Result;
}

#if WITH_EDITOR
bool URivermaxMediaOutput::CanEditChange(const FProperty* InProperty) const
{
	return Super::CanEditChange(InProperty);
}

void URivermaxMediaOutput::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif //WITH_EDITOR

