// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class UMetaHumanAssetReport;

namespace UE::MetaHuman
{
class SReportDataItem;

// Top-level navigation UI presenting a list of collapsible sections each with a tree underneath
class SMetaHumanAssetReportView final : public SCompoundWidget
{
public:
	enum class EReportType: int32
	{
		Verification,
		Import
	};

	SLATE_BEGIN_ARGS(SMetaHumanAssetReportView)
		: _ReportType(EReportType::Verification)
		{
		}
		SLATE_ARGUMENT(EReportType, ReportType)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetReport(UMetaHumanAssetReport* ReportToDisplay);

private:
	void OnClickItem(TSharedPtr<SReportDataItem> Item) const;
	EVisibility GetSaveButtonVisibility() const;
	FReply OnSaveButtonClicked();
	const FSlateBrush* GetIconForHeader() const;
	FText GetTextForHeader() const;

	TStrongObjectPtr<UMetaHumanAssetReport> Report;
	TArray<TSharedPtr<SReportDataItem>> ReportData;
	TSharedPtr<STreeView<TSharedPtr<SReportDataItem>>> ReportItemsTreeView;
	EReportType ReportType = EReportType::Verification;
};
}
