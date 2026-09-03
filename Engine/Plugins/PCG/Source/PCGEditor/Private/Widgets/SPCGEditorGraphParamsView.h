// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

#include "Widgets/SCompoundWidget.h"

struct FPropertyAndParent;
class FPCGEditor;

/** Displays a PCGGraph's User Parameters details view. */
class SPCGEditorGraphUserParametersView final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphUserParametersView)	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<FPCGEditor>& InPCGEditor);
};