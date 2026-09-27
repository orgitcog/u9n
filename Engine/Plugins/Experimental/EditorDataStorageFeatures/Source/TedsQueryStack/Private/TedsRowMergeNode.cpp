// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsRowMergeNode.h"
#include "Elements/Framework/TypedElementRowHandleArray.h"

namespace UE::Editor::DataStorage::QueryStack
{
	FRowMergeNode::FRowMergeNode(TConstArrayView<TSharedPtr<IRowNode>> InParents, EMergeApproach InMergeApproach)
		: MergeApproach(InMergeApproach)
	{
		for (const TSharedPtr<IRowNode>& Parent : InParents)
		{
			Parents.Emplace<FParentInfo>({ .Parent = Parent, .Revision = Parent->GetRevision()});
		}
		Merge();
	}

	IRowNode::RevisionId FRowMergeNode::GetRevision() const
	{
		return Revision;
	}

	void FRowMergeNode::Update()
	{
		bool bRebuild = false;
		for (FParentInfo& ParentInfo : Parents)
		{
			ParentInfo.Parent->Update();
			RevisionId ParentRevision = ParentInfo.Parent->GetRevision();
			if (ParentInfo.Revision != ParentRevision)
			{
				bRebuild = true;
				ParentInfo.Revision = ParentRevision;
				break;
			}
		}

		if (bRebuild)
		{
			Merge();
			Revision++;
		}
	}

	FRowHandleArrayView FRowMergeNode::GetRows() const
	{
		return Rows.GetRows();
	}

	FRowHandleArray* FRowMergeNode::GetMutableRows()
	{
		return &Rows;
	}

	void FRowMergeNode::Merge()
	{
		Rows.Reset();

		int32 TotalCount = 0;
		for (FParentInfo& Parent : Parents)
		{
			TotalCount += Parent.Parent->GetRows().Num();
		}
		Rows.Reserve(TotalCount);

		for (FParentInfo& Parent : Parents)
		{
			Rows.Append(Parent.Parent->GetRows());
		}

		switch (MergeApproach)
		{
		case EMergeApproach::Append:
			break;
		case EMergeApproach::Sorted:
			Rows.Sort();
			break;
		case EMergeApproach::Unique:
			Rows.MakeUnique();
			break;
		case EMergeApproach::Repeating:
			Rows.ReduceToDuplicates();
			break;
		default:
			checkf(false, TEXT("Unexpected merge type in FQueryStack_RowMerge."));
			break;
		}
	}
} // namespace UE::Editor::DataStorage::QueryStack
