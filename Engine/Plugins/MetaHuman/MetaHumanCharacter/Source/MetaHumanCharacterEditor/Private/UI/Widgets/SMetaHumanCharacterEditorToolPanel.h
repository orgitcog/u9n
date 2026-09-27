// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

struct FSlateBrush;
class SButton;

/** Enum used to represent the tool panel hierarchy level. */
enum class EMetaHumanCharacterEditorPanelHierarchyLevel : uint8
{
	Top,
	Middle,
	Low
};

/** Button which allows to show an expanded/collapsed state. */
class SMetaHumanCharacterEditorArrowButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorArrowButton)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs);

	/** Gets whether this button is in expand state */
	bool IsExpanded() const { return bIsExpanded; }

private:
	/** Called when this button is clicked.*/
	FReply OnArrowButtonClicked();

	/** Gets the brush for this Button, according to the current state. */
	const FSlateBrush* GetArrowButtonImage() const;

	/** Reference to the Button widget. */
	TSharedPtr<SButton> Button;

	/** True if this button is in expand state */
	bool bIsExpanded = true;
};

/** Widget used to display tools and their properties widgets in the MetaHumanCharacter editor. */
class SMetaHumanCharacterEditorToolPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorToolPanel)
		: _Content()
		, _HeaderContent()
		, _HierarchyLevel(EMetaHumanCharacterEditorPanelHierarchyLevel::Top)
		, _RoundedBorders(true)
		, _Padding(FMargin(0.f))
		{}

		/** The panel's top level text label. */
		SLATE_ATTRIBUTE(FText, Label)

		/** The panel's top level icon. */
		SLATE_ATTRIBUTE(const FSlateBrush*, IconBrush)

		/** The panel's main slot, used for displaying content. */
		SLATE_DEFAULT_SLOT(FArguments, Content)
		
		/** The panel's additional header slot, used for displaying content in top right of header. */
		SLATE_NAMED_SLOT(FArguments, HeaderContent)

		/** The hierarchy level of this panel. */
		SLATE_ARGUMENT(EMetaHumanCharacterEditorPanelHierarchyLevel, HierarchyLevel)

		/** Whether the panel should have rounded borders. */
		SLATE_ARGUMENT(bool, RoundedBorders)

		SLATE_ATTRIBUTE(FMargin, Padding)


		FArguments& Padding(float Uniform)
		{
			_Padding = FMargin(Uniform);
			return *this;
		}

		FArguments& Padding(float Horizontal, float Vertical)
		{
			_Padding = FMargin(Horizontal, Vertical);
			return *this;
		}

		FArguments& Padding(float Left, float Top, float Right, float Bottom)
		{
			_Padding = FMargin(Left, Top, Right, Bottom);
			return *this;
		}

	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs);

private:
	/** Gets the panel border brush, according to the RoundedBorders attribute. */
	const FSlateBrush* GetPanelBorderBrush() const;

	/** Gets the visibility of the content slot, according to the arrow button state. */
	EVisibility GetContentSlotVisibility() const;

	/** Reference to the Arrow Button of the panel. */
	TSharedPtr<SMetaHumanCharacterEditorArrowButton> ArrowButton;

	/** The hierarchy level of this panel. */
	EMetaHumanCharacterEditorPanelHierarchyLevel HierarchyLevel = EMetaHumanCharacterEditorPanelHierarchyLevel::Top;

	/** The slate brush to draw for the ImageAttribute that we can invalidate. */
	TAttribute<const FSlateBrush*> IconBrushAttribute;

	/** True if the panel should has rounded borders. */
	bool bRoundedBorders = true;
};
