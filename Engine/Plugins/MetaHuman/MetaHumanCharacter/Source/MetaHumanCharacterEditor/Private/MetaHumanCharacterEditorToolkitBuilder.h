// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ToolkitBuilder.h"

struct FMetaHumanCharacterEditorToolkitSections : FToolkitSections
{
	TSharedPtr<SWidget> ToolCustomWarningsArea = nullptr;
	TSharedPtr<SWidget> ToolViewArea = nullptr;
};

/** A customized Toolkit Builder used for the MetaHuman Character editor implementation */
class FMetaHumanCharacterEditorToolkitBuilder : public FToolkitBuilder
{
public:
	/**
	 * Constructor
	 *
	 * @param ToolbarCustomizationName the name of the  customization fr the category toolbar
	 * @param InToolkitCommandList  the toolkit FUICommandList
	 * @param InToolkitSections The FToolkitSections for this toolkit builder
	 */
	explicit FMetaHumanCharacterEditorToolkitBuilder(
		FName ToolbarCustomizationName,
		TSharedPtr<FUICommandList> InToolkitCommandList,
		TSharedPtr<FToolkitSections> InToolkitSections);

	/** default constructor */
	explicit FMetaHumanCharacterEditorToolkitBuilder(FToolkitBuilderArgs& Args);

protected:
	//~Begin FToolElementRegistrationArgs interface
	virtual TSharedPtr<SWidget> GenerateWidget() override;
	//~End FToolElementRegistrationArgs interface

private:
	/** Reference to the custom Tool Warnings section */
	TSharedPtr<SWidget> ToolCustomWarningsArea;

	/** Reference to the Tool View area section */
	TSharedPtr<SWidget> ToolViewArea;
};
