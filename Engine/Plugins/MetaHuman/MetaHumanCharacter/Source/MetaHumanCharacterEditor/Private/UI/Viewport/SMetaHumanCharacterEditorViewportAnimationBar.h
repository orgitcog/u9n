// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Widgets/SCompoundWidget.h"
#include "ITransportControl.h"  // EPlaybackMode::Type

class SMetaHumanCharacterEditorViewportAnimationBar: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorViewportAnimationBar) {}
		SLATE_ARGUMENT(TSharedPtr<class FMetaHumanCharacterViewportClient>, AnimationBarViewportClient)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedRef<SWidget> MakeAnimationBarScrubber();

	TSharedPtr<class FMetaHumanCharacterViewportClient> ViewportClient;

private:
	class AMetaHumanInvisibleDrivingActor* GetInvisibleDrivingActor() const;
	bool bAnimationPlaying;

	// ScrubWidget
	EPlaybackMode::Type GetPlaybackMode() const;
	float GetScrubValue() const;
	uint32 GetNumberOfKeys() const;
	float GetSequenceLength() const;
	bool IsScrubWidgetEnabled() const;
	EVisibility WarningVisibility() const;

	void OnValueChanged(float NewValue);
	void OnBeginSliderMovement();
	void OnEndSliderMovement(float NewValue);

	// Notifiers 
	FReply OnClick_Forward();
	FReply OnClick_Backward();

	TSharedRef<SWidget> OnCreateStopButtonWidget();
};