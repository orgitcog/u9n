// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCommonEditorViewportToolbarBase.h"

/// Common code for toolbars in the Dataflow Editor
class SDataflowEditorViewportToolBarBase : public SCommonEditorViewportToolbarBase
{
protected:
	TSharedRef<SWidget> GenerateCameraSpeedSettingsMenu() const;
	FText GetCameraSpeedLabel() const;
	float GetCamSpeedSliderPosition() const;
	void OnSetCamSpeed(float NewValue) const;
	float GetCamSpeedScalarBoxValue() const;
	void OnSetCamSpeedScalarBoxValue(float NewValue) const;
	TSharedRef<SWidget> GenerateOptionsMenu() const;
};

