// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Delegates/Delegate.h"
#include "Engine/AssetUserData.h"
#include "SubtitlesAndClosedCaptionsDelegates.generated.h"

#define UE_API ENGINE_API

class FCanvas;
class UAssetUserData;

// Externally-Timed subtitles must be manually added and removed with USubtitlesSubsystem::QueueSubtitle and ::StopSubtitle.
// For the initial delay before becoming visible, use USubtitleAssetUserData::StartOffset instead of this enum.
UENUM()
enum class ESubtitleTiming : uint8
{
	InternallyTimed,
	ExternallyTimed
};

// ESRB rating categories.
UENUM(BlueprintType)
enum class ESRB : uint8
{
	Everyone,
	Everyone10Plus,
	Teen,
	Mature,
	AdultsOnly,
	RatingPending,
	RatingPending17Plus
};

// Subtitle type for type-specific rendering.
UENUM()
enum class ESubtitleType : uint8
{
	Subtitle,
	ClosedCaption,
	AudioDescription
};

// Minimum duration to display subtitle.
static constexpr float SubtitleMinDuration		= 0.05f;

// Default value to initialize subtitle duration to. Used by SoundWaves to check if they should manually set the duration.
static constexpr float SubtitleDefaultDuration	= 3.f;

/**
 *  Base class for subtitle data being attached to assets
 */
UCLASS(MinimalAPI, BlueprintType)
class USubtitleAssetUserData : public UAssetUserData
{
	GENERATED_BODY()

public:

	// #SUBTITLES_PRD: carried over from FSubtitleCue, still required
	//
	// The text to appear in the subtitle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Subtitles, meta = (Multiline = true))
	FText Text;

	// #SUBTITLES_PRD: carried over from FSubtitleCue, still required
	//
	// Time to display in seconds. 
	// Defaulted to 3 seconds so when adding new subtitles it's not required to enter a placeholder Duration.
	// Duration can be be set by ingestion pipelines when importing Subtitles in bulk
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Subtitles, meta = (ClampMin = 0.05f))
	float Duration = SubtitleDefaultDuration;

	// Some subtitles have a delay before they're allowed to be displayed (primarily from the legacy system).
	// StartOffset measures how long in Seconds, after queuing, before the subtitle is allowed to enter the active subtitles queue.
	// ESubtitleTiming::ExternallyTimed does not effect this initial delay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Subtitles, meta = (ClampMin = 0.f))
	float StartOffset = 0.f;

	// #SUBTITLES_PRD: Priority comes from USoundBase::GetSubtitlePriority, USoundCue::GetSubtitlePriority and USoundWave::GetSubtitlePriority
	// Consolidate various subtitle properties throughout sound/audio code into this new subtitles plugin.
	// Kept the 10000 default in case that's what users are already used to.
	// 
	// The priority of the subtitle.  Defaults to 10000.  Higher values will play instead of lower values.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Subtitles)
	float Priority = 10000.f;

	// ESRB rating category.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Subtitles)
	ESRB Esrb = ESRB::Everyone;

	// Subtitle type for type-specific rendering.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Subtitles)
	ESubtitleType SubtitleType = ESubtitleType::Subtitle;
};


struct FQueueSubtitleParameters
{
	const UAssetUserData& Subtitle;
	TOptional<float> Duration;
};

class FSubtitlesAndClosedCaptionsDelegates
{
public:

	// Have the subtitle subsystem to queue a subtitle to be displayed
	static UE_API TDelegate<void(const FQueueSubtitleParameters&, const ESubtitleTiming)> QueueSubtitle;

	static UE_API TDelegate<bool(const UAssetUserData&)> IsSubtitleActive;

	static UE_API TDelegate<void(const UAssetUserData&)> StopSubtitle;

	static UE_API TDelegate<void()> StopAllSubtitles;
};

#undef UE_API
