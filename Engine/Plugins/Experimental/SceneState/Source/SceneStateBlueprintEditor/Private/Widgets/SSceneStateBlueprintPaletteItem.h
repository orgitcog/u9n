// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPalette.h"

namespace UE::SceneState::Editor
{
	class FSceneStateBlueprintEditor;	
}

namespace UE::SceneState::Editor
{

/** Widget for displaying a Blueprint Item  */
class SBlueprintPaletteItem : public SGraphPaletteItem
{
public:
	SLATE_BEGIN_ARGS(SBlueprintPaletteItem) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FCreateWidgetForActionData* const InCreateData, TWeakPtr<FSceneStateBlueprintEditor> InBlueprintEditorWeak);

private:
	//~ Begin SGraphPaletteItem
	virtual void OnNameTextCommitted(const FText& InText, ETextCommit::Type InCommitType);
	//~ End SGraphPaletteItem
};

} // UE::SceneState::Editor
