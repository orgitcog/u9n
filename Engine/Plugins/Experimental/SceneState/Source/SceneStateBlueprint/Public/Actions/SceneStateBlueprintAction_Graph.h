// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphSchema_K2_Actions.h"
#include "Textures/SlateIcon.h"

namespace UE::SceneState::Graph
{

struct FBlueprintAction_Graph : public FEdGraphSchemaAction_K2Graph
{
	FBlueprintAction_Graph() = default;

	explicit FBlueprintAction_Graph(EEdGraphSchemaAction_K2Graph::Type InType
		, const FText& InNodeCategory
		, const FText& InMenuDesc
		, const FText& InToolTip
		, const FSlateIcon& InIcon
		, const int32 InGrouping
		, const int32 InSectionID = 0)
		: FEdGraphSchemaAction_K2Graph(InType, InNodeCategory, InMenuDesc, InToolTip, InGrouping, InSectionID)
		, Icon(InIcon)
	{
	}

	static FName StaticGetTypeId()
	{
		static FName Type = TEXT("FEdGraphSchemaAction");
		return Type;
	}

	//~ Begin FEdGraphSchemaAction
	virtual const FSlateBrush* GetPaletteIcon() const override
	{
		return Icon.GetIcon();
	}

	virtual FName GetTypeId() const override
	{
		return StaticGetTypeId();
	}
	//~ End FEdGraphSchemaAction

	const FSlateIcon Icon;
};

} // UE::SceneState::Editor
