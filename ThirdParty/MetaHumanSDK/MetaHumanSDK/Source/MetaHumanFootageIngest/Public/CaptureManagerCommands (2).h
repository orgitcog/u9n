// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FCaptureManagerCommands
	: public TCommands<FCaptureManagerCommands>
{
public:

	FCaptureManagerCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> Save;
	TSharedPtr<FUICommandInfo> SaveAll;
	TSharedPtr<FUICommandInfo> Refresh;

	TSharedPtr<FUICommandInfo> StartStopCapture;
};