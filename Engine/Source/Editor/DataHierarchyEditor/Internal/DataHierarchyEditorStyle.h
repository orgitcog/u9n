// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

/** Manages the style which provides resources for Hierarchy Editor widgets. */
class DATAHIERARCHYEDITOR_API FDataHierarchyEditorStyle : public FSlateStyleSet
{
public:
	static void Register();
	static void Unregister();
	static void Shutdown();

	/** reloads textures used by slate renderer */
	static void ReloadTextures();

	/** @return The Slate style set for Hierarchy Editor widgets */
	static const FDataHierarchyEditorStyle& Get();

	static void ReinitializeStyle();

private:	
	FDataHierarchyEditorStyle();

	void InitDataHierarchyEditor();

	static TSharedPtr<FDataHierarchyEditorStyle> DataHierarchyEditorStyle;
};
