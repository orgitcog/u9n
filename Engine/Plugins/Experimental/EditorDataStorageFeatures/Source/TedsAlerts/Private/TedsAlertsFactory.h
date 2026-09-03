// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "TedsAlertColumns.h"
#include "UObject/ObjectMacros.h"

#include "TedsAlertsFactory.generated.h"

/**
 * Factory that manages tables, queries and any other data for alerts.
 */
UCLASS()
class UTedsAlertsFactory final : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	static const FName AlertChainTableName;
	static const FName UnsortedAlertChainTableName;

	~UTedsAlertsFactory() override = default;
	
	void RegisterTables(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;
	void RegisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;

	UE::Editor::DataStorage::TableHandle GetAlertChainTable() const;
	UE::Editor::DataStorage::TableHandle GetUnsortedAlertChainTable() const;

	UE::Editor::DataStorage::QueryHandle GetSortedAlertsQuery() const;
	UE::Editor::DataStorage::QueryHandle GetUnsortedAlertsQuery() const;

private:
	static const FName AlertConditionName;

	UE::Editor::DataStorage::TableHandle ChainTable = UE::Editor::DataStorage::InvalidTableHandle;
	UE::Editor::DataStorage::TableHandle UnsortedChainTable = UE::Editor::DataStorage::InvalidTableHandle;
	
	UE::Editor::DataStorage::QueryHandle SortedAlertsQuery = UE::Editor::DataStorage::InvalidQueryHandle;
	UE::Editor::DataStorage::QueryHandle UnsortedAlertsQuery = UE::Editor::DataStorage::InvalidQueryHandle;
	UE::Editor::DataStorage::QueryHandle AlertActionQuery = UE::Editor::DataStorage::InvalidQueryHandle;
	UE::Editor::DataStorage::QueryHandle ChildAlertColumnReadWriteQuery = UE::Editor::DataStorage::InvalidTableHandle;
	UE::Editor::DataStorage::QueryHandle ParentReadOnlyQuery = UE::Editor::DataStorage::InvalidTableHandle;

	void RegisterSubQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage);
	void RegisterSortUnsortedAlertsQuery(UE::Editor::DataStorage::ICoreProvider& DataStorage);
	void RegisterOnAddQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage);
	void RegisterOnRemoveQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage);
	void RegisterParentUpdatesQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage);
	void RegisterChildAlertUpdatesQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage);

	static void AssignAlert(
		UE::Editor::DataStorage::ISubqueryContext& TargetContext, UE::Editor::DataStorage::RowHandle TargetRow, FTedsAlertColumn& Target,
		UE::Editor::DataStorage::IQueryContext& SourceContext, UE::Editor::DataStorage::RowHandle SourceRow, FTedsAlertColumn& Source,
		int32 AlertActionQueryIndex);
	static void SwapAlerts(
		UE::Editor::DataStorage::ISubqueryContext& OriginalContext, UE::Editor::DataStorage::RowHandle OriginalRow, FTedsAlertColumn& Original,
		UE::Editor::DataStorage::IQueryContext& NewContext, UE::Editor::DataStorage::RowHandle NewRow, FTedsAlertColumn& New,
		int32 AlertActionQueryIndex);
	static void AppendAlert(
		FTedsAlertColumn& LastAlert, 
		UE::Editor::DataStorage::IQueryContext& AdditionalAlertContext, 
		UE::Editor::DataStorage::RowHandle AdditionalAlertRow, 
		FTedsAlertColumn& AdditionalAlert);
	static FTedsAlertActionColumn* GetAction(UE::Editor::DataStorage::IQueryContext& Context,
		UE::Editor::DataStorage::RowHandle Row, int32 ChildAlertQueryIndex);

	static void AddChildAlertsToHierarchy(
		UE::Editor::DataStorage::IQueryContext& Context, UE::Editor::DataStorage::RowHandle Parent, int32 ParentQueryIndex);
	static void IncrementParents(
		UE::Editor::DataStorage::IQueryContext& Context, UE::Editor::DataStorage::RowHandle Row, FTedsAlertColumnType AlertType,
		int32 ChildAlertQueryIndex);
	static bool MoveToNextParent(
		UE::Editor::DataStorage::RowHandle& Parent, UE::Editor::DataStorage::IQueryContext& Context, int32 SubQueryIndex);
	static void ResetChildAlertCounters(FTedsChildAlertColumn& ChildAlert);
};
