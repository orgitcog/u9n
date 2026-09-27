// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SMetaHumanCharacterEditorTileView.h"
#include "Widgets/SCompoundWidget.h"
#include "MetaHumanBodyType.h"
#include "Tools/MetaHumanCharacterEditorBodyEditingTools.h"

class SMetaHumanCharacterEditorFixedCompatibilityPanel : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, uint8)

public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorFixedCompatibilityPanel)
	{}

		SLATE_ARGUMENT(UMetaHumanCharacterFixedCompatibilityBodyProperties*, FixedCompatabilityProperties)

		/** Called when the selection of the Tile View has changed. */
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void UpdateItemListFromProperties();

private:
	int32 GetHeightValue() const;
	void OnHeightValueChanged(int32 HeightValue);
	const FSlateBrush* GetFixedCompatabilityBodyBrush(uint8 InItem);

	TWeakObjectPtr<UMetaHumanCharacterFixedCompatibilityBodyProperties> FixedCompatabilityProperties;
	TSharedPtr<SMetaHumanCharacterEditorTileView<EMetaHumanBodyType>> TileView;

	FOnSelectionChanged OnSelectionChangedDelegate;
};
