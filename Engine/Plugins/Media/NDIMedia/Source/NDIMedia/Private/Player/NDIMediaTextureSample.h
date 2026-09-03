// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreTextureSampleBase.h"

class FNDIMediaTextureSampleConverter;
struct NDIlib_video_frame_v2_t;

/**
 * Implements a media texture sample for NDIMedia.
 */
class FNDIMediaTextureSample : public FMediaIOCoreTextureSampleBase
{
	using Super = FMediaIOCoreTextureSampleBase;

public:
	//~ Begin IMediaTextureSample
#if WITH_ENGINE
	virtual IMediaTextureSampleConverter* GetMediaTextureSampleConverter() override;
#endif
	//~ End IMediaTextureSample

	/**
	 * Initialize the sample.
	 * 
	 * @param InVideoFrame Received Video frame data from NDI.
	 * @param InColorFormatArgs Color space and encoding settings from the media source.
	 * @param InTime Current playback time of the sample.
	 * @return true if the sample was correctly initialized, false otherwise.
	 */
	bool Initialize(const NDIlib_video_frame_v2_t& InVideoFrame, const UE::MediaIOCore::FColorFormatArgs& InColorFormatArgs, const FTimespan& InTime, const TOptional<FTimecode>& InTimecode);

	/** Progressive vs Interlaced. */
	bool bIsProgressive = true;

	/** If interlaced, which field. */
	int32 FieldIndex = 0;

	/** Needs a custom conversion. */
	bool bIsCustomFormat = false;

	/** Custom converter. */
	TSharedPtr<FNDIMediaTextureSampleConverter> CustomConverter;
};
