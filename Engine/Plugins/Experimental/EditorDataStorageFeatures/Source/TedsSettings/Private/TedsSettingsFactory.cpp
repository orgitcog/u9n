// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsSettingsFactory.h"

#include "TedsSettingsColumns.h"
#include "TedsSettingsWidgets.h"

void UTedsSettingsFactory::RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage, UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage::Queries;

	RowHandle PurposeRow = DataStorageUi.FindPurpose(DataStorageUi.GetGeneralWidgetPurposeID());
	
	DataStorageUi.RegisterWidgetFactory<FSettingsContainerReferenceWidgetConstructor>(PurposeRow, TColumn<FSettingsContainerReferenceColumn>());
	DataStorageUi.RegisterWidgetFactory<FSettingsCategoryReferenceWidgetConstructor>(PurposeRow, TColumn<FSettingsCategoryReferenceColumn>());
	DataStorageUi.RegisterWidgetFactory<FSettingsSectionWidgetConstructor>(PurposeRow, TColumn<FSettingsSectionTag>());
}
