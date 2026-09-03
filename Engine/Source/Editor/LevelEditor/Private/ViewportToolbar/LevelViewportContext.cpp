// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelViewportContext.h"

#include "Editor/LevelEditor/Private/LevelEditorInternalTools.h"

FLevelEditorViewportClient* ULegacyLevelViewportToolbarContext::GetLevelViewportClient() const
{
	if (TSharedPtr<SLevelViewport> Viewport = LevelViewport.Pin())
	{
		return &Viewport->GetLevelViewportClient();
	}
	
	return Super::GetLevelViewportClient();
}
