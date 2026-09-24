// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

namespace UE::TweeningUtilsEditor
{
class TWEENINGUTILSEDITOR_API FTweeningUtilsCommands : public TCommands<FTweeningUtilsCommands>
{
public:
	
	TSharedPtr<FUICommandInfo> SetControlsToTween;
	TSharedPtr<FUICommandInfo> SetTweenPushPull;
	TSharedPtr<FUICommandInfo> SetTweenBlendNeighbor;
	TSharedPtr<FUICommandInfo> SetTweenBlendRelative;
	TSharedPtr<FUICommandInfo> SetTweenBlendEase;
	TSharedPtr<FUICommandInfo> SetTweenSmoothRough;
	TSharedPtr<FUICommandInfo> SetTweenTimeOffset;
	TSharedPtr<FUICommandInfo> ToggleOvershootMode;

	TSharedPtr<FUICommandInfo> DragAnimSliderTool;
	TSharedPtr<FUICommandInfo> ChangeAnimSliderTool;

	FTweeningUtilsCommands();

	//~ Begin TCommands Interface
	virtual void RegisterCommands() override;
	//~ End TCommands Interface
};
}

