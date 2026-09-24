// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/SListView.h" // IWYU pragma: keep
#include "Widgets/Views/STableRow.h"


class SDockTab;
class STableViewBase;
class ULiveLinkDevice;
class ULiveLinkDeviceCapability;


DECLARE_DELEGATE_OneParam(FOnSelectionChangedDelegate, ULiveLinkDevice*);


struct FLiveLinkDeviceRowData
{
	TWeakObjectPtr<ULiveLinkDevice> WeakDevice;

	FLiveLinkDeviceRowData(ULiveLinkDevice* InDevice);
};


class SLiveLinkDeviceTable : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLiveLinkDeviceTable) {}
		SLATE_EVENT(FOnSelectionChangedDelegate, OnSelectionChanged)
	SLATE_END_ARGS()

public:
	static const FName ColumnName_StatusIcon;
	static const FName ColumnName_DisplayName;
	static const FName ColumnName_Remove;

public:
	void Construct(const FArguments& InArgs, TSharedRef<SDockTab> InContainingTab);
	virtual ~SLiveLinkDeviceTable();

private:
	void RegenerateList();
	void RegenerateColumns();
	void RegenerateRows();

private:
	TWeakPtr<SDockTab> WeakContainingTab;

	TSharedPtr<SHeaderRow> HeaderRow;
	TArray<TSharedPtr<FLiveLinkDeviceRowData>> DeviceRows;
	TSharedPtr<SListView<TSharedPtr<FLiveLinkDeviceRowData>>> DeviceListView;

	FOnSelectionChangedDelegate OnSelectionChangedDelegate;
};


class SLiveLinkDeviceRowWidget : public SMultiColumnTableRow<TSharedPtr<FLiveLinkDeviceRowData>>
{
	SLATE_BEGIN_ARGS(SLiveLinkDeviceRowWidget) {}
	SLATE_END_ARGS()

public:
	void Construct(
		const FArguments& InArgs,
		TSharedPtr<FLiveLinkDeviceRowData> InRowData,
		const TSharedRef<STableViewBase>& InOwnerTableView
	);

	//~ Begin SMultiColumnTableRow interface
	TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnId) override;
	//~ End SMultiColumnTableRow interface

	ULiveLinkDevice* GetDevice() { return RowData ? RowData->WeakDevice.Get() : nullptr; }
	const ULiveLinkDevice* GetDevice() const { return RowData ? RowData->WeakDevice.Get() : nullptr; }

private:
	TSharedPtr<FLiveLinkDeviceRowData> RowData;
};
