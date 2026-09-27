// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FGeometryCacheLevelSequenceBakerCommands : public TCommands<FGeometryCacheLevelSequenceBakerCommands>
{
public:
	FGeometryCacheLevelSequenceBakerCommands();

	/** Exports sequence to geometry cache. */
	TSharedPtr< FUICommandInfo > BakeGeometryCache;
	
	virtual void RegisterCommands() override;
};
