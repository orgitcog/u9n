// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/NameWidget.h"

#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NameWidget"

void UNameWidgetFactory::RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage,
	UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage::Queries;
	DataStorageUi.RegisterWidgetFactory<FNameWidgetConstructor>(
		DataStorageUi.FindPurpose(DataStorageUi.GetGeneralWidgetPurposeID()),
		TColumn<FNameColumn>());
}

FNameWidgetConstructor::FNameWidgetConstructor()
	: FSimpleWidgetConstructor(StaticStruct())
{
}

TSharedPtr<SWidget> FNameWidgetConstructor::CreateWidget(UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::IUiProvider* DataStorageUi, UE::Editor::DataStorage::RowHandle TargetRow, UE::Editor::DataStorage::RowHandle WidgetRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	UE::Editor::DataStorage::FAttributeBinder Binder(TargetRow, DataStorage);

	return SNew(STextBlock)
				.Text(Binder.BindText(&FNameColumn::Name))
				.ToolTipText(Binder.BindText(&FNameColumn::Name));
}

#undef LOCTEXT_NAMESPACE
