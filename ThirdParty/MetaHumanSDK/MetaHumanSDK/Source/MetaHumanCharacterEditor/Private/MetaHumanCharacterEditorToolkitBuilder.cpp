// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorToolkitBuilder.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

FMetaHumanCharacterEditorToolkitBuilder::FMetaHumanCharacterEditorToolkitBuilder(
	FName ToolbarCustomizationName,
	TSharedPtr<FUICommandList> InToolkitCommandList,
	TSharedPtr<FToolkitSections> InToolkitSections)
	: FToolkitBuilder(ToolbarCustomizationName, InToolkitCommandList, InToolkitSections)
{
	const TSharedPtr<FMetaHumanCharacterEditorToolkitSections> Sections = StaticCastSharedPtr<FMetaHumanCharacterEditorToolkitSections>(InToolkitSections);
	if (Sections.IsValid())
	{
		ToolCustomWarningsArea = Sections->ToolCustomWarningsArea;
		ToolViewArea = Sections->ToolViewArea;
	}
}

FMetaHumanCharacterEditorToolkitBuilder::FMetaHumanCharacterEditorToolkitBuilder(FToolkitBuilderArgs& Args)
	: FToolkitBuilder(Args)
{
	const TSharedPtr<FMetaHumanCharacterEditorToolkitSections> Sections = StaticCastSharedPtr<FMetaHumanCharacterEditorToolkitSections>(Args.ToolkitSections);
	if (Sections.IsValid())
	{
		ToolCustomWarningsArea = Sections->ToolCustomWarningsArea;
		ToolViewArea = Sections->ToolViewArea;
	}
}

TSharedPtr<SWidget> FMetaHumanCharacterEditorToolkitBuilder::GenerateWidget()
{
	const TSharedPtr<SWidget> Widget = FCategoryDrivenContentBuilderBase::GenerateWidget();

	if (MainContentVerticalBox.IsValid() && ToolViewArea.IsValid())
	{
		MainContentVerticalBox->AddSlot()
			.AutoHeight()
			[
				ToolCustomWarningsArea.ToSharedRef()
			];

		MainContentVerticalBox->AddSlot()
			[
				ToolViewArea.ToSharedRef()
			];
	}

	return Widget;
}

#undef LOCTEXT_NAMESPACE