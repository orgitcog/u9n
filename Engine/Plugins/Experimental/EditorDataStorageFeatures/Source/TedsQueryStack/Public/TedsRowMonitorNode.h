// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Elements/Framework/TypedElementRowHandleArray.h"
#include "TedsQueryStackInterfaces.h"
#include "UObject/ObjectPtr.h"

namespace UE::Editor::DataStorage
{
	class ICoreProvider;
} // namespace UE::Editor::DataStorage

namespace UE::Editor::DataStorage::QueryStack
{
	/**
	 * Monitors tables for the addition and removal of one or more column types and updates the internal status if a change is detected.
	 */
	class FRowMonitorNode : public IRowNode
	{
	public:
		TEDSQUERYSTACK_API FRowMonitorNode(
			ICoreProvider& Storage, TSharedPtr<IRowNode> ParentRow, TSharedPtr<IQueryNode> QueryNode);
		TEDSQUERYSTACK_API FRowMonitorNode(
			ICoreProvider& Storage, TSharedPtr<IRowNode> ParentRow, TArray<TObjectPtr<const UScriptStruct>> Columns);
		TEDSQUERYSTACK_API FRowMonitorNode(
			ICoreProvider& Storage, TSharedPtr<IQueryNode> QueryNode, TSharedPtr<IRowNode> ParentRow, 
			TArray<TObjectPtr<const UScriptStruct>> MonitoredColumns);
		TEDSQUERYSTACK_API FRowMonitorNode(
			ICoreProvider& Storage, TSharedPtr<IQueryNode> QueryNode);
		TEDSQUERYSTACK_API FRowMonitorNode(
			ICoreProvider& Storage, TArray<TObjectPtr<const UScriptStruct>> Columns);
		TEDSQUERYSTACK_API FRowMonitorNode(
			ICoreProvider& Storage, TSharedPtr<IQueryNode> QueryNode, TArray<TObjectPtr<const UScriptStruct>> MonitoredColumns);

		TEDSQUERYSTACK_API virtual ~FRowMonitorNode() override;

		TEDSQUERYSTACK_API virtual RevisionId GetRevision() const override;
		TEDSQUERYSTACK_API virtual void Update() override;
		TEDSQUERYSTACK_API virtual FRowHandleArrayView GetRows() const override;
		TEDSQUERYSTACK_API virtual FRowHandleArray* GetMutableRows() override;

	private:
		const FRowHandleArray& ResolveRows() const;
		FRowHandleArray& ResolveRows();
		void UpdateColumnsFromQuery();
		void UpdateRows();
		void UpdateMonitoredColumns();

		FRowHandleArray Rows;
		FRowHandleArray AddedRows;
		FRowHandleArray RemovedRows;

		TArray<QueryHandle> Observers;
		TArray<TObjectPtr<const UScriptStruct>> MonitoredColumns;
		
		TSharedPtr<IQueryNode> QueryNode;
		TSharedPtr<IRowNode> ParentRow;
		ICoreProvider& Storage;
		RevisionId QueryRevision = 0;
		RevisionId ParentRevision = 0;
		RevisionId Revision = 0;
		bool bFixedColumns = false;
	};
} // namespace UE::Editor::DataStorage::QueryStack
