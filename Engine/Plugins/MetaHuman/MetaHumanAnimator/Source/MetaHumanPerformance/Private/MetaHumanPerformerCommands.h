// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FMetaHumanPerformanceCommands
	: public TCommands<FMetaHumanPerformanceCommands>
{
public:
	FMetaHumanPerformanceCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> StartProcessingShot;
	TSharedPtr<FUICommandInfo> CancelProcessingShot;
	TSharedPtr<FUICommandInfo> ExportAnimation;
	TSharedPtr<FUICommandInfo> ExportLevelSequence;
	TSharedPtr<FUICommandInfo> ToggleRig;
	TSharedPtr<FUICommandInfo> ToggleFootage;
	TSharedPtr<FUICommandInfo> ToggleControlRigDisplay;
	TArray<TSharedPtr<FUICommandInfo>> ViewSetupStore;
	TArray<TSharedPtr<FUICommandInfo>> ViewSetupRestore;
	TSharedPtr<FUICommandInfo> ToggleShowFramesAsTheyAreProcessed;
};