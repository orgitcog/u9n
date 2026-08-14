// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"

namespace EditorAnimUtils{ struct FNameDuplicationRule; }

struct FCanProcessResult 
{
	bool bCanProcess;
	FText CanProcessText;
};

// dialog to select path to export to
class SMetaHumanBatchExportPathDialog: public SWindow
{
	
public:

	SLATE_BEGIN_ARGS(SMetaHumanBatchExportPathDialog){}
	SLATE_ARGUMENT(FString, AssetTypeName)
	SLATE_ARGUMENT(EditorAnimUtils::FNameDuplicationRule*, NameRule)
	SLATE_ARGUMENT(FString, DefaultFolder)
	SLATE_ARGUMENT(FString, PrefixHint)
	SLATE_ATTRIBUTE(FCanProcessResult, CanProcessConditional)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// displays the dialog in a blocking fashion
	EAppReturnType::Type ShowModal();

private:
	
	FReply OnButtonClick(EAppReturnType::Type InButtonID);
	
	// example rename text
	void UpdateExampleText();
	
	// modify folder output path
	FText GetFolderPath() const;

	// remove characters not allowed in asset names
	static FString ConvertToCleanString(const FText& InToClean);
	
	FText ExampleText; // The rename rule sample text
	EAppReturnType::Type UserResponse = EAppReturnType::Cancel;

	// the name rule are editing with this pop-up window
	EditorAnimUtils::FNameDuplicationRule* NameRule = nullptr;

	// Conditional to determine if you can proceed
	TAttribute<FCanProcessResult> CanProcessConditional;
	bool bCanProcess = true;
	FText CanProcessText;

	bool CanProcess() const;
	void UpdateCanProcess();
};
