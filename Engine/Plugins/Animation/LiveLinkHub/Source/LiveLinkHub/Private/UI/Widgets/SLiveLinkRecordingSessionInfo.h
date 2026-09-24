// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"


/** Display and edit the session name, slate name, take number. */
class SLiveLinkRecordingSessionInfo : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkRecordingSessionInfo) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
