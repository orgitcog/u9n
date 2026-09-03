// Copyright Epic Games, Inc. All Rights Reserved.

#include "SImTableView.h"

#include "Misc/SlateIMLogging.h"
#include "Misc/SlateIMSlotData.h"

SLATE_IMPLEMENT_WIDGET(SImTableRow)
SLATE_IMPLEMENT_WIDGET(SImTableHeader)
SLATE_IMPLEMENT_WIDGET(SImTableView)

namespace SlateIM::Private
{
	const FName ExpanderColumnName = TEXT("Expander");
	const FName BaseColumnName = TEXT("TableColumn");
}


void FSlateIMTableRow::RemoveUnusedChildren(int32 LastUsedChildIndex)
{
	Children.SetNum(FMath::Min(Children.Num(), LastUsedChildIndex + 1));
}

void FSlateIMTableRow::UpdateChild(FSlateIMChild Child, int32 Index, const FSlateIMSlotData& AlignmentData)
{
	if (TSharedPtr<FSlateIMTableRow> ChildRow = Child.GetChild<FSlateIMTableRow>())
	{
		ChildRow->UpdateColumnCount(ColumnCount);
		ChildRow->SetOwningTable(OwningTable);
	}

	if (Children.IsValidIndex(Index))
	{
		Children[Index] = Child;
	}
	else
	{
		Children.Add(Child);
	}

	if (OwningTable)
	{
		OwningTable->MarkDirty();
	}
}

TSharedPtr<SWidget> FSlateIMTableRow::GetAsWidget()
{
	if (OwningTable)
	{
		return StaticCastSharedPtr<SImTableRow>(OwningTable->WidgetFromItem(AsShared()));
	}

	return nullptr;
}

void FSlateIMTableRow::GetChildRows(TArray<TSharedRef<FSlateIMTableRow>>& OutRows) const
{
	for (const FSlateIMChild& Child : Children)
	{
		if (TSharedPtr<FSlateIMTableRow> ChildRow = Child.GetChild<FSlateIMTableRow>())
		{
			OutRows.Add(ChildRow.ToSharedRef());
		}
	}
}

int32 FSlateIMTableRow::CountCellWidgetsUpToIndex(int32 Index) const
{
	int32 CellCount = 0;
	
	for (int32 i = 0; i < Children.Num() && i <= Index; ++i)
	{
		const FSlateIMChild& Child = Children[i];
		if (TSharedPtr<SWidget> ChildRow = Child.GetWidget())
		{
			++CellCount;
		}
	}

	return CellCount;
}

TSharedRef<SWidget> FSlateIMTableRow::GetCellWidget(int32 CellIndex) const
{
	int32 CellCount = 0;
	for (const FSlateIMChild& Child : Children)
	{
		if (TSharedPtr<SWidget> ChildRow = Child.GetWidget())
		{
			if (CellIndex == CellCount)
			{
				return ChildRow.ToSharedRef();
			}

			++CellCount;
		}
	}

	return SNullWidget::NullWidget;
}

void FSlateIMTableRow::UpdateColumnCount(int32 NewColumnCount)
{
	ColumnCount = NewColumnCount;

	for (const FSlateIMChild& Child : Children)
	{
		if (TSharedPtr<FSlateIMTableRow> ChildRow = Child.GetChild<FSlateIMTableRow>())
		{
			ChildRow->UpdateColumnCount(ColumnCount);
		}
	}
}

void FSlateIMTableRow::SetOwningTable(const TSharedPtr<SImTableView>& InOwningTable)
{
	OwningTable = InOwningTable;

	for (const FSlateIMChild& Child : Children)
	{
		if (TSharedPtr<FSlateIMTableRow> ChildRow = Child.GetChild<FSlateIMTableRow>())
		{
			ChildRow->SetOwningTable(OwningTable);
		}
	}
}

bool FSlateIMTableRow::IsExpanded()
{
	return OwningTable && OwningTable->IsItemExpanded(AsShared());
}

bool FSlateIMTableRow::HasChildRows() const
{
	for (const FSlateIMChild& Child : Children)
	{
		if (TSharedPtr<FSlateIMTableRow> ChildRow = Child.GetChild<FSlateIMTableRow>())
		{
			return true;
		}
	}

	return false;
}

bool FSlateIMTableRow::ShouldDisplayExpander() const
{
	return OwningTable && OwningTable->IsTree();
}

bool FSlateIMTableRow::AreTableRowContentsRequired()
{
	if (OwningTable)
	{
		if (OwningTable->WidgetFromItem(AsShared()).IsValid())
		{
			// This row has a live widget so the contents are required
			return true;
		}

		const float NumLiveWidgets = OwningTable->GetNumLiveWidgets();
		const float CurrentScrollOffset = OwningTable->GetScrollOffset();

		// Require content from rows within one "page" of the current scroll position in either direction
		// This is probably a bigger window than necessary but should handle large scroll movements and should be small enough to not impact perf
		const float MinNearlyLiveWidget = FMath::Max(0.f, CurrentScrollOffset - NumLiveWidgets);
		const float MaxNearlyLiveWidget = CurrentScrollOffset + (2.f * NumLiveWidgets);
		const float ThisRowIndex = OwningTable->GetRowLinearizedIndex(AsShared());
		if (ThisRowIndex >= MinNearlyLiveWidget && ThisRowIndex <= MaxNearlyLiveWidget)
		{
			return true;
		}
	}
	
	return false;
}

void SImTableRow::PrivateRegisterAttributes(FSlateAttributeInitializer&)
{
}

void SImTableRow::Construct(const FSuperRowType::FArguments& InArgs, const TSharedRef<SImTableView>& InOwnerTableView, const TSharedRef<FSlateIMTableRow>& InTableRow)
{
	TableRow = InTableRow;
	SMultiColumnTableRow<TSharedRef<FSlateIMTableRow>>::Construct(InArgs, InOwnerTableView);
}

TSharedRef<SWidget> SImTableRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == SlateIM::Private::ExpanderColumnName)
	{
		return SAssignNew(ExpanderArrowWidget, SExpanderArrow, SharedThis(this))
			.StyleSet(ExpanderStyleSet)
			.ShouldDrawWires(true);
	}
	else if (TableRow)
	{
		const int32 ColumnIndex = ColumnName.GetNumber();
		if (ColumnIndex == 0 && TableRow->ShouldDisplayExpander())
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(ExpanderArrowWidget, SExpanderArrow, SharedThis(this))
					.StyleSet(ExpanderStyleSet)
					.ShouldDrawWires(true)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				[
					TableRow->GetCellWidget(ColumnIndex)
				];
		}
		else
		{
			return TableRow->GetCellWidget(ColumnIndex);
		}
	}

	return SNullWidget::NullWidget;
}

void SImTableHeader::PrivateRegisterAttributes(FSlateAttributeInitializer&)
{
}

void SImTableView::PrivateRegisterAttributes(FSlateAttributeInitializer&)
{
}

void SImTableView::Construct(const FArguments& InArgs)
{
	Super::Construct(
		FArguments(InArgs)
		.TreeItemsSource(&TableRows)
		.HeaderRow(SAssignNew(Header, SImTableHeader))
		.OnGenerateRow(this, &SImTableView::GenerateRow)
		.OnGetChildren(this, &SImTableView::GatherChildren));
}

FSlateIMChild SImTableView::GetChild(int32 Index)
{
	return TableRows.IsValidIndex(Index) ? FSlateIMChild(TableRows[Index]) : FSlateIMChild(nullptr);
}

void SImTableView::RemoveUnusedChildren(int32 LastUsedChildIndex)
{
	if (TableRows.Num() > (LastUsedChildIndex + 1))
	{
		bIsDirty = true;
		TableRows.SetNum(LastUsedChildIndex + 1);
	}
}

void SImTableView::UpdateChild(FSlateIMChild Child, int32 Index, const FSlateIMSlotData& AlignmentData)
{
	TSharedPtr<FSlateIMTableRow> Row = Child.GetChild<FSlateIMTableRow>();
	if (!ensureMsgf(Row, TEXT("Tables can only hold Rows/Cells")))
	{
		return;
	}

	Row->SetOwningTable(SharedThis(this));

	if (TableRows.IsValidIndex(Index))
	{
		if (TableRows[Index] != Row)
		{
			bIsDirty = true;
			TableRows[Index] = Row.ToSharedRef();
		}
	}
	else
	{
		bIsDirty = true;
		TableRows.Add(Row.ToSharedRef());
	}

	if (Row->GetColumnCount() != ColumnCount)
	{
		bIsDirty = true;
		Row->UpdateColumnCount(ColumnCount);
	}
}

float SImTableView::GetNumLiveWidgets() const
{
	// NumLiveWidgets can be 0 during updates, so we use the last valid value we had
	const int32 NumLiveWidgets = STreeView<TSharedRef<FSlateIMTableRow>>::GetNumLiveWidgets();
	if (NumLiveWidgets > 0)
	{
		CachedNumLiveWidgets = NumLiveWidgets;
	}

	return CachedNumLiveWidgets;
}

void SImTableView::AddColumn(const FStringView& ColumnLabel, const FStringView& ColumnToolTip, const FSlateIMSlotData& SlotData)
{
	// Only add columns that don't already exist
	if (Header)
	{
		// TODO - Handle changes to SlotData after construction
		const int32 MaxColumnIndex = Header->GetColumns().Num();
		if (ColumnCount >= MaxColumnIndex)
		{
			FName ColumnName = SlateIM::Private::BaseColumnName;
			ColumnName.SetNumber(ColumnCount);

			AddColumn_Internal(ColumnName, ColumnToolTip, SlotData, ColumnLabel);
		}
	}
	
	++ColumnCount;
}

void SImTableView::BeginTableUpdates()
{
	ColumnCount = 0;
}

void SImTableView::EndTableUpdates()
{
	if (bIsDirty)
	{
		bIsDirty = false;
		RequestListRefresh();
	}
}

void SImTableView::BeginTableContent()
{
	if (ColumnCount == 0)
	{
		// TODO - Do we force create a column here or can we hide the header and automatically treat everything like a list?
	}
	UpdateColumns();
}

void SImTableView::UpdateColumns()
{
	if (Header)
	{
		FName ColumnName = SlateIM::Private::BaseColumnName;
		const int32 MaxColumnIndex = Header->GetColumns().Num();
		for (int32 ColumnIndex = ColumnCount; ColumnIndex < MaxColumnIndex; ++ColumnIndex)
		{
			ColumnName.SetNumber(ColumnIndex);
			Header->RemoveColumn(ColumnName);
		}
	}
}

void SImTableView::SetTableRowStyle(const FTableRowStyle* InRowStyle)
{
	RowStyle = InRowStyle;
}

bool SImTableView::IsTree() const
{
	for (const TSharedRef<FSlateIMTableRow>& TableRow : TableRows)
	{
		if (TableRow->HasChildRows())
		{
			return true;
		}
	}

	return false;
}

TSharedRef<ITableRow> SImTableView::GenerateRow(TSharedRef<FSlateIMTableRow> InTableRow, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SImTableRow, SharedThis(this), InTableRow)
		.Style(RowStyle ? RowStyle : &FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"));
}

void SImTableView::GatherChildren(TSharedRef<FSlateIMTableRow> Row, TArray<TSharedRef<FSlateIMTableRow>>& OutChildren) const
{
	Row->GetChildRows(OutChildren);
}

int32 SImTableView::GetRowLinearizedIndex(const TSharedRef<FSlateIMTableRow>& Row) const
{
	return LinearizedItems.IndexOfByKey(Row);
}

void SImTableView::AddColumn_Internal(const FName& ColumnId, const FStringView& ColumnToolTip, const FSlateIMSlotData& SlotData, const FStringView& ColumnLabel)
{
	SHeaderRow::FColumn::FArguments Args = SHeaderRow::FColumn::FArguments()
		.ColumnId(ColumnId)
		.DefaultLabel(FText::FromStringView(ColumnLabel))
		.DefaultTooltip(FText::FromStringView(ColumnToolTip))
		.HAlignHeader(SlotData.HorizontalAlignment)
		.VAlignHeader(SlotData.VerticalAlignment);

	if (SlotData.bAutoSize && SlotData.MinWidth > 0)
	{
		if (SlotData.MaxWidth > 0 && SlotData.MaxWidth <= SlotData.MinWidth)
		{
			Args.FixedWidth(SlotData.MinWidth);
		}
		else
		{
			Args.ManualWidth(SlotData.MinWidth);
		}
	}
	else
	{
		if (SlotData.MinWidth > 0)
		{
			Args.FillSized(SlotData.MinWidth);
		}
		else
		{
			Args.FillWidth(1.f);
		}
	}
		
	Header->AddColumn(Args);
	bIsDirty = true;
}
