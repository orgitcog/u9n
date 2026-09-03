// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Templates/SharedPointer.h"

class ISettingsCategory;

class FTedsSettingsManager final : public TSharedFromThis<FTedsSettingsManager>
{
public:

	FTedsSettingsManager();

	const bool IsInitialized() const
	{
		return bIsInitialized;
	}

	void Initialize();
	void Shutdown();

	UE::Editor::DataStorage::RowHandle FindOrAddSettingsSection(const FName& ContainerName, const FName& CategoryName, const FName& SectionName);

	bool GetSettingsSectionFromRow(UE::Editor::DataStorage::RowHandle Row, FName& OutContainerName, FName& OutCategoryName, FName& OutSectionName);

private:

	void RegisterTables(UE::Editor::DataStorage::ICoreProvider& DataStorage);

	void RegisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage);
	void UnregisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage);

	void RegisterActiveSettings();
	void UnregisterActiveSettings();
	void UnregisterInactiveSettings();

	void RegisterSettingsContainer(const FName& ContainerName);

	void UpdateSettingsCategory(TSharedPtr<ISettingsCategory> SettingsCategory, UE::Editor::DataStorage::RowHandle ContainerRow, const bool bQueryExistingRows = true);

	void AddColumns(UE::Editor::DataStorage::ICoreProvider& DataStorage, UE::Editor::DataStorage::RowHandle Row, const TArray<void*>& Columns, const TArray<const UScriptStruct*>& ColumnTypes);

	bool bIsInitialized;
	UE::Editor::DataStorage::QueryHandle SelectAllActiveSettingsQuery;
	UE::Editor::DataStorage::QueryHandle SelectAllInactiveSettingsQuery;
	UE::Editor::DataStorage::TableHandle SettingsContainerTable;
	UE::Editor::DataStorage::TableHandle SettingsCategoryTable;
	UE::Editor::DataStorage::TableHandle SettingsInactiveSectionTable;

};
