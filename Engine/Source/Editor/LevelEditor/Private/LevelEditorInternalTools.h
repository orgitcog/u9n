// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "SLevelViewport.h"

#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceInterface.h"

namespace InternalEditorLevelLibrary
{
	TSharedPtr<SLevelViewport> GetLevelViewport(const FName& ViewportConfigKey);
	TSharedPtr<SLevelViewport> GetActiveLevelViewport();
	bool IsEditingLevelInstanceCurrentLevel(UWorld* InWorld);
	AActor* GetEditingLevelInstance(UWorld* InWorld);
	bool IsActorEditorContextVisible(UWorld* InWorld);
}
