// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class UAnimNextRigVMAsset;
class FTextFilterExpressionEvaluator;
class SSearchBox;

namespace UE::AnimNext::Editor
{
	class SRigVMFunctionRowWidget;
}

namespace UE::AnimNext::Editor
{

// Delegate called when the user picks a function from the dropdown menu
// The header passed when selecting 'None' will be invalid
using FOnRigVMFunctionPicked = TDelegate<void(const FRigVMGraphFunctionHeader&)>;

// Picker widget the allows choosing a RigVM function
class SRigVMFunctionPicker : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SRigVMFunctionPicker)
		: _AllowNew(true)
		, _AllowClear(true)
	{}

	// The 'current' asset, used for accessing private functions
	SLATE_ARGUMENT(FAssetData, CurrentAsset)

	// Delegate called when the user picks a function from the dropdown menu.
	// The header passed when selecting 'None' will be invalid 
	SLATE_EVENT(FOnRigVMFunctionPicked, OnRigVMFunctionPicked)

	// Delegate called when the user chooses 'New Function...'
	SLATE_EVENT(FSimpleDelegate, OnNewFunction)

	// Attribute used to display the currently-picked function in a combo button 
	SLATE_ATTRIBUTE(FText, FunctionName)

	// Attribute used to display tooltip information about the currently-picked function
	SLATE_ATTRIBUTE(FText, FunctionToolTip)

	// Whether to show the 'New Function...' item
	SLATE_ARGUMENT(bool, AllowNew)

	// Whether to show the 'None' item
	SLATE_ARGUMENT(bool, AllowClear)

	SLATE_END_ARGS()

	ANIMNEXTEDITOR_API void Construct(const FArguments& InArgs);

private:
	friend UE::AnimNext::Editor::SRigVMFunctionRowWidget;

	// Requests a refresh of the dropdown menu contents
	void RequestRefreshEntries();

	// Refreshes the dropdown menu contents
	void RefreshEntries();

	enum class EEntryType
	{
		None,
		Asset,
		Function,
		NewFunction,
	};

	struct FEntry
	{
		explicit FEntry(EEntryType InType)
			: Type(InType)
		{}

		EEntryType Type;
		FText Name;
		FText ToolTip;
		const FSlateBrush* Icon = nullptr;
	};

	struct FFunctionEntry : FEntry
	{
		FFunctionEntry()
			: FEntry(EEntryType::Function)
		{}

		FRigVMGraphFunctionHeader FunctionHeader;
	};

	struct FAssetEntry : FEntry
	{
		FAssetEntry()
			: FEntry(EEntryType::Asset)
		{}

		FAssetData Asset;
		TArray<TSharedPtr<FEntry>> Functions;
		TArray<TSharedPtr<FEntry>> FilteredFunctions;
	};

	struct FNewFunctionEntry : FEntry
	{
		FNewFunctionEntry()
			: FEntry(EEntryType::NewFunction)
		{}
	};

	struct FNoneEntry : FEntry
	{
		FNoneEntry()
			: FEntry(EEntryType::None)
		{}
	};

	FAssetData CurrentAsset;
	TWeakObjectPtr<UAnimNextRigVMAsset> WeakCurrentAsset;
	FOnRigVMFunctionPicked OnRigVMFunctionPicked;
	FSimpleDelegate OnNewFunction;
	TAttribute<FText> FunctionName;
	TAttribute<FText> FunctionToolTip;
	TSharedPtr<SSearchBox> SearchBox;
	FText FilterText;
	TSharedPtr<STreeView<TSharedPtr<FEntry>>> TreeView;
	TArray<TSharedPtr<FEntry>> Entries;
	TArray<TSharedPtr<FEntry>> FilteredEntries;
	TSharedPtr<FTextFilterExpressionEvaluator> TextFilter;
	bool bAllowClear = false;
	bool bAllowNew = false;
};

}
