// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SAssetEditorViewport.h"

class SMetaHumanCharacterPaletteEditorViewport : public SAssetEditorViewport
{
public:

	// These allow the toolkit to add an accept/cancel overlay when needed. PopulateViewportOverlays
	// is not helpful here because that gets called just once.
	virtual void AddOverlayWidget(TSharedRef<SWidget> InWidget, int32 InZOrder = INDEX_NONE);
	virtual void RemoveOverlayWidget(TSharedRef<SWidget> InWidget);
};