// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "SLevelViewport.h"
#include "LevelEditorMenuContext.h"

#include "LevelViewportContext.generated.h"

UCLASS()
class LEVELEDITOR_API ULevelViewportContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<SLevelViewport> LevelViewport;
};

// Customizes ULevelViewportToolBarContext for backwards compatability
// TODO: Remove this once the old toolbars are deprecated.
UCLASS()
class LEVELEDITOR_API ULegacyLevelViewportToolbarContext : public ULevelViewportToolBarContext
{
	GENERATED_BODY()
	
public:
	TWeakPtr<SLevelViewport> LevelViewport;
	
	virtual FLevelEditorViewportClient* GetLevelViewportClient() const override;
};
