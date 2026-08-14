// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsRowMonitorNode.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"

namespace UE::Editor::DataStorage::QueryStack
{
	FRowMonitorNode::FRowMonitorNode(
		ICoreProvider& Storage, TSharedPtr<IRowNode> ParentRow, TSharedPtr<IQueryNode> QueryNode)
		: QueryNode(MoveTemp(QueryNode))
		, ParentRow(MoveTemp(ParentRow))
		, Storage(Storage)
	{
		UpdateRows();
		UpdateColumnsFromQuery();
		UpdateMonitoredColumns();
	}

	FRowMonitorNode::FRowMonitorNode(
		ICoreProvider& Storage, TSharedPtr<IRowNode> ParentRow, TArray<TObjectPtr<const UScriptStruct>> Columns)
		: MonitoredColumns(MoveTemp(Columns))
		, ParentRow(MoveTemp(ParentRow))
		, Storage(Storage)
	{
		UpdateRows();
		UpdateMonitoredColumns();
	}

	FRowMonitorNode::FRowMonitorNode(
		ICoreProvider& Storage, TSharedPtr<IQueryNode> QueryNode, TSharedPtr<IRowNode> ParentRow, 
		TArray<TObjectPtr<const UScriptStruct>> MonitoredColumns)
		: MonitoredColumns(MoveTemp(MonitoredColumns))
		, QueryNode(MoveTemp(QueryNode))
		, ParentRow(MoveTemp(ParentRow))
		, Storage(Storage)
		, bFixedColumns(true)
	{
		UpdateRows();
		UpdateColumnsFromQuery();
		UpdateMonitoredColumns();
	}

	FRowMonitorNode::FRowMonitorNode(ICoreProvider& Storage, TSharedPtr<IQueryNode> QueryNode)
		: QueryNode(MoveTemp(QueryNode))
		, Storage(Storage)
	{
		UpdateColumnsFromQuery();
		UpdateMonitoredColumns();
	}
	
	FRowMonitorNode::FRowMonitorNode(ICoreProvider& Storage, TArray<TObjectPtr<const UScriptStruct>> Columns)
		: MonitoredColumns(MoveTemp(Columns))
		, Storage(Storage)
	{
		UpdateMonitoredColumns();
	}

	FRowMonitorNode::FRowMonitorNode(
		ICoreProvider& Storage, TSharedPtr<IQueryNode> QueryNode, TArray<TObjectPtr<const UScriptStruct>> MonitoredColumns)
		: MonitoredColumns(MoveTemp(MonitoredColumns))
		, QueryNode(MoveTemp(QueryNode))
		, Storage(Storage)
		, bFixedColumns(true)
	{
		UpdateColumnsFromQuery();
		UpdateMonitoredColumns();
	}

	FRowMonitorNode::~FRowMonitorNode()
	{
		for (QueryHandle Observer : Observers)
		{
			Storage.UnregisterQuery(Observer);
		}
	}

	INode::RevisionId FRowMonitorNode::GetRevision() const
	{
		return Revision;
	}

	void FRowMonitorNode::Update()
	{
		if (QueryNode)
		{
			QueryNode->Update();
			if (QueryNode->GetRevision() != QueryRevision)
			{
				UpdateColumnsFromQuery();
				UpdateMonitoredColumns();
				Revision++;
			}
		}
		
		bool MergeChanges = true;
		if (ParentRow)
		{
			ParentRow->Update();
			if (ParentRow->GetRevision() != ParentRevision)
			{
				UpdateRows();
				Revision++;
				MergeChanges = false;
			}
		}
		
		if (MergeChanges && (!AddedRows.IsEmpty() || !RemovedRows.IsEmpty()))
		{
			AddedRows.Sort();
			AddedRows.MakeUnique();

			RemovedRows.Sort();
			RemovedRows.MakeUnique();

			FRowHandleArray& TargetRows = ResolveRows();
			TargetRows.SortedMerge(AddedRows);
			TargetRows.Remove(RemovedRows.GetRows());

			Revision++;
		}
		AddedRows.Empty();
		RemovedRows.Empty();
	}

	FRowHandleArrayView FRowMonitorNode::GetRows() const
	{
		return ResolveRows().GetRows();
	}

	FRowHandleArray* FRowMonitorNode::GetMutableRows()
	{
		return &ResolveRows();
	}

	const FRowHandleArray& FRowMonitorNode::ResolveRows() const
	{
		return const_cast<FRowMonitorNode*>(this)->ResolveRows();
	}
	FRowHandleArray& FRowMonitorNode::ResolveRows()
	{
		FRowHandleArray* ParentRowArray = ParentRow ? ParentRow->GetMutableRows() : nullptr;
		return ParentRowArray == nullptr ? Rows : *ParentRowArray;
	}

	void FRowMonitorNode::UpdateColumnsFromQuery()
	{
		using namespace UE::Editor::DataStorage::Queries;

		if (!bFixedColumns)
		{
			if (QueryNode)
			{
				TSet<TObjectPtr<const UScriptStruct>> LocalColumns;
				
				const FQueryDescription& Query = Storage.GetQueryDescription(QueryNode->GetQuery());
				
				const FConditions* Conditions = Query.Conditions.GetPtrOrNull();
				TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ComplexConditionColumns;
				if (Conditions && !Conditions->IsEmpty())
				{
					ComplexConditionColumns = Conditions->GetColumns();
				}
				
				LocalColumns.Reserve(Query.SelectionTypes.Num() + Query.ConditionOperators.Num() + ComplexConditionColumns.Num());

				// Collect all columns that are selected for access.
				for (const TWeakObjectPtr<const UScriptStruct>& SelectionColumn : Query.SelectionTypes)
				{
					if (const UScriptStruct* SelectionColumnPtr = SelectionColumn.Get())
					{
						LocalColumns.Add(SelectionColumnPtr);
					}
				}

				// Collect all columns that are used in simple conditions.
				int32 NumConditions = Query.ConditionOperators.Num();
				for (int32 Index = 0; Index < NumConditions; ++Index)
				{
					FQueryDescription::EOperatorType Type = Query.ConditionTypes[Index];
					if (Type == FQueryDescription::EOperatorType::SimpleAll ||
						Type == FQueryDescription::EOperatorType::SimpleAny)
					{
						if (const UScriptStruct* ConditionColumn = Query.ConditionOperators[Index].Type.Get())
						{
							LocalColumns.Add(ConditionColumn);
						}
					}
				}

				// Collect all columns that are used for complex conditions.
				for (const TWeakObjectPtr<const UScriptStruct>& Column : ComplexConditionColumns)
				{
					if (const UScriptStruct* ColumnType = Column.Get())
					{
						LocalColumns.Add(ColumnType);
					}
				}

				MonitoredColumns.Empty();
				MonitoredColumns.Reserve(LocalColumns.Num());
				for (TObjectPtr<const UScriptStruct>& Column : LocalColumns)
				{
					MonitoredColumns.Add(Column);
				}
				
				QueryRevision = QueryNode->GetRevision();
			}
			else
			{
				MonitoredColumns.Empty();
				QueryRevision = 0;
			}
		}
	}

	void FRowMonitorNode::UpdateRows()
	{
		if (FRowHandleArray* ParentRowArray = ParentRow->GetMutableRows(); ParentRowArray == nullptr)
		{
			Rows.Empty();
			Rows.Append(ParentRow->GetRows());
		}
		ParentRevision = ParentRow->GetRevision();
	}

	void FRowMonitorNode::UpdateMonitoredColumns()
	{
		using namespace UE::Editor::DataStorage::Queries;

		static FName Name = FName("QueryStack Row Monitor node");

		for (QueryHandle Observer : Observers)
		{
			Storage.UnregisterQuery(Observer);
		}

		Observers.Reserve(MonitoredColumns.Num() * 2); // Add double, one for the OnAdd and one for the OnRemove.

		if (QueryNode)
		{
			auto OnAdd = [this](const FQueryDescription&, IQueryContext& Context)
				{
					AddedRows.Append(FRowHandleArrayView(Context.GetRowHandles(), FRowHandleArrayView::EFlags::IsUnique));
				};
			auto OnRemove = [this](const FQueryDescription&, IQueryContext& Context)
				{
					RemovedRows.Append(FRowHandleArrayView(Context.GetRowHandles(), FRowHandleArrayView::EFlags::IsUnique));
				};
			
			const FQueryDescription& QueryBase = Storage.GetQueryDescription(QueryNode->GetQuery());

			for (const TObjectPtr<const UScriptStruct>& Column : MonitoredColumns)
			{
				FString OnAddName = TEXT("QueryStack Row Monitor node: OnAdd - ");
				Column->GetFName().AppendString(OnAddName);
				FString OnRemoveName = TEXT("QueryStack Row Monitor node: OnRemove - ");
				Column->GetFName().AppendString(OnRemoveName);

				FQueryDescription OnAddObserver = QueryBase;
				OnAddObserver.Callback.Name = FName(OnAddName);
				OnAddObserver.Callback.Type = EQueryCallbackType::ObserveAdd;
				OnAddObserver.Callback.ExecutionMode = EExecutionMode::GameThread;
				OnAddObserver.Callback.Function = OnAdd;
				OnAddObserver.Callback.MonitoredType = Column;
				Observers.Add(Storage.RegisterQuery(MoveTemp(OnAddObserver)));

				FQueryDescription OnRemoveObserver = QueryBase;
				OnRemoveObserver.Callback.Name = FName(OnRemoveName);
				OnRemoveObserver.Callback.Type = EQueryCallbackType::ObserveRemove;
				OnRemoveObserver.Callback.ExecutionMode = EExecutionMode::GameThread;
				OnRemoveObserver.Callback.Function = OnRemove;
				OnRemoveObserver.Callback.MonitoredType = Column;
				Observers.Add(Storage.RegisterQuery(MoveTemp(OnRemoveObserver)));
			}
		}
		else
		{
			auto OnAdd = [this](IQueryContext& Context)
				{
					AddedRows.Append(FRowHandleArrayView(Context.GetRowHandles(), FRowHandleArrayView::EFlags::IsUnique));
				};
			auto OnRemove = [this](IQueryContext& Context)
				{
					RemovedRows.Append(FRowHandleArrayView(Context.GetRowHandles(), FRowHandleArrayView::EFlags::IsUnique));
				};

			for (const TObjectPtr<const UScriptStruct>& Column : MonitoredColumns)
			{
				FString OnAddName = TEXT("QueryStack Row Monitor node: OnAdd - ");
				Column->GetFName().AppendString(OnAddName);
				FString OnRemoveName = TEXT("QueryStack Row Monitor node: OnRemove - ");
				Column->GetFName().AppendString(OnRemoveName);

				Observers.Add(Storage.RegisterQuery(Select(FName(OnAddName),
					FObserver(FObserver::EEvent::Add, Column.Get())
						.SetExecutionMode(EExecutionMode::GameThread), OnAdd)
					.Compile()));
				Observers.Add(Storage.RegisterQuery(Select(FName(OnRemoveName),
					FObserver(FObserver::EEvent::Remove, Column.Get())
						.SetExecutionMode(EExecutionMode::GameThread), OnRemove)
					.Compile()));
			}
		}
	}
} // namespace UE::Editor::DataStorage::QueryStack
