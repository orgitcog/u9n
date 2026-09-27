// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISlateIMContainer.h"
#include "Misc/ISlateIMChild.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"

class SImTableView;

class FSlateIMTableRow : public ISlateIMContainer, public ISlateIMChild, public TSharedFromThis<FSlateIMTableRow>
{
	SLATE_IM_TYPE_DATA(FSlateIMTableRow, ISlateIMChild)
	// Deadly diamond of death, but it's okay since ISlateIMContainer and ISlateIMChild don't have implementations for ISlateIMTypeChecking
	// SLATE_IM_TYPE_DATA(FSlateIMTableRow, ISlateIMContainer)

public:
	virtual FString GetDebugName() override { return GetTypeId().ToString(); }
	virtual int32 GetNumChildren() override { return Children.Num(); }
	virtual FSlateIMChild GetChild(int32 Index) override { return Children.IsValidIndex(Index) ? Children[Index] : nullptr; }
	virtual FSlateIMChild GetContainer() override { return TSharedRef<ISlateIMChild>(AsShared()); }
	virtual void RemoveUnusedChildren(int32 LastUsedChildIndex) override;
	virtual void UpdateChild(FSlateIMChild Child, int32 Index, const FSlateIMSlotData& AlignmentData) override;
	virtual TSharedPtr<SWidget> GetAsWidget() override;

	void GetChildRows(TArray<TSharedRef<FSlateIMTableRow>>& OutRows) const;

	int32 CountCellWidgetsUpToIndex(int32 Index) const;

	TSharedRef<SWidget> GetCellWidget(int32 CellIndex) const;

	int32 GetColumnCount() const { return ColumnCount; }
	void UpdateColumnCount(int32 NewColumnCount);

	void SetOwningTable(const TSharedPtr<SImTableView>& InOwningTable);

	bool IsExpanded();
	bool HasChildRows() const;
	bool ShouldDisplayExpander() const;
	bool AreTableRowContentsRequired();

private:
	// A mix of cell widgets and child rows
	TArray<FSlateIMChild> Children;
	int32 ColumnCount = 0;
	TSharedPtr<SImTableView> OwningTable;
};

class SImTableRow : public SMultiColumnTableRow<TSharedRef<FSlateIMTableRow>>
{
	SLATE_DECLARE_WIDGET(SImTableRow, SMultiColumnTableRow<TSharedRef<FSlateIMTableRow>>)
	
public:	
	void Construct(const FSuperRowType::FArguments& InArgs, const TSharedRef<SImTableView>& InOwnerTableView, const TSharedRef<FSlateIMTableRow>& InTableRow);
	
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	
private:
	TSharedPtr<FSlateIMTableRow> TableRow;
};

class SImTableHeader : public SHeaderRow
{
	SLATE_DECLARE_WIDGET(SImTableHeader, SHeaderRow)
};

class SImTableView : public STreeView<TSharedRef<FSlateIMTableRow>>, public ISlateIMContainer
{
	SLATE_DECLARE_WIDGET(SImTableView, STreeView<TSharedRef<FSlateIMTableRow>>)
	SLATE_IM_TYPE_DATA(SImTableView, ISlateIMContainer)
	
public:
	void Construct(const FArguments& InArgs);

	virtual int32 GetNumChildren() override { return TableRows.Num(); }
	virtual FSlateIMChild GetChild(int32 Index) override;
	virtual FSlateIMChild GetContainer() override { return AsShared(); }
	virtual void RemoveUnusedChildren(int32 LastUsedChildIndex) override;
	virtual void UpdateChild(FSlateIMChild Child, int32 Index, const FSlateIMSlotData& AlignmentData) override;
	
	virtual float GetNumLiveWidgets() const override;

	void AddColumn(const FStringView& ColumnLabel, const FStringView& ColumnToolTip, const FSlateIMSlotData& SlotData);

	void BeginTableUpdates();
	void EndTableUpdates();

	void BeginTableContent();

	void UpdateColumns();

	void SetTableRowStyle(const FTableRowStyle* InRowStyle);

	// A table is a tree if any of its rows has children
	bool IsTree() const;

	void MarkDirty() { bIsDirty = true; }

	TSharedRef<ITableRow> GenerateRow(TSharedRef<FSlateIMTableRow> InTableRow, const TSharedRef<STableViewBase>& OwnerTable);

	void GatherChildren(TSharedRef<FSlateIMTableRow> Row, TArray<TSharedRef<FSlateIMTableRow>>& OutChildren) const;

	TSharedPtr<FSlateIMTableRow> GetRow(int32 Index) { return TableRows.IsValidIndex(Index) ? TableRows[Index].ToSharedPtr() : nullptr; }

	int32 GetRowLinearizedIndex(const TSharedRef<FSlateIMTableRow>& Row) const;

private:
	void AddColumn_Internal(const FName& ColumnId, const FStringView& ColumnToolTip, const FSlateIMSlotData& SlotData, const FStringView& ColumnLabel = FStringView());

	TArray<TSharedRef<FSlateIMTableRow>> TableRows;
	TSharedPtr<SImTableHeader> Header;
	int32 ColumnCount = 0;
	bool bIsDirty = false;
	const FTableRowStyle* RowStyle = nullptr;

	mutable int32 CachedNumLiveWidgets = 0;
};
