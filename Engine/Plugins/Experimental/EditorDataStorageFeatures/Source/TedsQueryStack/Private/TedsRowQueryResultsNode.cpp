// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsRowQueryResultsNode.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"

namespace UE::Editor::DataStorage::QueryStack
{
	FRowQueryResultsNode::FRowQueryResultsNode(ICoreProvider& Storage, TSharedPtr<IQueryNode> InQueryNode, ESyncFlags SyncFlags)
		: QueryNode(MoveTemp(InQueryNode))
		, Storage(Storage)
		, SyncFlags(SyncFlags)
	{
		Refresh();
		QueryRevision = QueryNode->GetRevision();
	}
	
	INode::RevisionId FRowQueryResultsNode::GetRevision() const
	{
		return Revision;
	}

	void FRowQueryResultsNode::Refresh()
	{
		if (EnumHasAllFlags(SyncFlags, ESyncFlags::IncrementWhenDifferent))
		{
			FRowHandleArray NewRows;
			RefreshInternal(NewRows);
			if (NewRows.Num() != Rows.Num())
			{
				Rows = MoveTemp(NewRows);
				Revision++;
			}
			else
			{
				Rows.Sort();
				NewRows.Sort();
				FRowHandleArrayView NewRowView = NewRows.GetRows();
				if (FMemory::Memcmp(NewRowView.GetData(), Rows.GetRows().GetData(), NewRowView.NumBytes()) != 0)
				{
					Rows = MoveTemp(NewRows);
					Revision++;
				}
			}
		}
		else
		{
			Rows.Empty();
			RefreshInternal(Rows);
			Revision++;
		}
	}

	void FRowQueryResultsNode::Update()
	{
		QueryNode->Update();

		if (EnumHasAllFlags(SyncFlags, ESyncFlags::RefreshOnUpdate) ||
			(EnumHasAllFlags(SyncFlags, ESyncFlags::RefreshOnQueryChange) && QueryRevision != QueryNode->GetRevision()))
		{
			Refresh();
			QueryRevision = QueryNode->GetRevision();
		}
	}

	FRowHandleArrayView FRowQueryResultsNode::GetRows() const
	{
		return Rows.GetRows();
	}

	FRowHandleArray* FRowQueryResultsNode::GetMutableRows()
	{
		return &Rows;
	}

	void FRowQueryResultsNode::RefreshInternal(FRowHandleArray& TargetRows)
	{
		using namespace UE::Editor::DataStorage::Queries;

		FQueryResult Result = Storage.RunQuery(QueryNode->GetQuery()); // This is optimized to only collect the number of rows.
		TargetRows.Reserve(Result.Count);

		Storage.RunQuery(QueryNode->GetQuery(), 
			EDirectQueryExecutionFlags::AllowBoundQueries | EDirectQueryExecutionFlags::IgnoreActivationCount,
			CreateDirectQueryCallbackBinding(
				[&TargetRows](IDirectQueryContext& Context)
				{
					TargetRows.Append(FRowHandleArrayView(Context.GetRowHandles(), FRowHandleArrayView::EFlags::IsUnique));
				}));
	}
} // namespace UE::Editor::DataStorage::QueryStack
