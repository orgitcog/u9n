// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerLabelWidget.h"

#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Layout/Margin.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "OutlinerLabelWidget"

void UOutlinerLabelWidgetFactory::RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage,
	UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage::Queries;
	DataStorageUi.RegisterWidgetFactory<FOutlinerLabelWidgetConstructor>(
		DataStorageUi.FindPurpose(UE::Editor::DataStorage::IUiProvider::FPurposeInfo("SceneOutliner", "RowLabel", NAME_None).GeneratePurposeID()),
		TColumn<FTypedElementLabelColumn>() && (TColumn<FTypedElementClassTypeInfoColumn>() || TColumn<FTypedElementScriptStructTypeInfoColumn>()));
}

void UOutlinerLabelWidgetFactory::RegisterWidgetPurposes(UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	DataStorageUi.RegisterWidgetPurpose(
	UE::Editor::DataStorage::IUiProvider::FPurposeInfo("SceneOutliner", "RowLabel", "Icon",
		UE::Editor::DataStorage::IUiProvider::EPurposeType::UniqueByNameAndColumn,
		LOCTEXT("IconItemCellWidgetPurpose", "The icon widget to use in cells for the Scene Outliner specific to the Item label column.")));

	DataStorageUi.RegisterWidgetPurpose(
	UE::Editor::DataStorage::IUiProvider::FPurposeInfo("SceneOutliner", "RowLabel", "Text",
		UE::Editor::DataStorage::IUiProvider::EPurposeType::UniqueByNameAndColumn,
		LOCTEXT("TextItemCellWidgetPurpose", "The text widget to use in cells for the Scene Outliner specific to the Item label column.")));
}

static void GetAllColumns(TArray<TWeakObjectPtr<const UScriptStruct>>& OutColumns, const UE::Editor::DataStorage::ICoreProvider& DataStorage, UE::Editor::DataStorage::RowHandle Row)
{
	OutColumns.Empty();
	DataStorage.ListColumns(Row, [&OutColumns](const UScriptStruct& ColumnType)
		{
			OutColumns.Emplace(&ColumnType);
			return true;
		});
}

FOutlinerLabelWidgetConstructor::FOutlinerLabelWidgetConstructor()
	: FSimpleWidgetConstructor(StaticStruct())
{
}

TSharedPtr<SWidget> FOutlinerLabelWidgetConstructor::CreateWidget(UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::IUiProvider* DataStorageUi, UE::Editor::DataStorage::RowHandle TargetRow,
	UE::Editor::DataStorage::RowHandle WidgetRow, const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	if (DataStorage->IsRowAvailable(TargetRow))
	{
		TSharedPtr<FTypedElementWidgetConstructor> OutIconWidgetConstructorPtr;
		TSharedPtr<FTypedElementWidgetConstructor> OutTextWidgetConstructorPtr;

		auto AssignIconWidgetToColumn = [&OutIconWidgetConstructorPtr](TUniquePtr<FTypedElementWidgetConstructor> Constructor, TConstArrayView<TWeakObjectPtr<const UScriptStruct>>)
			{
				OutIconWidgetConstructorPtr = TSharedPtr<FTypedElementWidgetConstructor>(Constructor.Release());
				return false;
			};

		auto AssignTextWidgetToColumn = [&OutTextWidgetConstructorPtr](TUniquePtr<FTypedElementWidgetConstructor> Constructor, TConstArrayView<TWeakObjectPtr<const UScriptStruct>>)
			{
				OutTextWidgetConstructorPtr = TSharedPtr<FTypedElementWidgetConstructor>(Constructor.Release());
				return false;
			};

		TArray<TWeakObjectPtr<const UScriptStruct>> Columns;
		GetAllColumns(Columns, *DataStorage, TargetRow);
		DataStorageUi->CreateWidgetConstructors(
			DataStorageUi->FindPurpose(UE::Editor::DataStorage::IUiProvider::FPurposeInfo("SceneOutliner", "RowLabel", "Icon").GeneratePurposeID()),
			UE::Editor::DataStorage::IUiProvider::EMatchApproach::LongestMatch, Columns, Arguments, AssignIconWidgetToColumn);
		GetAllColumns(Columns, *DataStorage, TargetRow);
		DataStorageUi->CreateWidgetConstructors(
			DataStorageUi->FindPurpose(UE::Editor::DataStorage::IUiProvider::FPurposeInfo("SceneOutliner", "RowLabel", "Text").GeneratePurposeID()),
			UE::Editor::DataStorage::IUiProvider::EMatchApproach::LongestMatch, Columns, Arguments, AssignTextWidgetToColumn);

		static const FMargin ColumnItemPadding(8, 0);
		constexpr float IconNameHorizontalPadding = 8.f;

		TSharedPtr<SWidget> IconWidget = SNullWidget::NullWidget;
		TSharedPtr<SWidget> TextWidget = SNullWidget::NullWidget;

		if (OutIconWidgetConstructorPtr)
		{
			IconWidget = DataStorageUi->ConstructWidget(WidgetRow, *OutIconWidgetConstructorPtr, Arguments);
		}

		if (OutTextWidgetConstructorPtr)
		{
			TextWidget = DataStorageUi->ConstructWidget(WidgetRow, *OutTextWidgetConstructorPtr, Arguments);
		}

		if (!IconWidget.IsValid())
		{
			IconWidget = SNullWidget::NullWidget;
		}

		if (!TextWidget.IsValid())
		{
			TextWidget = SNullWidget::NullWidget;
		}

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				IconWidget.ToSharedRef()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpacer)
					.Size(FVector2D(5.0f, 0.0f))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				TextWidget.ToSharedRef()
			];
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
