// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AssetDataVirtualPathWidget.h"

#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "Internationalization/Text.h"
#include "TedsAssetDataColumns.h"
#include "TedsAssetDataHelper.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

void UAssetDataVirtualPathWidgetFactory::RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage,
	UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage::Queries;
	DataStorageUi.RegisterWidgetFactory<FAssetDataVirtualPathWidgetConstructor>(
		DataStorageUi.FindPurpose(DataStorageUi.GetGeneralWidgetPurposeID()),
		TColumn<FVirtualPathColumn_Experimental>() || TColumn<FAssetPathColumn_Experimental>());
}

FAssetDataVirtualPathWidgetConstructor::FAssetDataVirtualPathWidgetConstructor()
	: FSimpleWidgetConstructor(StaticStruct())
{
}

TSharedPtr<SWidget> FAssetDataVirtualPathWidgetConstructor::CreateWidget(UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::IUiProvider* DataStorageUi, UE::Editor::DataStorage::RowHandle TargetRow, UE::Editor::DataStorage::RowHandle WidgetRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	static const FMargin ColumnItemPadding(5, 0, 5, 0);

	bool bIsAsset = DataStorage->HasColumns<FAssetTag>(TargetRow);
	TAttribute<FText> PathText = FText::GetEmpty();

	if (bIsAsset)
	{
		if (const FVirtualPathColumn_Experimental* VirtualPathColumn = DataStorage->GetColumn<FVirtualPathColumn_Experimental>(TargetRow))
		{
			FString VirtualPathString = VirtualPathColumn->VirtualPath.ToString();
			VirtualPathString = TedsAssetDataHelper::RemoveSlashFromStart(VirtualPathString);
			PathText = FText::FromString(VirtualPathString);
		}
	}
	else
	{
		if (const FAssetPathColumn_Experimental* AssetPathColumn = DataStorage->GetColumn<FAssetPathColumn_Experimental>(TargetRow))
		{
			FString AssetPathString = AssetPathColumn->Path.ToString();
			AssetPathString = TedsAssetDataHelper::RemoveSlashFromStart(AssetPathString);
			AssetPathString = TedsAssetDataHelper::RemoveAllFromLastSlash(AssetPathString);
			PathText = FText::FromString(AssetPathString);
		}
	}

	return SNew(SBox)
			.Padding(ColumnItemPadding)
			[
				SNew(STextBlock)
				.Text(PathText)
			];
}
