// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FDataLinkGraphCommands : public TCommands<FDataLinkGraphCommands>
{
public:
	FDataLinkGraphCommands();

	//~ Begin TCommands
	virtual void RegisterCommands() override;
	//~ End TCommands

	TSharedPtr<FUICommandInfo> Compile;

	TSharedPtr<FUICommandInfo> Preview;

	TSharedPtr<FUICommandInfo> ClearPreviewOutput;

	TSharedPtr<FUICommandInfo> ClearPreviewCache;
};
