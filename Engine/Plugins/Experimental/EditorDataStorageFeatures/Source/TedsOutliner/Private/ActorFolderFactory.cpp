// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorFolderFactory.h"

#include "Columns/ActorFolderColumns.h"
#include "Editor.h"
#include "EditorActorFolders.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementFolderColumns.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "SceneOutlinerHelpers.h"

namespace UE::TedsActorFolderFactory::Local
{
	static const FName TableName("Editor_ActorFolderTable");

	static bool bRegisterFoldersinTEDS = false;
	
	static FAutoConsoleVariableRef CVarUseTEDSOutliner(
		TEXT("TEDS.Feature.ActorFolders"),
		bRegisterFoldersinTEDS,
		TEXT("Populate FFolders and Actor Folders in TEDS. Must be set at startup."));
}

void UTedsActorFolderFactory::RegisterTables(UE::Editor::DataStorage::ICoreProvider& InDataStorage)
{
	InDataStorage.RegisterTable(
		UE::Editor::DataStorage::TTypedElementColumnTypeList<
		FFolderTag, FTypedElementLabelColumn, FTypedElementWorldColumn, FSlateColorColumn>(),
	UE::TedsActorFolderFactory::Local::TableName);
}

void UTedsActorFolderFactory::PreRegister(UE::Editor::DataStorage::ICoreProvider& InDataStorage)
{
	Super::PreRegister(InDataStorage);

	if (UE::TedsActorFolderFactory::Local::bRegisterFoldersinTEDS)
	{
		FActorFolders::Get().OnFolderCreated.AddUObject(this, &UTedsActorFolderFactory::OnFolderCreated);
		FActorFolders::Get().OnFolderDeleted.AddUObject(this, &UTedsActorFolderFactory::OnFolderDeleted);
		FActorFolders::Get().OnFolderMoved.AddUObject(this, &UTedsActorFolderFactory::OnFolderMoved);

		FEditorDelegates::MapChange.AddUObject(this, &UTedsActorFolderFactory::OnMapChange);
		FEditorDelegates::PostPIEStarted.AddUObject(this, &UTedsActorFolderFactory::OnPIEStarted);
	
		FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UTedsActorFolderFactory::OnLevelAdded);

		GEngine->OnActorFolderAdded().AddUObject(this, &UTedsActorFolderFactory::OnActorFolderAdded);
		GEngine->OnActorFolderRemoved().AddUObject(this, &UTedsActorFolderFactory::OnActorFolderRemoved);
	
		InDataStorage.OnUpdateCompleted().AddUObject(this, &UTedsActorFolderFactory::Tick);
	}
	
	// Store a pointer to data storage so we don't have to grab it from the global instance of the registry every time
	DataStorage = &InDataStorage;
}

void UTedsActorFolderFactory::RegisterQueries(UE::Editor::DataStorage::ICoreProvider& InDataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;
	using namespace UE::Editor;

	if (!UE::TedsActorFolderFactory::Local::bRegisterFoldersinTEDS)
	{
		return;
	}

	InDataStorage.RegisterQuery(
		Select(
			TEXT("Delete folder with deleted world"),
			FProcessor(EQueryTickPhase::PostPhysics, InDataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage))
				.SetExecutionMode(EExecutionMode::GameThread),
			[](IQueryContext& Context, RowHandle Row, const FTypedElementWorldColumn& World)
			{
				if (!World.World.IsValid(false, true))
				{
					Context.RemoveRow(Row);
				}
			}
		)
		.Where()
			.All<FFolderTag>()
		.Compile()
		);

	// We can't currently detect changes to folders since they aren't UObjects so we run this query for all folders every frame.
	// If this becomes an issue we can add an event on folder expansion changed that the factory can subscribe to instead (or add the sync tag manually)
	InDataStorage.RegisterQuery(
		Select(
			TEXT("Set Folder expansion state"),
			FProcessor(EQueryTickPhase::PostPhysics, InDataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage))
				.SetExecutionMode(EExecutionMode::GameThread),
			[](IQueryContext& Context, DataStorage::RowHandle Row,
				const FFolderCompatibilityColumn& FolderCompatibilityColumn, const FTypedElementWorldColumn& WorldColumn)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UTedsActorFolderFactory::SetFolderExpansionState);
				if (UWorld* World = WorldColumn.World.Get())
				{
					// PIE folders are always treated as expanded
					const bool bIsExpanded = !World->IsGameWorld() ?
						FActorFolders::Get().IsFolderExpanded(*World, FolderCompatibilityColumn.Folder) : true;
					
					if (bIsExpanded)
					{
						Context.AddColumns<FFolderExpandedTag>(Row);
					}
					else
					{
						Context.RemoveColumns<FFolderExpandedTag>(Row);
					}
				}
			}
		)
		.Where()
			.All<FFolderTag>()
		.Compile()
		);

	InDataStorage.RegisterQuery(
		Select(
			TEXT("Sync label column to folder"),
			FProcessor(EQueryTickPhase::FrameEnd, InDataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncDataStorageToExternal))
				.SetExecutionMode(EExecutionMode::GameThread),
			[this](RowHandle RowHandle)
			{
				// We have to defer the rename because it deletes and re-creates the folder which re-registers them in TEDS which cannot be done while
				// we are in this query callback
				FoldersToRename.Add(RowHandle);
			}
		)
		.Where()
			.All<FFolderTag, FFolderCompatibilityColumn, FTypedElementWorldColumn, FTypedElementSyncBackToWorldTag>()
		.Compile()
	);
}

void UTedsActorFolderFactory::PreShutdown(UE::Editor::DataStorage::ICoreProvider& InDataStorage)
{
	if (UE::TedsActorFolderFactory::Local::bRegisterFoldersinTEDS)
	{
		FActorFolders::Get().OnFolderCreated.RemoveAll(this);
		FActorFolders::Get().OnFolderDeleted.RemoveAll(this);
		FActorFolders::Get().OnFolderMoved.RemoveAll(this);

		FEditorDelegates::MapChange.RemoveAll(this);
		FEditorDelegates::PostPIEStarted.RemoveAll(this);
	
		FWorldDelegates::LevelAddedToWorld.RemoveAll(this);

		GEngine->OnActorFolderAdded().RemoveAll(this);
		GEngine->OnActorFolderRemoved().RemoveAll(this);
	
		InDataStorage.OnUpdateCompleted().RemoveAll(this);
	}
	

	Super::PreShutdown(InDataStorage);
}

void UTedsActorFolderFactory::OnFolderCreated(UWorld& World, const FFolder& Folder)
{
	RegisterFolder(World, Folder);
}

void UTedsActorFolderFactory::OnFolderDeleted(UWorld&, const FFolder& Folder)
{
	UnregisterFolder(Folder);
}

void UTedsActorFolderFactory::OnMapChange(uint32 MapChangeFlags)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UTedsActorFolderFactory::OnMapChange);
	
	// Iterate through editor world folders and add them to TEDS
	if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
	{
		FActorFolders::Get().ForEachFolder(*World, [this, World](const FFolder& Folder)
		{
			OnFolderCreated(*World, Folder);
			return true;
		});
	}
}

void UTedsActorFolderFactory::OnPIEStarted(bool bIsSimulating)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UTedsActorFolderFactory::OnPIEStarted);
	
	// Iterate through PIE world actors and add their folders to TEDS
	if (FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
	{
		if (UWorld* PieWorld = PIEWorldContext->World())
		{
			for (FActorIterator ActorIt(PieWorld); ActorIt; ++ActorIt)
			{
				if(AActor* Actor = *ActorIt)
				{
					OnFolderCreated(*PieWorld, Actor->GetFolder());
				}
			}
		}
	}
}

void UTedsActorFolderFactory::OnActorFolderAdded(UActorFolder* InActorFolder)
{
	if(ULevel* Level = InActorFolder->GetOuterULevel())
	{
		if(UWorld* World = Level->GetWorld())
		{
			RegisterFolder(*World, InActorFolder->GetFolder());
		}
	}
	
}

void UTedsActorFolderFactory::OnActorFolderRemoved(UActorFolder* InActorFolder)
{
	UnregisterFolder(InActorFolder->GetFolder());
}

void UTedsActorFolderFactory::OnLevelAdded(ULevel* InLevel, UWorld* InWorld)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UTedsActorFolderFactory::OnLevelAdded);

	if(InLevel && InWorld)
	{
		// Iterate through all actors in the level and add any folders containing them
		for (AActor* Actor : InLevel->Actors)
		{
			if (Actor)
			{
				// Parent Folder
				FFolder Folder = Actor->GetFolder();

				if(!Folder.IsNone())
				{
					RegisterFolder(*InWorld, Folder);
				}
			}
		}
	}
}

void UTedsActorFolderFactory::OnFolderMoved(UWorld& InWorld, const FFolder& InOldFolder, const FFolder& InNewFolder)
{
	UE::Editor::DataStorage::FMapKey OldIndex(static_cast<uint64>(GetTypeHash(InOldFolder)));
	UE::Editor::DataStorage::RowHandle Row = DataStorage->LookupMappedRow(OldIndex);

	// Remove the mapping to the old folder
	DataStorage->RemoveRowMapping(OldIndex);
	
	// Map this row to the new folder information
	UE::Editor::DataStorage::FMapKey NewIndex(static_cast<uint64>(GetTypeHash(InNewFolder)));
	DataStorage->MapRow(NewIndex, Row);

	// Update the default columns in TEDS as the data could have changed after the folder moved
	SetFolderColumns(Row, InWorld, InNewFolder);
}

// Register the folder and fill in the columns with data (World, FFolder, Parent, ActorFolder etc)
UE::Editor::DataStorage::RowHandle UTedsActorFolderFactory::RegisterFolder(UWorld& World, const FFolder& Folder)
{
	// Don't register invalid folders
	if(Folder.IsNone())
	{
		return UE::Editor::DataStorage::InvalidRowHandle;
	}

	UE::Editor::DataStorage::FMapKey Index(static_cast<uint64>(GetTypeHash(Folder)));
	UE::Editor::DataStorage::RowHandle Row = DataStorage->LookupMappedRow(Index);
	
	if (!DataStorage->IsRowAvailable(Row))
	{
		// Add and index the row
		static UE::Editor::DataStorage::RowHandle Table = DataStorage->FindTable(UE::TedsActorFolderFactory::Local::TableName);

		if(Table == UE::Editor::DataStorage::InvalidTableHandle)
		{
			return UE::Editor::DataStorage::InvalidRowHandle;
		}
	
		Row = DataStorage->AddRow(Table);
		DataStorage->MapRow(Index, Row);

		SetFolderColumns(Row, World,  Folder);
	}
	
	return Row;
}

void UTedsActorFolderFactory::SetFolderColumns(UE::Editor::DataStorage::RowHandle Row, UWorld& World, const FFolder& Folder)
{
	DataStorage->GetColumn<FTypedElementWorldColumn>(Row)->World = TWeakObjectPtr<UWorld>(&World);
	DataStorage->GetColumn<FTypedElementLabelColumn>(Row)->Label = Folder.GetLeafName().ToString();
	DataStorage->AddColumn(Row, FFolderCompatibilityColumn{.Folder = Folder});

	if(UActorFolder* ActorFolder = Folder.GetActorFolder())
	{
		DataStorage->AddColumn(Row, FTypedElementUObjectColumn{.Object = ActorFolder});
	}
	else
	{
		DataStorage->RemoveColumn<FTypedElementUObjectColumn>(Row);
	}
		
	// Find or register the parent folder
	UE::Editor::DataStorage::RowHandle ParentRow = RegisterFolder(World, Folder.GetParent());

	if(DataStorage->IsRowAvailable(ParentRow))
	{
		DataStorage->AddColumn(Row, FTableRowParentColumn{.Parent = ParentRow});
	}
	else
	{
		DataStorage->RemoveColumn<FTableRowParentColumn>(Row);
	}
}

void UTedsActorFolderFactory::UnregisterFolder(const FFolder& Folder)
{
	UE::Editor::DataStorage::FMapKey Index(static_cast<uint64>(GetTypeHash(Folder)));
	UE::Editor::DataStorage::RowHandle Row = DataStorage->LookupMappedRow(Index);

	if (DataStorage->IsRowAvailable(Row))
	{
		DataStorage->RemoveRow(Row);
	}
}

void UTedsActorFolderFactory::Tick()
{
	for(UE::Editor::DataStorage::RowHandle RowHandle : FoldersToRename)
	{
		FFolderCompatibilityColumn* FolderCompatibilityColumn = DataStorage->GetColumn<FFolderCompatibilityColumn>(RowHandle);
		FTypedElementWorldColumn* WorldColumn = DataStorage->GetColumn<FTypedElementWorldColumn>(RowHandle);
		FTypedElementLabelColumn* LabelColumn = DataStorage->GetColumn<FTypedElementLabelColumn>(RowHandle);

		if(FolderCompatibilityColumn && WorldColumn && LabelColumn)
		{
			SceneOutliner::FSceneOutlinerHelpers::RenameFolder(FolderCompatibilityColumn->Folder,
				FText::FromString(LabelColumn->Label), WorldColumn->World.Get());
		}
	}
	
	FoldersToRename.Empty();
}
