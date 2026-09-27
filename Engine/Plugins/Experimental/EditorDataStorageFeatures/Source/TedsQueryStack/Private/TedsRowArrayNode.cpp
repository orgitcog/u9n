// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsRowArrayNode.h"

namespace UE::Editor::DataStorage::QueryStack
{
	FRowArrayNode::FRowArrayNode(FRowHandleArray Rows)
		: Rows(MoveTemp(Rows))
	{
	}

	void FRowArrayNode::MarkDirty()
	{
		Revision++;
	}

	INode::RevisionId FRowArrayNode::GetRevision() const
	{
		return Revision;
	}

	void FRowArrayNode::Update()
	{
	}

	FRowHandleArrayView FRowArrayNode::GetRows() const
	{
		return Rows.GetRows();
	}

	FRowHandleArray* FRowArrayNode::GetMutableRows()
	{
		return &Rows;
	}
} // namespace UE::Editor::DataStorage::QueryStack
