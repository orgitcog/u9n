// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanMediaSourceCreateParams.h"

#include "Widgets/SCompoundWidget.h"



class METAHUMANLOCALLIVELINKSOURCE_API SMetaHumanMediaSourceWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMetaHumanMediaSourceWidget) {}
	SLATE_END_ARGS()

	enum EMediaType
	{
		Video = 0,
		Audio,
		VideoAndAudio
	};

	void Construct(const FArguments& InArgs, EMediaType InMediaType = EMediaType::VideoAndAudio);

	FMetaHumanMediaSourceCreateParams GetCreateParams() const;
	bool CanCreate() const;

	enum EWidgetType
	{
		VideoDevice = 0,
		VideoTrack,
		VideoTrackFormat,
		AudioDevice,
		AudioTrack,
		AudioTrackFormat,
		Filtered,
		StartTimeout,
		FormatWaitTime,
		SampleTimeout,
	};

	TSharedPtr<SWidget> GetWidget(EWidgetType InWidgetType) const;

	void Repopulate();

private:

	TSharedPtr<class SMetaHumanMediaSourceWidgetImpl> Impl;
};
