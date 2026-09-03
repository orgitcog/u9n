// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"

class FMetaHumanCharacterPaletteViewportClient : public FEditorViewportClient
{
public:
	FMetaHumanCharacterPaletteViewportClient(FEditorModeTools* InModeToos, FPreviewScene* InPreviewScene);

	//~Begin FEditorViewportClient interface
	virtual void Tick(float InDeltaSeconds) override;
	//~End FEditorViewportClientInterface
};