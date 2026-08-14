// Copyright Epic Games, Inc.All Rights Reserved.

#include "PaletteEditor/MetaHumanCharacterPaletteEditorViewportClient.h"

#include "PreviewScene.h"

FMetaHumanCharacterPaletteViewportClient::FMetaHumanCharacterPaletteViewportClient(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene)
	: FEditorViewportClient{ InModeTools, InPreviewScene }
{
	// The real time override is required to make sure the world ticks while the viewport is not active
	// or this requires the user to interact with the viewport to get up to date lighting and textures
	AddRealtimeOverride(true, NSLOCTEXT("FMetaHumanCharacterPaletteViewportClient", "RealTimeOverride", "Real-time Override"));
	SetRealtime(true);
}

void FMetaHumanCharacterPaletteViewportClient::Tick(float InDeltaSeconds)
{
	FEditorViewportClient::Tick(InDeltaSeconds);

	if (!GIntraFrameDebuggingGameThread && GetPreviewScene() != nullptr)
	{
		GetPreviewScene()->GetWorld()->Tick(LEVELTICK_All, InDeltaSeconds);
	}
}
