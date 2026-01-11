// Copyright Epic Games, Inc. All Rights Reserved.


#include "Widgets/STedsTableViewerRow.h"

#include "TedsTableViewerColumn.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Widgets/STedsTableViewer.h"

namespace UE::Editor::DataStorage
{
	void STedsTableViewerRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedRef<FTedsTableViewerModel>& InTableViewerModel)
	{
		Item = InArgs._Item;
		TableViewerModel = InTableViewerModel;
		ParentWidgetRowHandle = InArgs._ParentWidgetRowHandle;
		ItemHeight = InArgs._ItemHeight;

		const auto Args = FSuperRowType::FArguments()
		.Padding(InArgs._Padding)
		.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("SceneOutliner.TableViewRow"));

		SMultiColumnTableRow<TableViewerItemPtr>::Construct(Args, OwnerTableView);
	}

	TSharedRef<SWidget> STedsTableViewerRow::GenerateWidgetForColumn(const FName& ColumnName)
	{
		if(TSharedPtr<FTedsTableViewerColumn> Column = TableViewerModel->GetColumn(ColumnName))
		{
			auto WidgetRowSetupDelegate = [this] (ICoreProvider& InStorage, const RowHandle& InUIRowHandle)
			{
				InStorage.AddColumn(InUIRowHandle, FTableRowParentColumn{ .Parent = ParentWidgetRowHandle });
			};

			if (TSharedPtr<SWidget> RowWidget = Column->ConstructRowWidget(Item, WidgetRowSetupDelegate))
			{
				return SNew(SBox)
						.HeightOverride(this, &STedsTableViewerRow::GetCurrentItemHeight)
						.MinDesiredHeight(20.f)
						.VAlign(VAlign_Center)
						[
							RowWidget.ToSharedRef()
						];
			}
		}

		return SNullWidget::NullWidget;
	}

	FOptionalSize STedsTableViewerRow::GetCurrentItemHeight() const
	{
		return ItemHeight.IsSet() ? ItemHeight.Get() : FOptionalSize();
	}

	TSharedRef<SWidget> SHierarchyViewerRow::GenerateWidgetForColumn(const FName& ColumnName)
	{
		TSharedRef<SWidget> ActualWidget = STedsTableViewerRow::GenerateWidgetForColumn(ColumnName);

		// We show the expander arrow on the first column only
		if (TableViewerModel->GetColumnIndex(ColumnName) == 0)
		{
			return SNew(SBox)
				.MinDesiredHeight(20.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6.f, 0.f, 0.f, 0.f)
					[
						SNew(SExpanderArrow, SharedThis(this)).IndentAmount(12)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						ActualWidget
					]
				];
		}

		return ActualWidget;
	}
} // namespace UE::Editor::DataStorage
