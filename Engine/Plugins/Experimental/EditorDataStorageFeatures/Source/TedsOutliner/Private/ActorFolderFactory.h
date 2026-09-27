// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Containers/Ticker.h"

#include "ActorFolderFactory.generated.h"

namespace UE::Editor::DataStorage
{
	struct IQueryContext;
}

class IEditorDataStorageProvider;
class UWorld;
struct FFolder;
class UActorFolder;

UCLASS()
class UTedsActorFolderFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTedsActorFolderFactory() override = default;

	virtual void RegisterTables(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;
	virtual void PreRegister(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;
	virtual void RegisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;
	virtual void PreShutdown(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;

private:

	void OnFolderCreated(UWorld& World, const FFolder& Folder);
	void OnFolderDeleted(UWorld& World, const FFolder& Folder);
	void OnFolderMoved(UWorld& InWorld, const FFolder& InOldFolder, const FFolder& InNewFolder);

	void OnActorFolderAdded(UActorFolder* InActorFolder);
	void OnActorFolderRemoved(UActorFolder* InActorFolder);

	void OnMapChange(uint32 MapChangeFlags);
	void OnLevelAdded(ULevel* InLevel, UWorld* InWorld);

	void OnPIEStarted(bool bIsSimulating);

	// Register a folder in TEDS (If not already registered) and return the row handle
	UE::Editor::DataStorage::RowHandle RegisterFolder(UWorld& World, const FFolder& Folder);
	void SetFolderColumns(UE::Editor::DataStorage::RowHandle Row, UWorld& World, const FFolder& Folder);


	// Unregister a folder in TEDS
	void UnregisterFolder(const FFolder& Folder);

	void Tick();
	
private:
	
	UE::Editor::DataStorage::ICoreProvider* DataStorage;

	// List of folders to process rename in the next tick
	TArray<UE::Editor::DataStorage::RowHandle> FoldersToRename;
};
