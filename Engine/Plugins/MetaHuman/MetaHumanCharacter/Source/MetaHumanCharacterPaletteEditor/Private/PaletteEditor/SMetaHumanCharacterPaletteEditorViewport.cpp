// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaletteEditor/SMetaHumanCharacterPaletteEditorViewport.h"

void SMetaHumanCharacterPaletteEditorViewport::AddOverlayWidget(TSharedRef<SWidget> InWidget, int32 InZOrder)
{
	ViewportOverlay->AddSlot(InZOrder)
	[
		InWidget
	];
}

void SMetaHumanCharacterPaletteEditorViewport::RemoveOverlayWidget(TSharedRef<SWidget> InWidget)
{
	ViewportOverlay->RemoveSlot(InWidget);
}
