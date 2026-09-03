// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SAssetEditorViewport.h"

class SMetaHumanCharacterEditorViewport : public SAssetEditorViewport
{
public:

	virtual TSharedPtr<SWidget> BuildViewportToolbar() override;

	TSharedRef<class FMetaHumanCharacterViewportClient> GetMetaHumanCharacterEditorViewportClient() const;
};