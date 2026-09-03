// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsAssetData.h"

#include "TedsAssetDataColumns.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Async/ParallelFor.h"
#include "Containers/Array.h"
#include "Containers/ChunkedArray.h"
#include "Elements/Columns/TypedElementFolderColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Common/TypedElementQueryTypes.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Engine/Engine.h"

#define TRACK_TEDSASSETDATA_MEMORY 0

namespace UE::Editor::AssetData::Private
{
using namespace DataStorage;

constexpr int32 ParallelForMinBatchSize = 1024 * 4;

struct FPopulateAssetDataRowArgs
{
	FAssetData AssetData;
	FMapKey ObjectPathKey;
};

// Only safe if the GT is blocked during the operation
template<typename TAssetData>
FPopulateAssetDataRowArgs ThreadSafe_PopulateAssetDataTableRow(TAssetData&& InAssetData, const ICoreProvider& Database)
{
	FPopulateAssetDataRowArgs Output;
	Output.ObjectPathKey = FMapKey(InAssetData.GetSoftObjectPath());

	if (Database.IsRowAssigned(Database.LookupMappedRow(Output.ObjectPathKey)))
	{
		// No need to initialize the rest of the row here. The invalid's asset data will be used as flag to skip the data generated here.
		return Output;
	}

	Output.AssetData = Forward<TAssetData>(InAssetData);

	return Output;
}


void PopulateAssetDataTableRow(FPopulateAssetDataRowArgs&& InAssetDataRowArgs, ICoreProvider& Database, RowHandle RowHandle)
{
	Database.GetColumn<FAssetDataColumn_Experimental>(RowHandle)->AssetData = MoveTemp(InAssetDataRowArgs.AssetData);
}

struct FPopulatePathRowArgs
{
	FName AssetRegistryPath;
	FMapKey AssetRegistryPathKey;
	FName AssetName;

	operator bool() const 
	{
		return !AssetRegistryPath.IsNone();
	}

	void MarkAsInvalid()
	{
		AssetRegistryPath = FName();
	}
};

void GetParentFolderIndex(FStringView Path, int32& OutParentFolderIndex)
{
	OutParentFolderIndex = INDEX_NONE;

	if (Path.Len() > 1)
	{
		OutParentFolderIndex = 1;
	}

	// Skip the first '/'
	for (int32 Index = 1; Index < Path.Len(); ++Index)
	{
		if (Path[Index] == TEXT('/'))
		{
			OutParentFolderIndex = Index;
		}
	}
}

// Only thread safe if the game thread is blocked 
FPopulatePathRowArgs ThreadSafe_PopulatePathRowArgs(FMapKey&& AssetRegistryPathKey, FName InAssetRegistryPath, FStringView PathAsString)
{
	int32 CharacterIndex;
	FStringView FolderName;
	GetParentFolderIndex(PathAsString, CharacterIndex);
	if (CharacterIndex != INDEX_NONE)
	{
		FolderName = PathAsString.RightChop(CharacterIndex);
	}

	FPopulatePathRowArgs Args;
	Args.AssetRegistryPath = InAssetRegistryPath;
	Args.AssetName = FName(FolderName);
	Args.AssetRegistryPathKey = MoveTemp(AssetRegistryPathKey);

	return Args;
}

void PopulatePathDataTableRow(FPopulatePathRowArgs&& InPopulatePathRowArgs, ICoreProvider& Database, RowHandle InRowHandle)
{
	Database.GetColumn<FAssetPathColumn_Experimental>(InRowHandle)->Path = InPopulatePathRowArgs.AssetRegistryPath;
	Database.GetColumn<FNameColumn>(InRowHandle)->Name = InPopulatePathRowArgs.AssetName;
}


FTedsAssetData::FTedsAssetData(UE::Editor::DataStorage::ICoreProvider& InDatabase)
	: Database(InDatabase)
{
	using namespace UE::Editor::DataStorage::Queries;
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::FTedsAssetData);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	// Register to events from asset registry
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	AssetRegistry.OnAssetsAdded().AddRaw(this, &FTedsAssetData::OnAssetsAdded);
	AssetRegistry.OnAssetsRemoved().AddRaw(this, &FTedsAssetData::OnAssetsRemoved);
	AssetRegistry.OnAssetsUpdated().AddRaw(this, &FTedsAssetData::OnAssetsUpdated);
	AssetRegistry.OnAssetRenamed().AddRaw(this, &FTedsAssetData::OnAssetRenamed);
	AssetRegistry.OnAssetsUpdatedOnDisk().AddRaw(this, &FTedsAssetData::OnAssetsUpdatedOnDisk);
	AssetRegistry.OnPathsAdded().AddRaw(this, &FTedsAssetData::OnPathsAdded);
	AssetRegistry.OnPathsRemoved().AddRaw(this, &FTedsAssetData::OnPathsRemoved);

	// Register data types to TEDS
	PathsTable = Database.FindTable(FName(TEXT("Editor_AssetRegistryPathsTable")));
	if (PathsTable == InvalidTableHandle)
	{
		PathsTable = Database.RegisterTable<FFolderTag, FAssetPathColumn_Experimental, FNameColumn, FUpdatedPathTag>(FName(TEXT("Editor_AssetRegistryPathsTable")));
	}

	AssetsDataTable = Database.FindTable(FName(TEXT("Editor_AssetRegistryAssetDataTable")));
	if (AssetsDataTable == InvalidTableHandle)
	{
		AssetsDataTable = Database.RegisterTable<FAssetDataColumn_Experimental, FUpdatedPathTag, FUpdatedAssetDataTag>(FName(TEXT("Editor_AssetRegistryAssetDataTable")));
	}


	RemoveUpdatedPathTagQuery = Database.RegisterQuery(
		Select(
			TEXT("FTedsAssetData: Remove Updated Path Tag"),
			FPhaseAmble(FPhaseAmble::ELocation::Postamble, EQueryTickPhase::FrameEnd),
			[](IQueryContext& Context, const RowHandle* Rows)
			{
				Context.RemoveColumns<FUpdatedPathTag>(TConstArrayView<RowHandle>(Rows, Context.GetRowCount()));
			})
		.Where()
			.All<FUpdatedPathTag>()
		.Compile());

	RemoveUpdatedAssetDataTagQuery = Database.RegisterQuery(
		Select(
			TEXT("FTedsAssetData: Remove Updated Asset Data Tag"),
			FPhaseAmble(FPhaseAmble::ELocation::Postamble, EQueryTickPhase::FrameEnd),
			[](IQueryContext& Context, const RowHandle* Rows)
			{
				Context.RemoveColumns<FUpdatedAssetDataTag>(TConstArrayView<RowHandle>(Rows, Context.GetRowCount()));
			})
		.Where()
			.All<FUpdatedAssetDataTag>()
		.Compile());

	// Init with the data existing at moment in asset registry
	TArray<FAssetData> AssetsData;
	AssetRegistry.GetAllAssets(AssetsData);

	TChunkedArray<FName> CachedPaths;
	AssetRegistry.EnumerateAllCachedPaths([&CachedPaths](FName Name)
		{
			CachedPaths.AddElement(Name);
			return true;
		});

	TArray<FNameBuilder> NameBuilders;
	TArray<FPopulatePathRowArgs> PopulatePathRowArgs;
	PopulatePathRowArgs.AddDefaulted(CachedPaths.Num());

	// Prepare Path Rows
	ParallelForWithTaskContext(TEXT("Populating TEDS Asset Registry Path"), NameBuilders, CachedPaths.Num(),ParallelForMinBatchSize, [&PopulatePathRowArgs, &CachedPaths, this](FNameBuilder& NameBuilder, int32 Index)
		{
			FName Path = CachedPaths[Index];
			Path.ToString(NameBuilder);
			PopulatePathRowArgs[Index] = ThreadSafe_PopulatePathRowArgs(FMapKey(Path), Path, NameBuilder);
		});

	TArray<RowHandle> ReservedRows;
	ReservedRows.AddUninitialized(PopulatePathRowArgs.Num() + AssetsData.Num());
	Database.BatchReserveRows(ReservedRows);

	TArray<TPair<FMapKey, RowHandle>> KeysToReservedRows;
	KeysToReservedRows.AddDefaulted(ReservedRows.Num());

	// Index Reserved Path Rows
	TConstArrayView<RowHandle> ReservedPopulatePathRows(ReservedRows.GetData(), PopulatePathRowArgs.Num());
	TArrayView<TPair<FMapKey, RowHandle>> KeysToReservedPathRows(KeysToReservedRows.GetData(), PopulatePathRowArgs.Num());
	ParallelFor(TEXT("Populating TEDS Asset Registry Path Data Keys"), ReservedPopulatePathRows.Num(), ParallelForMinBatchSize, [&KeysToReservedPathRows, &ReservedPopulatePathRows, &PopulatePathRowArgs](int32 Index)
		{
			KeysToReservedPathRows[Index] = TPair<FMapKey, RowHandle>(PopulatePathRowArgs[Index].AssetRegistryPathKey, ReservedPopulatePathRows[Index]);
		});
	Database.BatchMapRows(KeysToReservedPathRows);

	// Populate Path Rows
	Database.BatchAddRow(PathsTable, ReservedPopulatePathRows, [PathRowArgs = MoveTemp(PopulatePathRowArgs), Index = 0, this](RowHandle InRowHandle) mutable
		{
			PopulatePathDataTableRow(MoveTemp(PathRowArgs[Index]), Database, InRowHandle);
			++Index;
		});

	// Prepare Asset Data Rows
	TArray<FPopulateAssetDataRowArgs> PopulateAssetDataRowArgs;
	PopulateAssetDataRowArgs.AddDefaulted(AssetsData.Num());
	ParallelFor(TEXT("Populating TEDS Asset Registry Asset Data"), PopulateAssetDataRowArgs.Num(), ParallelForMinBatchSize, [&PopulateAssetDataRowArgs, &AssetsData, this](int32 Index)
		{
			PopulateAssetDataRowArgs[Index] = ThreadSafe_PopulateAssetDataTableRow(MoveTemp(AssetsData[Index]), Database);
		});

	// Index Reserved Asset Data Rows
	TConstArrayView<RowHandle> ReservedAssetDataRows(ReservedRows.GetData() + ReservedPopulatePathRows.Num(), AssetsData.Num());
	TArrayView<TPair<FMapKey, RowHandle>> KeysToResevedAssetRows(KeysToReservedRows.GetData() + ReservedPopulatePathRows.Num(), ReservedAssetDataRows.Num());
	ParallelFor(TEXT("Populating TEDS Asset Registry Asset Data Keys"), PopulateAssetDataRowArgs.Num(), ParallelForMinBatchSize, [&PopulateAssetDataRowArgs, &ReservedAssetDataRows, &KeysToResevedAssetRows](int32 Index)
		{
			KeysToResevedAssetRows[Index] = TPair<FMapKey, RowHandle>(MoveTemp(PopulateAssetDataRowArgs[Index].ObjectPathKey), ReservedAssetDataRows[Index]);
		});
	Database.BatchMapRows(KeysToResevedAssetRows);

	// Populate Asset Rows
	Database.BatchAddRow(AssetsDataTable, ReservedAssetDataRows,  [AssetDataRowArgs = MoveTemp(PopulateAssetDataRowArgs), Index = 0, this](RowHandle InRowHandle) mutable
		{
			PopulateAssetDataTableRow(MoveTemp(AssetDataRowArgs[Index]), Database, InRowHandle);
			++Index;
		});
}

FTedsAssetData::~FTedsAssetData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::~FTedsAssetData);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	// Not needed on a editor shut down
	if (!IsEngineExitRequested())
	{
		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			Database.UnregisterQuery(RemoveUpdatedAssetDataTagQuery);
			Database.UnregisterQuery(RemoveUpdatedPathTagQuery);


			AssetRegistry->OnAssetsAdded().RemoveAll(this);
			AssetRegistry->OnAssetsRemoved().RemoveAll(this);
			AssetRegistry->OnAssetsUpdated().RemoveAll(this);
			AssetRegistry->OnAssetsUpdatedOnDisk().RemoveAll(this);
			AssetRegistry->OnAssetRenamed().RemoveAll(this);
			AssetRegistry->OnPathsAdded().RemoveAll(this);
			AssetRegistry->OnPathsRemoved().RemoveAll(this);
	
			AssetRegistry->EnumerateAllCachedPaths([this](FName InPath)
				{
					const FMapKeyView PathKey = FMapKeyView(InPath);
					const RowHandle Row = Database.LookupMappedRow(PathKey);
					Database.RemoveRow(Row);
					return true;
				});

			AssetRegistry->EnumerateAllAssets([this](const FAssetData& InAssetData)
				{
					const FMapKey AssetPathKey = FMapKey(InAssetData.GetSoftObjectPath());
					const RowHandle Row = Database.LookupMappedRow(AssetPathKey);
					Database.RemoveRow(Row);
					return true;
				});
		}
	}
}

void FTedsAssetData::ProcessAllEvents()
{
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->Tick(-1.f);
	}
}

void FTedsAssetData::OnAssetsAdded(TConstArrayView<FAssetData> InAssetsAdded)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnAssetsAdded);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	TArray<int32> Contexts;
	TArray<FPopulateAssetDataRowArgs> PopulateRowArgs;
	PopulateRowArgs.AddDefaulted(InAssetsAdded.Num());

	UE::AssetRegistry::FFiltering::InitializeShouldSkipAsset();

	ParallelForWithTaskContext(TEXT("Populating TEDS Asset Registry Asset Data"), Contexts, InAssetsAdded.Num(), ParallelForMinBatchSize, [&PopulateRowArgs, &InAssetsAdded, this](int32& ValidAssetCount, int32 Index)
		{
			FPopulateAssetDataRowArgs RowArgs;
			const FAssetData& AssetData = InAssetsAdded[Index];

			if (!UE::AssetRegistry::FFiltering::ShouldSkipAsset(AssetData.AssetClassPath, AssetData.PackageFlags))
			{
				RowArgs = ThreadSafe_PopulateAssetDataTableRow(AssetData, Database);
				if (RowArgs.AssetData.IsValid())
				{
					++ValidAssetCount;
				}
			}
			PopulateRowArgs[Index] = MoveTemp(RowArgs);
		});


	int32 NewRowsCount = 0;
	for (int32 Context : Contexts)
	{
		NewRowsCount += Context;
	}

	int32 Index = 0;
	if (NewRowsCount > 0)
	{
		TArray<TPair<FMapKey, RowHandle>> KeyToRow;
		KeyToRow.Reserve(NewRowsCount);
		Database.BatchAddRow(AssetsDataTable, NewRowsCount, [RowArgs = MoveTemp(PopulateRowArgs), Index = 0, &KeyToRow, this](RowHandle InRowHandle) mutable
			{
				FPopulateAssetDataRowArgs ARowArgs = MoveTemp(RowArgs[Index]);
				while (!ARowArgs.AssetData.IsValid())
				{
					++Index;
					ARowArgs = MoveTemp(RowArgs[Index]);
				}

				KeyToRow.Emplace(MoveTemp(ARowArgs.ObjectPathKey), InRowHandle);
				PopulateAssetDataTableRow(MoveTemp(ARowArgs), Database, InRowHandle);
				++Index;
			});

		Database.BatchMapRows(KeyToRow);
	}
}

void FTedsAssetData::OnAssetsRemoved(TConstArrayView<FAssetData> InAssetsRemoved)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnAssetsRemoved);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	TArray<RowHandle> RowsToRemove;
	RowsToRemove.Reserve(InAssetsRemoved.Num());

	for (const FAssetData& Asset : InAssetsRemoved)
	{
		const FMapKey AssetKey = FMapKey(Asset.GetSoftObjectPath());
		const RowHandle AssetRow = Database.LookupMappedRow(AssetKey);
		if (Database.IsRowAssigned(AssetRow))
		{
			RowsToRemove.Add(AssetRow);
		}
	}

	Database.BatchRemoveRows(RowsToRemove);
}

void FTedsAssetData::OnAssetsUpdated(TConstArrayView<FAssetData> InAssetsUpdated)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnAssetsUpdated);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	for (const FAssetData& Asset : InAssetsUpdated)
	{
		const FMapKey AssetKey = FMapKey(Asset.GetSoftObjectPath());
		const RowHandle Row = Database.LookupMappedRow(AssetKey);
		if (Database.IsRowAssigned(Row))
		{
			Database.GetColumn<FAssetDataColumn_Experimental>(Row)->AssetData = Asset;
			Database.AddColumn<FUpdatedAssetDataTag>(Row);
		}
	}
}

void FTedsAssetData::OnAssetsUpdatedOnDisk(TConstArrayView<FAssetData> InAssetsUpdated)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnAssetsUpdatedOnDisk);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	for (const FAssetData& Asset : InAssetsUpdated)
	{
		const FMapKey AssetKey = FMapKey(Asset.GetSoftObjectPath());
		const RowHandle Row = Database.LookupMappedRow(AssetKey);
		if (Database.IsRowAssigned(Row))
		{
			Database.GetColumn<FAssetDataColumn_Experimental>(Row)->AssetData = Asset;
			Database.AddColumn<FUpdatedAssetDataTag>(Row);
		}
	}
}

void FTedsAssetData::OnAssetRenamed(const FAssetData& InAsset, const FString& InOldObjectPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnAssetRenamed);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	using namespace UE::Editor::DataStorage;

	FMapKey NewAssetHash = FMapKey(InAsset.GetSoftObjectPath());
	const FMapKey OldAssetHash = FMapKey(FSoftObjectPath(InOldObjectPath));
	const RowHandle Row = Database.LookupMappedRow(OldAssetHash);
	if (Database.IsRowAssigned(Row))
	{
		Database.GetColumn<FAssetDataColumn_Experimental>(Row)->AssetData = InAsset;

		// Update the asset in folder columns
		const FMapKeyView NewFolderHash(InAsset.PackagePath);
		FStringView OldPackagePath(InOldObjectPath);

		{
			int32 CharacterIndex = 0;
			OldPackagePath.FindLastChar(TEXT('/'), CharacterIndex);
			OldPackagePath.LeftInline(CharacterIndex);
		}

		const FMapKey OldFolderHash = FMapKey(FName(OldPackagePath));

		Database.AddColumn<FUpdatedPathTag>(Row);
		Database.RemapRow(OldAssetHash, MoveTemp(NewAssetHash));
	}
}

void FTedsAssetData::OnPathsAdded(TConstArrayView<FStringView> InPathsAdded)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnPathsAdded);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"));
#endif

	TArray<int32> PreparePathRowContexts;
	TArray<FPopulatePathRowArgs> PopulateRowArgs;
	PopulateRowArgs.AddDefaulted(InPathsAdded.Num());

	ParallelForWithTaskContext(TEXT("Populating TEDS Asset Registry Path"), PreparePathRowContexts, InPathsAdded.Num(), ParallelForMinBatchSize, [&PopulateRowArgs, &InPathsAdded, this](int32& WorkerValidCount, int32 Index)
		{
			FStringView Path = InPathsAdded[Index];
			FName PathName(Path);
			FMapKey AssetRegistryPathKey = FMapKey(PathName);
			FPopulatePathRowArgs RowArgs;

			if (Database.LookupMappedRow(AssetRegistryPathKey) != InvalidRowHandle)
			{
				RowArgs.MarkAsInvalid();
			}
			else
			{
				RowArgs = ThreadSafe_PopulatePathRowArgs(MoveTemp(AssetRegistryPathKey), PathName, Path);
				++WorkerValidCount;
			}

			PopulateRowArgs[Index] = MoveTemp(RowArgs);
		});

	int32 NewRowsCount = 0;
	for (const int32& Context : PreparePathRowContexts)
	{
		NewRowsCount += Context;
	}


	if (NewRowsCount > 0)
	{
		TArray<RowHandle> ReservedRow;
		ReservedRow.AddUninitialized(NewRowsCount);
		Database.BatchReserveRows(ReservedRow);

		TArray<TPair<FMapKey, RowHandle>> KeysAndRows;
		KeysAndRows.Reserve(NewRowsCount);

		int32 RowCount = 0;
		for (int32 Index = 0; Index < NewRowsCount; ++Index)
		{
			const FPopulatePathRowArgs* RowArgs = &PopulateRowArgs[RowCount];
			while (!RowArgs)
			{
				++RowCount;
				RowArgs = &PopulateRowArgs[RowCount];
			}

			KeysAndRows.Emplace(RowArgs->AssetRegistryPathKey, ReservedRow[Index]);
			++RowCount;
		}

		Database.BatchMapRows(KeysAndRows);

		int32 Index = 0;
		Database.BatchAddRow(PathsTable, ReservedRow, [&PopulateRowArgs, &Index, this](RowHandle InRowHandle)
			{
				FPopulatePathRowArgs RowArgs = MoveTemp(PopulateRowArgs[Index]);
				while (!RowArgs)
				{
					++Index;
					RowArgs = MoveTemp(PopulateRowArgs[Index]);
				}

				PopulatePathDataTableRow(MoveTemp(RowArgs), Database, InRowHandle);
				++Index;
			});
	}
}
 
void FTedsAssetData::OnPathsRemoved(TConstArrayView<FStringView> InPathsRemoved)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnPathsRemoved);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	for (const FStringView Path : InPathsRemoved)
	{
		FMapKey PathKey = FMapKey(FName(Path));
		Database.RemoveRow(Database.LookupMappedRow(PathKey));
	}
}
} // namespace UE::Editor::AssetData::Private

#undef TRACK_TEDSASSETDATA_MEMORY
