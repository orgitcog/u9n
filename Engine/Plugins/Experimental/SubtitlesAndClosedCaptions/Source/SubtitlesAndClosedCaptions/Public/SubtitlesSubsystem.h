// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ActiveSubtitle.h"
#include "Fonts/SlateFontInfo.h"
#include "Math/Color.h"
#include "Subsystems/WorldSubsystem.h"

#include "SubtitleTextBlock.h"

#include "Subtitles/SubtitlesAndClosedCaptionsDelegates.h"
#include "SubtitlesSubsystem.generated.h"

class FCanvas;
class UAssetUserData;
class UFont;
struct FQueueSubtitleParameters;

#if WITH_DEV_AUTOMATION_TESTS
namespace SubtitlesAndClosedCaptions::Test
{
#if 0 // Temporarily disabling these tests as they have a dangling reference that trips up the static analysis on certain build configurations.
	struct FMovieSceneSubtitlesTest;
#endif
	struct FSubtitlesTest;
}
#endif

/*
* #SUBTITLES_PRD -	Requirement:	Ability to allow designers to “script” subtitle location for sequences and scenes to avoid subtitles overlapping important scenes or characters
*					Use a UEngineSubsystem for blueprints
*
*	Game configuration for font customization per game
*/
UCLASS(config = Game, defaultConfig)
class SUBTITLESANDCLOSEDCAPTIONS_API USubtitlesSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	USubtitlesSubsystem() = default;

	// FSubtitlesAndClosedCaptionsDelegates

	// Adds a subtitle to the queue: Params contains the subtitle asset and an optional duration. The highest-priority subtitle in the queue will be displayed.
	// If Timing is ExternallyTimed, the queued subtitle will remain in the queue until manually removed.
	// If the subtitle asset has a non-zero StartOffset, it will sit in a delayed-start queue instead of being queued for display.
	virtual void QueueSubtitle(const FQueueSubtitleParameters& Params, const ESubtitleTiming Timing = ESubtitleTiming::InternallyTimed);

	// Returns true if the given subtitle asset is being displayed.
	virtual bool IsSubtitleActive(const UAssetUserData& Data) const;

	// Stops the given subtitle asset being displayed.  This includes subtitles not yet being displayed due to their StartOffset
	virtual void StopSubtitle(const UAssetUserData& Data);

	// Stops all queued subtitles from being displayed.  This includes subtitles not yet being displayed due to their StartOffset.
	virtual void StopAllSubtitles();

protected:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void BindDelegates();

	UPROPERTY()
	TMap<ESubtitleType, FSlateFontInfo> SubtitleFontInfo;

	// Sorted by priority, desc
	UPROPERTY()
	TArray<FActiveSubtitle> ActiveSubtitles;

	// Unsorted; subtitles with a delayed start offset still need to be tracked before entering the queue proper.
	UPROPERTY()
	TArray<FActiveSubtitle> DelayedSubtitles;

protected:
	virtual void AddActiveSubtitle(const USubtitleAssetUserData& Subtitle, float Duration);
	virtual void MakeDelayedSubtitleActive(TObjectPtr<const USubtitleAssetUserData> Subtitle);
	virtual void RemoveActiveSubtitle(TObjectPtr<const USubtitleAssetUserData> Subtitle);

#if WITH_DEV_AUTOMATION_TESTS
#if 0 // Temporarily disabling these tests as they have a dangling reference that trips up the static analysis on certain build configurations.
	friend struct SubtitlesAndClosedCaptions::Test::FMovieSceneSubtitlesTest;
#endif
	friend struct SubtitlesAndClosedCaptions::Test::FSubtitlesTest;
public:
	const TArray<FActiveSubtitle>& GetActiveSubtitles() const
	{
		return ActiveSubtitles;
	}

	// As this is for testing, assume that the number of subtitles has already been checked.
	const USubtitleAssetUserData* GetTopRankedSubtitle() const
	{
		check(ActiveSubtitles.Num() > 0);
		const FActiveSubtitle& ActiveSubtitle = ActiveSubtitles[0];

		return ActiveSubtitle.Subtitle.Get();
	}

	void TestActivatingDelayedSubtitle(const UAssetUserData& Data)
	{
		const USubtitleAssetUserData* Subtitle = CastChecked<const USubtitleAssetUserData>(&Data);
		MakeDelayedSubtitleActive(Subtitle);
	}
#endif
	//	WITH_DEV_AUTOMATION_TESTS

private:
	UPROPERTY()
	TObjectPtr<USubtitleTextBlock> SubtitleWidget = nullptr;

	bool bInitializedWidget = false;
	bool TryCreateUMGWidget();
	void UpdateWidgetData();
};
