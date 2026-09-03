// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SWidget.h"

class SEditorViewportGridPanel : public SGridPanel
{
	SLATE_DECLARE_WIDGET_API(SEditorViewportGridPanel, SGridPanel, UNREALED_API)

public:
	SLATE_BEGIN_ARGS(SEditorViewportGridPanel)
		{}
		SLATE_ATTRIBUTE(TSharedPtr<SWidget>, ViewportWidget);
	SLATE_END_ARGS()

	UNREALED_API void Construct(const FArguments& InArgs);
	UNREALED_API virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	UNREALED_API void UpdateAspectRatio(const float& AspectRatio);

private:
	TAttribute<TSharedPtr<SWidget>> ViewportWidget;
	float DebugAspectRatio;
};