// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"

class UMetaHumanAssetReport;

namespace UE::MetaHuman
{
class SImportItemView;

struct FImportResult
{
	TStrongObjectPtr<UMetaHumanAssetReport> Report;
	TStrongObjectPtr<UObject> Target;
};

class SImportSummary final : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SImportSummary)
		{
		}
		SLATE_ARGUMENT(TArray<TSharedPtr<FImportResult>>, ImportResults)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void ChangeSelection(TSharedPtr<FImportResult> Item);
	TArray<TSharedPtr<FImportResult>> ImportResults;
	TSharedPtr<SImportItemView> ItemView;
};
} // namespace UE::MetaHuman
