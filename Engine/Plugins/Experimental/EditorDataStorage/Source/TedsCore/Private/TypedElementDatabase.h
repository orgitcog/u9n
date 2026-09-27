// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "MassArchetypeTypes.h"
#include "Misc/TVariant.h"
#include "Templates/SharedPointer.h"
#include "TypedElementDatabaseCommandBuffer.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementDatabase.generated.h"

struct FMassEntityManager;
struct FMassProcessingPhaseManager;
class UEditorDataStorageFactory;
class FOutputDevice;
class UWorld;

namespace UE::Editor::DataStorage
{
	class FEnvironment;
} // namespace UE::Editor::DataStorage

UCLASS()
class TEDSCORE_API UEditorDataStorage final
	: public UObject
	, public UE::Editor::DataStorage::ICoreProvider
{
	GENERATED_BODY()

public:
	template<typename FactoryType, typename DatabaseType>
	class TFactoryIterator
	{
	public:
		using ThisType = TFactoryIterator<FactoryType, DatabaseType>;
		using FactoryPtr = FactoryType*;
		using DatabasePtr = DatabaseType*;

		TFactoryIterator() = default;
		explicit TFactoryIterator(DatabasePtr InDatabase);

		FactoryPtr operator*() const;
		ThisType& operator++();
		operator bool() const;

	private:
		DatabasePtr Database = nullptr;
		int32 Index = 0;
	};

	using FactoryIterator = TFactoryIterator<UEditorDataStorageFactory, UEditorDataStorage>;
	using FactoryConstIterator = TFactoryIterator<const UEditorDataStorageFactory, const UEditorDataStorage>;

public:
	~UEditorDataStorage() override = default;
	
	void Initialize();
	
	void SetFactories(TConstArrayView<UClass*> InFactories);
	void ResetFactories();

	/** An iterator which allows traversal of factory instances. Ordered lowest->highest of GetOrder() */
	FactoryIterator CreateFactoryIterator();
	/** An iterator which allows traversal of factory instances. Ordered lowest->highest of GetOrder() */
	FactoryConstIterator CreateFactoryIterator() const;

	/** Returns factory instance given the type of factory */
	virtual const UEditorDataStorageFactory* FindFactory(const UClass* FactoryType) const override;
	/** Helper for FindFactory(const UClass*) */
	template<typename FactoryTypeT>
	const FactoryTypeT* FindFactory() const;
	
	void Deinitialize();

	/** Triggered at the start of the underlying Mass' tick cycle. */
	void OnPreMassTick(float DeltaTime);
	/** Triggered just before underlying Mass processing completes it's tick cycle. */
	void OnPostMassTick(float DeltaTime);

	TSharedPtr<FMassEntityManager> GetActiveMutableEditorEntityManager();
	TSharedPtr<const FMassEntityManager> GetActiveEditorEntityManager() const;

	virtual UE::Editor::DataStorage::TableHandle RegisterTable(TConstArrayView<const UScriptStruct*> ColumnList, const FName& Name) override;
	virtual UE::Editor::DataStorage::TableHandle RegisterTable(
		UE::Editor::DataStorage::TableHandle SourceTable, TConstArrayView<const UScriptStruct*> ColumnList, const FName& Name) override;
	virtual UE::Editor::DataStorage::TableHandle FindTable(const FName& Name) override;

	virtual UE::Editor::DataStorage::RowHandle ReserveRow() override;
	virtual void BatchReserveRows(int32 Count, TFunctionRef<void(UE::Editor::DataStorage::RowHandle)> ReservationCallback) override;
	virtual void BatchReserveRows(TArrayView<UE::Editor::DataStorage::RowHandle> ReservedRows) override;
	virtual UE::Editor::DataStorage::RowHandle AddRow(UE::Editor::DataStorage::TableHandle Table, 
		UE::Editor::DataStorage::RowCreationCallbackRef OnCreated) override;
	UE::Editor::DataStorage::RowHandle AddRow(UE::Editor::DataStorage::TableHandle Table) override;
	virtual bool AddRow(UE::Editor::DataStorage::RowHandle ReservedRow, UE::Editor::DataStorage::TableHandle Table) override;
	virtual bool AddRow(UE::Editor::DataStorage::RowHandle ReservedRow, UE::Editor::DataStorage::TableHandle Table,
		UE::Editor::DataStorage::RowCreationCallbackRef OnCreated) override;
	virtual bool BatchAddRow(UE::Editor::DataStorage::TableHandle Table, int32 Count,
		UE::Editor::DataStorage::RowCreationCallbackRef OnCreated) override;
	virtual bool BatchAddRow(UE::Editor::DataStorage::TableHandle Table, TConstArrayView<UE::Editor::DataStorage::RowHandle> ReservedHandles,
		UE::Editor::DataStorage::RowCreationCallbackRef OnCreated) override;
	virtual void RemoveRow(UE::Editor::DataStorage::RowHandle Row) override;
	virtual void BatchRemoveRows(TConstArrayView<UE::Editor::DataStorage::RowHandle> Rows) override;
	virtual void RemoveAllRowsWithColumns(TConstArrayView<const UScriptStruct*> Columns) override;
	virtual bool IsRowAvailable(UE::Editor::DataStorage::RowHandle Row) const override;
	virtual bool IsRowAssigned(UE::Editor::DataStorage::RowHandle Row) const override;
	/** Same as IsRowAvailable, but doesn't check if the data storage has been initialized. */
	bool IsRowAvailableUnsafe(UE::Editor::DataStorage::RowHandle Row) const;
	/** Same as IsRowAssigned, but doesn't check if the data storage has been initialized. */
	bool IsRowAssignedUnsafe(UE::Editor::DataStorage::RowHandle Row) const;

	virtual void AddColumn(UE::Editor::DataStorage::RowHandle Row, const UScriptStruct* ColumnType) override;
	virtual void AddColumn(UE::Editor::DataStorage::RowHandle Row, const UE::Editor::DataStorage::FValueTag& Tag, const FName& InValue) override;
	virtual void AddColumnData(UE::Editor::DataStorage::RowHandle Row, const UScriptStruct* ColumnType,
		const UE::Editor::DataStorage::ColumnCreationCallbackRef Initializer,
		UE::Editor::DataStorage::ColumnCopyOrMoveCallback Relocator) override;
	virtual void RemoveColumn(UE::Editor::DataStorage::RowHandle Row, const UScriptStruct* ColumnType) override;
	virtual void RemoveColumn(UE::Editor::DataStorage::RowHandle Row, const UE::Editor::DataStorage::FValueTag& Tag) override;
	virtual void* GetColumnData(UE::Editor::DataStorage::RowHandle Row, const UScriptStruct* ColumnType) override;
	virtual const void* GetColumnData(UE::Editor::DataStorage::RowHandle Row, const UScriptStruct* ColumnType) const override;
	virtual void AddColumns(UE::Editor::DataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> Columns) override;
	virtual void RemoveColumns(UE::Editor::DataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> Columns) override;
	virtual void AddRemoveColumns(UE::Editor::DataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnsToAdd,
		TConstArrayView<const UScriptStruct*> ColumnsToRemove) override;
	virtual void BatchAddRemoveColumns(TConstArrayView<UE::Editor::DataStorage::RowHandle> Rows,TConstArrayView<const UScriptStruct*> ColumnsToAdd,
		TConstArrayView<const UScriptStruct*> ColumnsToRemove) override;
	virtual bool HasColumns(UE::Editor::DataStorage::RowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) const override;
	virtual bool HasColumns(UE::Editor::DataStorage::RowHandle Row, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes) const override;
	virtual void ListColumns(UE::Editor::DataStorage::RowHandle Row, UE::Editor::DataStorage::ColumnListCallbackRef Callback) const override;
	virtual void ListColumns(UE::Editor::DataStorage::RowHandle Row, UE::Editor::DataStorage::ColumnListWithDataCallbackRef Callback) override;
	virtual bool MatchesColumns(UE::Editor::DataStorage::RowHandle Row, const UE::Editor::DataStorage::Queries::FConditions& Conditions) const override;

	virtual const UScriptStruct* FindDynamicColumn(const UE::Editor::DataStorage::FDynamicColumnDescription& Description) const override;
	virtual const UScriptStruct* GenerateDynamicColumn(const UE::Editor::DataStorage::FDynamicColumnDescription& Description) override;
	virtual void ForEachDynamicColumn(const UScriptStruct& Template, TFunctionRef<void(const UScriptStruct& Type)> Callback) const override;

	void RegisterTickGroup(FName GroupName, UE::Editor::DataStorage::EQueryTickPhase Phase, FName BeforeGroup, FName AfterGroup, UE::Editor::DataStorage::EExecutionMode ExecutionMode);
	void UnregisterTickGroup(FName GroupName, UE::Editor::DataStorage::EQueryTickPhase Phase);

	UE::Editor::DataStorage::QueryHandle RegisterQuery(UE::Editor::DataStorage::FQueryDescription&& Query) override;
	virtual void UnregisterQuery(UE::Editor::DataStorage::QueryHandle Query) override;
	virtual const UE::Editor::DataStorage::FQueryDescription& GetQueryDescription(UE::Editor::DataStorage::QueryHandle Query) const override;
	virtual FName GetQueryTickGroupName(UE::Editor::DataStorage::EQueryTickGroups Group) const override;
	virtual UE::Editor::DataStorage::FQueryResult RunQuery(UE::Editor::DataStorage::QueryHandle Query) override;
	virtual UE::Editor::DataStorage::FQueryResult RunQuery(UE::Editor::DataStorage::QueryHandle Query, UE::Editor::DataStorage::DirectQueryCallbackRef Callback) override;
	virtual UE::Editor::DataStorage::FQueryResult RunQuery(UE::Editor::DataStorage::QueryHandle Query, UE::Editor::DataStorage::EDirectQueryExecutionFlags Flags,
		UE::Editor::DataStorage::DirectQueryCallbackRef Callback) override;
	virtual void ActivateQueries(FName ActivationName) override;

	virtual UE::Editor::DataStorage::RowHandle LookupMappedRow(const UE::Editor::DataStorage::FMapKeyView& Key) const override;
	virtual void MapRow(UE::Editor::DataStorage::FMapKey Key, UE::Editor::DataStorage::RowHandle Row) override;
	virtual void BatchMapRows(TArrayView<TPair<UE::Editor::DataStorage::FMapKey, UE::Editor::DataStorage::RowHandle>> MapRowPairs) override;
	virtual void RemapRow(const UE::Editor::DataStorage::FMapKeyView& OriginalKey, UE::Editor::DataStorage::FMapKey NewKey) override;
	virtual void RemoveRowMapping(const UE::Editor::DataStorage::FMapKeyView& Key) override;

	virtual UE::Editor::DataStorage::FTypedElementOnDataStorageUpdate& OnUpdate() override;
	virtual UE::Editor::DataStorage::FTypedElementOnDataStorageUpdate& OnUpdateCompleted() override;
	virtual bool IsAvailable() const override;
	virtual void* GetExternalSystemAddress(UClass* Target) override;

	virtual bool SupportsExtension(FName Extension) const override;
	virtual void ListExtensions(TFunctionRef<void(FName)> Callback) const override;
	
	TSharedPtr<UE::Editor::DataStorage::FEnvironment> GetEnvironment();
	TSharedPtr<const UE::Editor::DataStorage::FEnvironment> GetEnvironment() const;

	FMassArchetypeHandle LookupArchetype(UE::Editor::DataStorage::TableHandle InTableHandle) const;

	void DebugPrintQueryCallbacks(FOutputDevice& Output) override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

private:
	void PreparePhase(UE::Editor::DataStorage::EQueryTickPhase Phase, float DeltaTime);
	void FinalizePhase(UE::Editor::DataStorage::EQueryTickPhase Phase, float DeltaTime);
	void Reset();

	int32 GetTableChunkSize(FName TableName) const;
	
	struct FFactoryTypePair
	{
		// Used to find the factory by type without needing to dereference each one
		TObjectPtr<UClass> Type;
		
		TObjectPtr<UEditorDataStorageFactory> Instance;
	};
	
	static const FName TickGroupName_Default;
	static const FName TickGroupName_PreUpdate;
	static const FName TickGroupName_Update;
	static const FName TickGroupName_PostUpdate;
	static const FName TickGroupName_SyncWidget;
	static const FName TickGroupName_SyncExternalToDataStorage;
	static const FName TickGroupName_SyncDataStorageToExternal;
	
	TArray<FMassArchetypeHandle> Tables;
	TMap<FName, UE::Editor::DataStorage::TableHandle> TableNameLookup;

	// Ordered array of factories by the return value of GetOrder()
	TArray<FFactoryTypePair> Factories;

	TSharedPtr<UE::Editor::DataStorage::FEnvironment> Environment;
	
	UE::Editor::DataStorage::FTypedElementOnDataStorageUpdate OnUpdateDelegate;
	UE::Editor::DataStorage::FTypedElementOnDataStorageUpdate OnUpdateCompletedDelegate;
	FDelegateHandle OnPreMassTickHandle;
	FDelegateHandle OnPostMassTickHandle;

	TSharedPtr<FMassEntityManager> ActiveEditorEntityManager;
	TSharedPtr<FMassProcessingPhaseManager> ActiveEditorPhaseManager;
};

template <typename FactoryType, typename DatabaseType>
UEditorDataStorage::TFactoryIterator<FactoryType, DatabaseType>::TFactoryIterator(DatabasePtr InDatabase): Database(InDatabase)
{}

template <typename FactoryType, typename DatabaseType>
typename UEditorDataStorage::TFactoryIterator<FactoryType, DatabaseType>::FactoryPtr UEditorDataStorage::TFactoryIterator<FactoryType, DatabaseType>::operator*() const
{
	return Database->Factories[Index].Instance;
}

template <typename FactoryType, typename DatabaseType>
typename UEditorDataStorage::TFactoryIterator<FactoryType, DatabaseType>::ThisType& UEditorDataStorage::TFactoryIterator<FactoryType, DatabaseType>::operator++()
{
	if (Database != nullptr && Index < Database->Factories.Num())
	{
		++Index;
	}
	return *this;
}

template <typename FactoryType, typename DatabaseType>
UEditorDataStorage::TFactoryIterator<FactoryType, DatabaseType>::operator bool() const
{
	return Database != nullptr && Index < Database->Factories.Num();
}

template <typename FactoryTypeT>
const FactoryTypeT* UEditorDataStorage::FindFactory() const
{
	return static_cast<const FactoryTypeT*>(FindFactory(FactoryTypeT::StaticClass()));
}
