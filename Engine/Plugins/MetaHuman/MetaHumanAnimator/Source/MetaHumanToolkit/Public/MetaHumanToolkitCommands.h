// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

/** Commands used by the base MetaHuman tookit */
class METAHUMANTOOLKIT_API FMetaHumanToolkitCommands
	: public TCommands<FMetaHumanToolkitCommands>
{
public:
	FMetaHumanToolkitCommands();

	//~Begin TCommands<> interface
	virtual void RegisterCommands() override;
	//~End TCommands<> interface

public:
	TSharedPtr<FUICommandInfo> ViewMixToSingle;
	TSharedPtr<FUICommandInfo> ViewMixToWipe;
	TSharedPtr<FUICommandInfo> ViewMixToDual;

	TSharedPtr<FUICommandInfo> ToggleSingleViewToA;
	TSharedPtr<FUICommandInfo> ToggleSingleViewToB;

	TSharedPtr<FUICommandInfo> ToggleRGBChannel;
	TSharedPtr<FUICommandInfo> ToggleCurves;
	TSharedPtr<FUICommandInfo> ToggleControlVertices;
	TSharedPtr<FUICommandInfo> ToggleDepthMesh;
	TSharedPtr<FUICommandInfo> ToggleUndistortion;
};