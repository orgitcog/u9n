// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsSettingsManager.h"

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Common/EditorDataStorageFeatures.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TedsSettingsColumns.h"
#include "TedsSettingsLog.h"
#include "UObject/UnrealType.h"

namespace UE::Editor::Settings::Private
{
	static UE::Editor::DataStorage::FMapKey GenerateIndexKey(const FName& ContainerName, const FName& CategoryName, const FName& SectionName)
	{
		FString Key = "ISettingsSection: ";
		ContainerName.AppendString(Key);
		Key.AppendChar(TEXT(','));
		CategoryName.AppendString(Key);
		Key.AppendChar(TEXT(','));
		SectionName.AppendString(Key);
	
		return UE::Editor::DataStorage::FMapKey(MoveTemp(Key));
	}

	static bool IsUnknownColumn(const UScriptStruct* ColumnType)
	{
		check(ColumnType);
		return ColumnType != FSettingsSectionTag::StaticStruct()
			&& ColumnType != FSettingsInactiveSectionTag::StaticStruct()
			&& ColumnType != FSettingsContainerReferenceColumn::StaticStruct()
			&& ColumnType != FSettingsCategoryReferenceColumn::StaticStruct()
			&& ColumnType != FNameColumn::StaticStruct()
			&& ColumnType != FDisplayNameColumn::StaticStruct()
			&& ColumnType != FDescriptionColumn::StaticStruct()
			&& ColumnType != FTypedElementUObjectColumn::StaticStruct()
			&& ColumnType != FTypedElementUObjectIdColumn::StaticStruct()
			&& ColumnType != FTypedElementClassTypeInfoColumn::StaticStruct()
			&& ColumnType != FTypedElementClassDefaultObjectTag::StaticStruct();
	}
}

FTedsSettingsManager::FTedsSettingsManager()
	: bIsInitialized{ false }
	, SelectAllActiveSettingsQuery{ UE::Editor::DataStorage::InvalidQueryHandle }
	, SelectAllInactiveSettingsQuery{ UE::Editor::DataStorage::InvalidQueryHandle }
	, SettingsContainerTable{ UE::Editor::DataStorage::InvalidTableHandle }
	, SettingsCategoryTable{ UE::Editor::DataStorage::InvalidTableHandle }
	, SettingsInactiveSectionTable{ UE::Editor::DataStorage::InvalidTableHandle }
{
}

void FTedsSettingsManager::Initialize()
{
	if (!bIsInitialized)
	{
		using namespace UE::Editor::DataStorage;
		auto OnDataStorage = [this]
			{
				ICoreProvider* DataStorage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
				check(DataStorage);

				RegisterTables(*DataStorage);
				RegisterQueries(*DataStorage);
				RegisterActiveSettings();
			};

		if (AreEditorDataStorageFeaturesEnabled())
		{
			OnDataStorage();
		}
		else
		{
			OnEditorDataStorageFeaturesEnabled().AddSPLambda(this, OnDataStorage);
		}

		bIsInitialized = true;
	}
}

void FTedsSettingsManager::Shutdown()
{
	if (bIsInitialized)
	{
		using namespace UE::Editor::DataStorage;
		OnEditorDataStorageFeaturesEnabled().RemoveAll(this);

		if (AreEditorDataStorageFeaturesEnabled())
		{
			ICoreProvider* DataStorage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
			check(DataStorage);

			UnregisterActiveSettings();
			UnregisterInactiveSettings();
			UnregisterQueries(*DataStorage);
		}

		bIsInitialized = false;
	}
}

UE::Editor::DataStorage::RowHandle FTedsSettingsManager::FindOrAddSettingsSection(const FName& ContainerName, const FName& CategoryName, const FName& SectionName)
{
	using namespace UE::Editor::DataStorage;
	using namespace UE::Editor::Settings::Private;

	if (bIsInitialized)
	{
		if (ICoreProvider* DataStorage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName))
		{
			// Ensure that the SettingsInactiveSectionTable is registered since it is possible for OnDataStorageInterfacesSet to be called
			// for a delegate bound in another module before it has been called for our bound delegate.
			RegisterTables(*DataStorage);

			FMapKey SectionIndexKey = GenerateIndexKey(ContainerName, CategoryName, SectionName);

			RowHandle SectionRow = DataStorage->LookupMappedRow(SectionIndexKey);

			if (SectionRow == InvalidRowHandle)
			{
				SectionRow = DataStorage->AddRow(SettingsInactiveSectionTable);

				DataStorage->AddColumn<FSettingsInactiveSectionTag>(SectionRow);
				DataStorage->AddColumn<FNameColumn>(SectionRow, { .Name = SectionName });
				DataStorage->AddColumn<FSettingsContainerReferenceColumn>(SectionRow, { .ContainerName = ContainerName, .ContainerRow = InvalidRowHandle });
				DataStorage->AddColumn<FSettingsCategoryReferenceColumn>(SectionRow, { .CategoryName = CategoryName, .CategoryRow = InvalidRowHandle });

				DataStorage->MapRow(MoveTemp(SectionIndexKey), SectionRow);
			}

			return SectionRow;
		}
	}

	return InvalidRowHandle;
}

bool FTedsSettingsManager::GetSettingsSectionFromRow(UE::Editor::DataStorage::RowHandle Row, FName& OutContainerName, FName& OutCategoryName, FName& OutSectionName)
{
	using namespace UE::Editor::DataStorage;

	if (bIsInitialized)
	{
		if (const ICoreProvider* DataStorage = GetDataStorageFeature<ICoreProvider>(StorageFeatureName))
		{
			if (DataStorage->IsRowAvailable(Row) && (DataStorage->HasColumns<FSettingsSectionTag>(Row) || DataStorage->HasColumns<FSettingsInactiveSectionTag>(Row)))
			{
				const FSettingsContainerReferenceColumn* ContainerReferenceColumn = DataStorage->GetColumn<FSettingsContainerReferenceColumn>(Row);
				const FSettingsCategoryReferenceColumn* CategoryReferenceColumn = DataStorage->GetColumn<FSettingsCategoryReferenceColumn>(Row);
				const FNameColumn* NameColumn = DataStorage->GetColumn<FNameColumn>(Row);

				if (ContainerReferenceColumn && CategoryReferenceColumn && NameColumn)
				{
					OutContainerName = ContainerReferenceColumn->ContainerName;
					OutCategoryName = CategoryReferenceColumn->CategoryName;
					OutSectionName = NameColumn->Name;

					return true;
				}
			}
		}
	}

	return false;
}

void FTedsSettingsManager::RegisterTables(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage;
	if (SettingsContainerTable == InvalidTableHandle)
	{
		SettingsContainerTable = DataStorage.RegisterTable(
			TTypedElementColumnTypeList<FNameColumn, FDisplayNameColumn, FDescriptionColumn, FSettingsContainerTag>(),
			FName(TEXT("Editor_SettingsContainerTable")));
	}

	if (SettingsCategoryTable == InvalidTableHandle)
	{
		SettingsCategoryTable = DataStorage.RegisterTable(
			TTypedElementColumnTypeList<FSettingsContainerReferenceColumn, FNameColumn, FDisplayNameColumn, FDescriptionColumn, FSettingsCategoryTag>(),
			FName(TEXT("Editor_SettingsCategoryTable")));
	}

	if (SettingsInactiveSectionTable == InvalidTableHandle)
	{
		SettingsInactiveSectionTable = DataStorage.RegisterTable(
			TTypedElementColumnTypeList<FSettingsContainerReferenceColumn, FSettingsCategoryReferenceColumn, FNameColumn, FSettingsInactiveSectionTag>(),
			FName(TEXT("Editor_SettingsInactiveSectionTable")));
	}
}

void FTedsSettingsManager::RegisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;

	if (SelectAllActiveSettingsQuery == UE::Editor::DataStorage::InvalidQueryHandle)
	{
		SelectAllActiveSettingsQuery = DataStorage.RegisterQuery(
			Select()
				.ReadOnly<FSettingsContainerReferenceColumn, FSettingsCategoryReferenceColumn, FNameColumn>()
			.Where()
				.All<FSettingsSectionTag>()
			.Compile());
	}

	if (SelectAllInactiveSettingsQuery == UE::Editor::DataStorage::InvalidQueryHandle)
	{
		SelectAllInactiveSettingsQuery = DataStorage.RegisterQuery(
			Select()
				.ReadOnly<FSettingsContainerReferenceColumn, FSettingsCategoryReferenceColumn, FNameColumn>()
			.Where()
				.All<FSettingsInactiveSectionTag>()
			.Compile());
	}
}

void FTedsSettingsManager::UnregisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	DataStorage.UnregisterQuery(SelectAllActiveSettingsQuery);
	SelectAllActiveSettingsQuery = UE::Editor::DataStorage::InvalidQueryHandle;

	DataStorage.UnregisterQuery(SelectAllInactiveSettingsQuery);
	SelectAllInactiveSettingsQuery = UE::Editor::DataStorage::InvalidQueryHandle;
}

void FTedsSettingsManager::RegisterActiveSettings()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.RegisterActiveSettings);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	check(SettingsModule);

	TArray<FName> ContainerNames;
	SettingsModule->GetContainerNames(ContainerNames);

	for (FName ContainerName : ContainerNames)
	{
		RegisterSettingsContainer(ContainerName);
	}

	SettingsModule->OnContainerAdded().AddSP(this, &FTedsSettingsManager::RegisterSettingsContainer);
}

void FTedsSettingsManager::RegisterSettingsContainer(const FName& ContainerName)
{
	using namespace UE::Editor::DataStorage;

	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.RegisterSettingsContainer);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	check(SettingsModule);

	ICoreProvider* DataStorage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
	check(DataStorage);

	UE_LOG(LogTedsSettings, Verbose, TEXT("Register Settings Container : '%s'"), *ContainerName.ToString());

	ISettingsContainerPtr ContainerPtr = SettingsModule->GetContainer(ContainerName);

	FMapKey ContainerIndexKey = FMapKey(ContainerPtr.Get());
	RowHandle ContainerRow = DataStorage->LookupMappedRow(ContainerIndexKey);
	if (ContainerRow == InvalidRowHandle)
	{
		ContainerRow = DataStorage->AddRow(SettingsContainerTable);
		DataStorage->AddColumn<FNameColumn>(ContainerRow, { .Name = ContainerName });
		DataStorage->AddColumn<FDisplayNameColumn>(ContainerRow, { .DisplayName = ContainerPtr->GetDisplayName() });
		DataStorage->AddColumn<FDescriptionColumn>(ContainerRow, { .Description = ContainerPtr->GetDescription() });
		DataStorage->AddColumn<FSettingsContainerTag>(ContainerRow);

		DataStorage->MapRow(MoveTemp(ContainerIndexKey), ContainerRow);
	}

	TArray<ISettingsCategoryPtr> Categories;
	ContainerPtr->GetCategories(Categories);

	for (ISettingsCategoryPtr CategoryPtr : Categories)
	{
		const bool bQueryExistingRows = false;
		UpdateSettingsCategory(CategoryPtr, ContainerRow, bQueryExistingRows);
	}

	// OnCategoryModified is called at the same time as OnSectionRemoved so we only bind to OnCategoryModified for add / update / remove
	ContainerPtr->OnCategoryModified().AddSPLambda(this, [this, ContainerPtr, ContainerRow](const FName& ModifiedCategoryName)
		{
			UE_LOG(LogTedsSettings, Verbose, TEXT("Settings Category modified : '%s->%s'"), *ContainerPtr->GetName().ToString(), *ModifiedCategoryName.ToString());

			ISettingsCategoryPtr CategoryPtr = ContainerPtr->GetCategory(ModifiedCategoryName);

			UpdateSettingsCategory(CategoryPtr, ContainerRow);
		});
}

void FTedsSettingsManager::UnregisterActiveSettings()
{
	using namespace UE::Editor::DataStorage;
	using namespace UE::Editor::Settings::Private;

	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.UnregisterActiveSettings);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	check(SettingsModule);

	ICoreProvider* DataStorage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
	check(DataStorage);

	ICompatibilityProvider* DataStorageCompatibility = GetMutableDataStorageFeature<ICompatibilityProvider>(CompatibilityFeatureName);
	check(DataStorageCompatibility);

	SettingsModule->OnContainerAdded().RemoveAll(this);

	TArray<FName> ContainerNames;
	SettingsModule->GetContainerNames(ContainerNames);

	for (FName ContainerName : ContainerNames)
	{
		UE_LOG(LogTedsSettings, Verbose, TEXT("Unregister Settings Container : '%s'"), *ContainerName.ToString());

		ISettingsContainerPtr ContainerPtr = SettingsModule->GetContainer(ContainerName);

		ContainerPtr->OnCategoryModified().RemoveAll(this);

		TArray<ISettingsCategoryPtr> Categories;
		ContainerPtr->GetCategories(Categories);

		for (ISettingsCategoryPtr CategoryPtr : Categories)
		{
			UE_LOG(LogTedsSettings, Verbose, TEXT("Unregister Settings Category : '%s'"), *CategoryPtr->GetName().ToString());

			TArray<ISettingsSectionPtr> Sections;
			const bool bIgnoreVisibility = true;
			CategoryPtr->GetSections(Sections, bIgnoreVisibility);

			for (ISettingsSectionPtr SectionPtr : Sections)
			{
				if (TStrongObjectPtr<UObject> SettingsObjectPtr = SectionPtr->GetSettingsObject().Pin(); SettingsObjectPtr)
				{
					DataStorageCompatibility->RemoveCompatibleObject(SettingsObjectPtr);

					UE_LOG(LogTedsSettings, Verbose, TEXT("Removed Settings Section : '%s'"), *SectionPtr->GetName().ToString());
				}
			}

			FMapKeyView CategoryIndexKey = FMapKeyView(CategoryPtr.Get());
			RowHandle CategoryRow = DataStorage->LookupMappedRow(CategoryIndexKey);
			if (CategoryRow != InvalidRowHandle)
			{
				DataStorage->RemoveRow(CategoryRow);
			}
		}

		FMapKeyView ContainerIndexKey = FMapKeyView(ContainerPtr.Get());
		RowHandle ContainerRow = DataStorage->LookupMappedRow(ContainerIndexKey);
		if (ContainerRow != InvalidRowHandle)
		{
			DataStorage->RemoveRow(ContainerRow);
		}
	}
}

void FTedsSettingsManager::UnregisterInactiveSettings()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.UnregisterInactiveSettings);

	using namespace UE::Editor::DataStorage::Queries;
	using namespace UE::Editor::Settings::Private;

	ICoreProvider* DataStorage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
	check(DataStorage);
	
	TSet<RowHandle> RowHandles;

	DataStorage->RunQuery(SelectAllInactiveSettingsQuery, CreateDirectQueryCallbackBinding(
		[&RowHandles](IDirectQueryContext& Context, const RowHandle*)
		{
			RowHandles.Append(Context.GetRowHandles());
		}));

	for (const RowHandle& Row : RowHandles)
	{
		DataStorage->RemoveRow(Row);
	}
}

void FTedsSettingsManager::UpdateSettingsCategory(TSharedPtr<ISettingsCategory> SettingsCategory, UE::Editor::DataStorage::RowHandle ContainerRow, const bool bQueryExistingRows)
{
	using namespace UE::Editor::DataStorage;
	using namespace UE::Editor::Settings::Private;

	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.UpdateSettingsCategory);

	ICoreProvider* DataStorage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
	check(DataStorage);

	ICompatibilityProvider* DataStorageCompatibility = GetMutableDataStorageFeature<ICompatibilityProvider>(CompatibilityFeatureName);
	check(DataStorageCompatibility);

	const FName& ContainerName = DataStorage->GetColumn<FNameColumn>(ContainerRow)->Name;
	const FName& CategoryName = SettingsCategory->GetName();

	UE_LOG(LogTedsSettings, Verbose, TEXT("Update Settings Category: '%s->%s'"), *ContainerName.ToString(), *CategoryName.ToString());

	FMapKey CategoryIndexKey = FMapKey(SettingsCategory.Get());

	RowHandle CategoryRow = DataStorage->LookupMappedRow(CategoryIndexKey);
	if (CategoryRow == InvalidRowHandle)
	{
		CategoryRow = DataStorage->AddRow(SettingsCategoryTable);

		DataStorage->AddColumn<FSettingsContainerReferenceColumn>(CategoryRow, { .ContainerName = ContainerName, .ContainerRow = ContainerRow });
		DataStorage->AddColumn<FNameColumn>(CategoryRow, { .Name = CategoryName });
		DataStorage->AddColumn<FDisplayNameColumn>(CategoryRow, { .DisplayName = SettingsCategory->GetDisplayName() });
		DataStorage->AddColumn<FDescriptionColumn>(CategoryRow, { .Description = SettingsCategory->GetDescription() });
		DataStorage->AddColumn<FSettingsCategoryTag>(CategoryRow);

		DataStorage->MapRow(MoveTemp(CategoryIndexKey), CategoryRow);
	}

	TArray<RowHandle> OldRowHandles;
	TArray<FName> OldSectionNames;

	// Gather all existing active rows for the given { ContainerName, CategoryName } pair.
	if (bQueryExistingRows)
	{
		using namespace UE::Editor::DataStorage::Queries;

		DataStorage->RunQuery(SelectAllActiveSettingsQuery, CreateDirectQueryCallbackBinding(
			[&OldRowHandles, &OldSectionNames, &ContainerName, &CategoryName](
				IDirectQueryContext& Context,
				const FSettingsContainerReferenceColumn* ContainerColumns,
				const FSettingsCategoryReferenceColumn* CategoryColumns,
				const FNameColumn* SectionNameColumns)
			{
				const uint32 RowCount = Context.GetRowCount();

				for (uint32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
				{
					const FName& TempContainerName = ContainerColumns[RowIndex].ContainerName;
					const FName& TempCategoryName = CategoryColumns[RowIndex].CategoryName;
					if (TempContainerName == ContainerName &&
						TempCategoryName == CategoryName)
					{
						OldRowHandles.Emplace(Context.GetRowHandles()[RowIndex]);
						OldSectionNames.Emplace(SectionNameColumns[RowIndex].Name);
					}
				}
			}));
	}

	TArray<FName> NewSectionNames;
	TArray<ISettingsSectionPtr> NewSections;

	const bool bIgnoreVisibility = true;
	SettingsCategory->GetSections(NewSections, bIgnoreVisibility);

	// Iterate the category and add rows for all sections ( replace any existing row for the section as its object may have changed )
	for (ISettingsSectionPtr SectionPtr : NewSections)
	{
		const FName& SectionName = SectionPtr->GetName();

		if (TStrongObjectPtr<UObject> SettingsObjectPtr = SectionPtr->GetSettingsObject().Pin(); SettingsObjectPtr)
		{
			NewSectionNames.Emplace(SectionName);

			FMapKey SectionIndexKey = GenerateIndexKey(ContainerName, CategoryName, SectionName);

			TArray<void*> ColumnsToCopy;
			TArray<const UScriptStruct*> ColumnTypesToCopy;

			RowHandle OldSectionRow = DataStorage->LookupMappedRow(SectionIndexKey);
			if (OldSectionRow != InvalidRowHandle)
			{
				UE_LOG(LogTedsSettings, Verbose, TEXT("Settings Section : '%s' is already in data storage"), *SectionName.ToString());
				
				// Capture unknown columns and copy them to the new row before we delete the old row.
				DataStorage->ListColumns(OldSectionRow, [&ColumnsToCopy, &ColumnTypesToCopy](void* Column, const UScriptStruct& ColumnType)
					{
						if (IsUnknownColumn(&ColumnType))
						{
							ColumnsToCopy.Add(Column);
							ColumnTypesToCopy.Add(&ColumnType);
						}
					});
			}

			RowHandle NewSectionRow = DataStorageCompatibility->AddCompatibleObject(SettingsObjectPtr);

			DataStorage->AddColumn<FSettingsSectionTag>(NewSectionRow);
			DataStorage->AddColumn<FSettingsContainerReferenceColumn>(NewSectionRow, { .ContainerName = ContainerName, .ContainerRow = ContainerRow });
			DataStorage->AddColumn<FSettingsCategoryReferenceColumn>(NewSectionRow, { .CategoryName = CategoryName, .CategoryRow = CategoryRow });
			DataStorage->AddColumn<FNameColumn>(NewSectionRow, { .Name = SectionName });
			DataStorage->AddColumn<FDisplayNameColumn>(NewSectionRow, { .DisplayName = SectionPtr->GetDisplayName() });
			DataStorage->AddColumn<FDescriptionColumn>(NewSectionRow, { .Description = SectionPtr->GetDescription() });
			
			if (OldSectionRow != InvalidRowHandle && OldSectionRow != NewSectionRow)
			{
				AddColumns(*DataStorage, NewSectionRow, ColumnsToCopy, ColumnTypesToCopy);

				// Remove the old row after we copy the unknown columns to the new row.
				DataStorage->RemoveRow(OldSectionRow);

				UE_LOG(LogTedsSettings, Verbose, TEXT("Removed Settings Section : '%s'"), *SectionName.ToString());
			}
			
			UE_CLOG(OldSectionRow != NewSectionRow, LogTedsSettings, Verbose, TEXT("Added Settings Section : '%s'"), *SectionName.ToString());
			UE_CLOG(OldSectionRow == NewSectionRow, LogTedsSettings, Verbose, TEXT("Updated Settings Section : '%s'"), *SectionName.ToString());

			DataStorage->MapRow(MoveTemp(SectionIndexKey), NewSectionRow);			
		}
	}

	// Iterate the old active sections and inactivate rows not in the new active sections list.
	for (int32 RowIndex = 0; RowIndex < OldSectionNames.Num(); ++RowIndex)
	{
		const FName& OldSectionName = OldSectionNames[RowIndex];

		if (NewSectionNames.Contains(OldSectionName))
		{
			continue;
		}

		RowHandle OldSectionRow = OldRowHandles[RowIndex];
		check(OldSectionRow != InvalidRowHandle);

		// Add an inactive row for the settings section.
		RowHandle NewSectionRow = DataStorage->AddRow(SettingsInactiveSectionTable);

		DataStorage->AddColumn<FSettingsInactiveSectionTag>(NewSectionRow);
		DataStorage->AddColumn<FNameColumn>(NewSectionRow, { .Name = OldSectionName });
		DataStorage->AddColumn<FSettingsContainerReferenceColumn>(NewSectionRow, { .ContainerName = ContainerName, .ContainerRow = InvalidRowHandle });
		DataStorage->AddColumn<FSettingsCategoryReferenceColumn>(NewSectionRow, { .CategoryName = CategoryName, .CategoryRow = InvalidRowHandle });

		// Capture unknown columns and copy them to the new row before we delete the old row.
		TArray<void*> ColumnsToCopy;
		TArray<const UScriptStruct*> ColumnTypesToCopy;		
		
		DataStorage->ListColumns(OldSectionRow, [&ColumnsToCopy, &ColumnTypesToCopy](void* Column, const UScriptStruct& ColumnType)
			{
				if (IsUnknownColumn(&ColumnType))
				{
					ColumnsToCopy.Add(Column);
					ColumnTypesToCopy.Add(&ColumnType);
				}
			});

		AddColumns(*DataStorage, NewSectionRow, ColumnsToCopy, ColumnTypesToCopy);

		DataStorage->RemoveRow(OldSectionRow);

		DataStorage->MapRow(GenerateIndexKey(ContainerName, CategoryName, OldSectionName), NewSectionRow);

		UE_LOG(LogTedsSettings, Verbose, TEXT("Inactivated Settings Section : '%s'"), *OldSectionName.ToString());
	}
}

void FTedsSettingsManager::AddColumns(UE::Editor::DataStorage::ICoreProvider& DataStorage, UE::Editor::DataStorage::RowHandle Row, const TArray<void*>& Columns, const TArray<const UScriptStruct*>& ColumnTypes)
{
	for (int32 ColumnIndex = 0; ColumnIndex < Columns.Num(); ++ColumnIndex)
	{
		void* SourceColumn = Columns[ColumnIndex];
		const UScriptStruct* SourceColumnType = ColumnTypes[ColumnIndex];

		DataStorage.AddColumnData(Row, SourceColumnType,
			[SourceColumn](void* TargetColumn, const UScriptStruct& ColumnType)
			{
				ColumnType.CopyScriptStruct(TargetColumn, SourceColumn);
			},
			[](const UScriptStruct& ColumnType, void* Destination, void* Source)
			{
				ColumnType.CopyScriptStruct(Destination, Source);
			});
	}
}
