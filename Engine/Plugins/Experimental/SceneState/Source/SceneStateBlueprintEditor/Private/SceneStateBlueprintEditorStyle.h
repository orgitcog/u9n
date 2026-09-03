// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class UEdGraphSchema;
struct FSlateIcon;
template <typename T> class TSubclassOf;

namespace UE::SceneState::Editor
{

class FBlueprintEditorStyle : public FSlateStyleSet
{
public:
	static FBlueprintEditorStyle& Get()
	{
		static FBlueprintEditorStyle Instance;
		return Instance;
	}

	FBlueprintEditorStyle();
	virtual ~FBlueprintEditorStyle() override;

	FSlateIcon GetGraphSchemaIcon(TSubclassOf<UEdGraphSchema> InSchemaClass) const;
};

} // UE::SceneState::Editor
