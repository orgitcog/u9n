// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Model/ProjectLauncherModel.h"

class ITableRow;
class STableViewBase;
template<typename ItemType> class STreeView;

class PROJECTLAUNCHER_API SCustomLaunchMapListView
	: public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, TArray<FString> );

	SLATE_BEGIN_ARGS(SCustomLaunchMapListView){}
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged);
		SLATE_ATTRIBUTE(TArray<FString>, SelectedMaps);
		SLATE_ATTRIBUTE(FString, ProjectPath)
	SLATE_END_ARGS()

public:
	void Construct( const FArguments& InArgs, TSharedRef<ProjectLauncher::FModel> InModel );

	void RefreshMapList();
	TSharedRef<SWidget> MakeControlsWidget();

protected:
	TSharedPtr<ProjectLauncher::FModel> Model;
	TAttribute<TArray<FString>> SelectedMaps;
	TAttribute<FString> ProjectPath;
	FOnSelectionChanged OnSelectionChanged;

private:
	bool bShowFolders = true; // whether to display the available maps in a hierarchy or flat list

	struct FMapTreeNode
	{
		FString Name;
		ECheckBoxState CheckBoxState = ECheckBoxState::Unchecked;
		bool bFiltered = false;
		TArray<TSharedPtr<FMapTreeNode>> Children;
	};
	typedef TSharedPtr<FMapTreeNode> FMapTreeNodePtr;

	FMapTreeNodePtr MapTreeRoot;

	void OnProjectChanged();


	void RefreshCheckBoxState( bool bExpand = false);
	ECheckBoxState RefreshCheckBoxStateRecursive(FMapTreeNodePtr Node, bool bExpand);
	void SetCheckBoxStateRecursive(FMapTreeNodePtr Node, ECheckBoxState CheckBoxState, TArray<FString>& CheckedMaps);

	void OnSearchFilterTextCommitted(const FText& SearchText, ETextCommit::Type InCommitType);
	void OnSearchFilterTextChanged(const FText& SearchText);

	FString CurrentFilterText;

	TSharedPtr<STreeView<FMapTreeNodePtr>> MapTreeView;
	TArray<FMapTreeNodePtr> MapTreeViewItemsSource;

	void GetMapTreeNodeChildren(FMapTreeNodePtr Item, TArray<FMapTreeNodePtr>& OutChildren);
	TSharedRef<ITableRow> GenerateMapTreeNodeRow(FMapTreeNodePtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	ECheckBoxState GetMapTreeNodeCheckState(FMapTreeNodePtr Item) const;
	void SetMapTreeNodeCheckState(ECheckBoxState CheckBoxState, FMapTreeNodePtr Item);
	const FSlateBrush* GetMapTreeNodeIcon(FMapTreeNodePtr Node) const;
	FSlateColor GetMapTreeNodeColor(FMapTreeNodePtr Node) const;

	mutable bool bHasPaintedThisFrame = false;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	bool bMapListDirty = false;
	void RefreshMapListInternal();
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
};
