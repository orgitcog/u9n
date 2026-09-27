// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsRowViewNode.h"

namespace UE::Editor::DataStorage::QueryStack
{
	FRowViewNode::FRowViewNode(FRowHandleArrayView Rows)
		: Rows(Rows)
	{}

	void FRowViewNode::MarkDirty()
	{
		Revision++;
	}

	void FRowViewNode::ResetView(FRowHandleArrayView InRows)
	{
		Rows = InRows;
		MarkDirty();
	}

	INode::RevisionId FRowViewNode::GetRevision() const
	{
		return Revision;
	}

	void FRowViewNode::Update() 
	{
	}

	FRowHandleArrayView FRowViewNode::GetRows() const
	{
		return Rows;
	}

	FRowHandleArray* FRowViewNode::GetMutableRows()
	{
		return nullptr;
	}
} // namespace UE::Editor::DataStorage::QueryStack
