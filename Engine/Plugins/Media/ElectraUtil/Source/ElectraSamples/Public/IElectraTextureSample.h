// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IMediaTextureSample.h"
#include "IMediaTextureSampleConverter.h"
#include "MediaObjectPool.h"
#include "Misc/Timespan.h"
#include "Misc/Timecode.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "Templates/RefCounting.h"

#include "MediaVideoDecoderOutput.h"

#define UE_API ELECTRASAMPLES_API

class IElectraTextureSampleBase
	: public IMediaTextureSample
	, public IMediaPoolable
{
public:
	UE_API void Initialize(FVideoDecoderOutput* InVideoDecoderOutput);

	virtual bool IsCacheable() const override
	{
		return true;
	}

#if !UE_SERVER
	UE_API virtual void InitializePoolable() override;
	UE_API virtual void ShutdownPoolable() override;
#endif

	UE_API virtual FIntPoint GetDim() const override;
	UE_API virtual FIntPoint GetOutputDim() const override;

	UE_API virtual FMediaTimeStamp GetTime() const override;
	UE_API virtual FTimespan GetDuration() const override;

	virtual TOptional<FTimecode> GetTimecode() const override
	{
		return Timecode;
	}
	virtual TOptional<FFrameRate> GetFramerate() const override
	{
		return Framerate;
	}

	virtual double GetAspectRatio() const override
	{
		return VideoDecoderOutput->GetAspectRatio();
	}

	virtual EMediaOrientation GetOrientation() const override
	{
		return (EMediaOrientation)VideoDecoderOutput->GetOrientation();
	}

	UE_API virtual bool IsOutputSrgb() const override;
	UE_API virtual const FMatrix& GetYUVToRGBMatrix() const override;
	UE_API virtual bool GetFullRange() const override;

	UE_API virtual FMatrix44f GetSampleToRGBMatrix() const override;
	UE_API virtual const UE::Color::FColorSpace& GetSourceColorSpace() const override;
	UE_API virtual UE::Color::EEncoding GetEncodingType() const override;
	UE_API virtual float GetHDRNitsNormalizationFactor() const override;
	UE_API virtual bool GetDisplayMasteringLuminance(float& OutMin, float& OutMax) const override;
	UE_API virtual TOptional<UE::Color::FColorSpace> GetDisplayMasteringColorSpace() const override;
	UE_API virtual bool GetMaxLuminanceLevels(uint16& OutCLL, uint16& OutFALL) const override;
	UE_API virtual MediaShaders::EToneMapMethod GetToneMapMethod() const override;

	UE_API virtual void SetTime(const FMediaTimeStamp& InTime);

protected:
	virtual float GetSampleDataScale(bool b10Bit) const { return 1.0f; }

	/** Output data from video decoder. */
	TSharedPtr<FVideoDecoderOutput, ESPMode::ThreadSafe> VideoDecoderOutput;

	/** Quick access for some HDR related info */
	TWeakPtr<const IVideoDecoderHDRInformation, ESPMode::ThreadSafe> HDRInfo;
	TWeakPtr<const IVideoDecoderColorimetry, ESPMode::ThreadSafe> Colorimetry;
	/** Optional timecode */
	TWeakPtr<const IVideoDecoderTimecode> DecoderTimecode;
	TOptional<FTimecode> Timecode;
	TOptional<FFrameRate> Framerate;

	/** YUV matrix, adjusted to compensate for decoder output specific scale */
	FMatrix44f SampleToRgbMtx;

	/** YUV to RGB matrix without any adjustments for decoder output specifics */
	const FMatrix* YuvToRgbMtx;

	/** Precomputed colorimetric data */
	UE::Color::EEncoding ColorEncoding;
	UE::Color::FColorSpace SourceColorSpace;
	TOptional<UE::Color::FColorSpace> DisplayMasteringColorSpace;
	float DisplayMasteringLuminanceMin;
	float DisplayMasteringLuminanceMax;
	uint16 MaxCLL;
	uint16 MaxFALL;
};

#undef UE_API
