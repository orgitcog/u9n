// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsQueryNode.h"

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"

namespace UE::Editor::DataStorage::QueryStack
{
	FQueryNode::FQueryNode(ICoreProvider& Storage)
		: Storage(Storage)
	{
	}

	FQueryNode::FQueryNode(ICoreProvider& Storage, UE::Editor::DataStorage::FQueryDescription Query)
		: QueryHandle(Storage.RegisterQuery(MoveTemp(Query)))
		, Storage(Storage)
	{
	}

	FQueryNode::~FQueryNode()
	{
		Storage.UnregisterQuery(QueryHandle);
	}

	void FQueryNode::SetQuery(UE::Editor::DataStorage::FQueryDescription Query)
	{
		Storage.UnregisterQuery(QueryHandle);
		QueryHandle = Storage.RegisterQuery(MoveTemp(Query));
		Revision++;
	}

	void FQueryNode::ClearQuery()
	{
		if (QueryHandle != InvalidQueryHandle)
		{
			Storage.UnregisterQuery(QueryHandle);
			QueryHandle = InvalidQueryHandle;
			Revision++;
		}
	}

	INode::RevisionId FQueryNode::GetRevision() const
	{
		return Revision;
	}

	void FQueryNode::Update()
	{
		// Nothing to update.
	}

	QueryHandle FQueryNode::GetQuery() const
	{
		return QueryHandle;
	}
} // namespace UE::Editor::DataStorage::QueryStack
