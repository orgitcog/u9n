// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

#include "Framework/Commands/UICommandInfo.h"

namespace UE::MediaViewer::Private
{

class FMediaViewerCommands : public TCommands<FMediaViewerCommands>
{
public:
	FMediaViewerCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> ToggleOverlay;

	TSharedPtr<FUICommandInfo> MoveLeft;
	TSharedPtr<FUICommandInfo> MoveRight;
	TSharedPtr<FUICommandInfo> MoveUp;
	TSharedPtr<FUICommandInfo> MoveDown;
	TSharedPtr<FUICommandInfo> MoveForward;
	TSharedPtr<FUICommandInfo> MoveBackward;

	TSharedPtr<FUICommandInfo> RotatePlusYaw;
	TSharedPtr<FUICommandInfo> RotateMinusYaw;
	TSharedPtr<FUICommandInfo> RotatePlusPitch;
	TSharedPtr<FUICommandInfo> RotateMinusPitch;
	TSharedPtr<FUICommandInfo> RotatePlusRoll;
	TSharedPtr<FUICommandInfo> RotateMinusRoll;
	
	TSharedPtr<FUICommandInfo> Scale12;
	TSharedPtr<FUICommandInfo> Scale25;
	TSharedPtr<FUICommandInfo> Scale50;
	TSharedPtr<FUICommandInfo> Scale100;
	TSharedPtr<FUICommandInfo> Scale200;
	TSharedPtr<FUICommandInfo> Scale400;
	TSharedPtr<FUICommandInfo> Scale800;

	TSharedPtr<FUICommandInfo> ScaleToFit;

	TSharedPtr<FUICommandInfo> CopyTransform;
	TSharedPtr<FUICommandInfo> ResetTransform;
	TSharedPtr<FUICommandInfo> ResetAllTransforms;
	TSharedPtr<FUICommandInfo> ToggleLockedTransform;

	TSharedPtr<FUICommandInfo> MipMinus;
	TSharedPtr<FUICommandInfo> MipPlus;

	TSharedPtr<FUICommandInfo> SecondImageOpacityMinus;
	TSharedPtr<FUICommandInfo> SecondImageOpacityPlus;

	TSharedPtr<FUICommandInfo> CopyColor;

	TSharedPtr<FUICommandInfo> AddToLibrary;

	TSharedPtr<FUICommandInfo> SwapAB;
};

} // UE::MediaViewer::Private
