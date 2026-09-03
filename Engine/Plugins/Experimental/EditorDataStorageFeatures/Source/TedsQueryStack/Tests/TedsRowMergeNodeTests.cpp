// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS

#include "TedsRowMergeNode.h"
#include "TedsRowViewNode.h"
#include "Tests/TestHarnessAdapter.h"

TEST_CASE_NAMED(TEDS_QueryStack_RowMergeNode_Tests, "Editor::DataStorage::QueryStack::FRowMergeNode", "[ApplicationContextMask][EngineFilter]")
{
	using namespace UE::Editor::DataStorage;
	using namespace UE::Editor::DataStorage::QueryStack;

	SECTION("No parents")
	{
		FRowMergeNode Node({}, FRowMergeNode::EMergeApproach::Append);
		CHECK_EQUALS(TEXT("Revision"), Node.GetRevision(), 0);
		CHECK_MESSAGE(TEXT("Default for rows is not empty."), Node.GetRows().IsEmpty());
		
		Node.Update();
	}

	SECTION("Append two nodes")
	{
		RowHandle ValueArray[] = {1, 2, 3};
		FRowHandleArrayView Values = FRowHandleArrayView( ValueArray,
			FRowHandleArrayView::EFlags::IsSorted | FRowHandleArrayView::EFlags::IsUnique);
		TSharedPtr<IRowNode> View0 = MakeShared<FRowViewNode>(Values);
		TSharedPtr<IRowNode> View1 = MakeShared<FRowViewNode>(Values);
		
		FRowMergeNode Node({View0, View1}, FRowMergeNode::EMergeApproach::Append);
		
		FRowHandleArrayView Rows = Node.GetRows();
		CHECK_MESSAGE(TEXT("Expected rows to not be sorted."), !Rows.IsSorted());
		CHECK_EQUALS(TEXT("Size"), Rows.Num(), 6);
		CHECK_EQUALS(TEXT("Rows[0]"), Rows[0], 1llu);
		CHECK_EQUALS(TEXT("Rows[1]"), Rows[1], 2llu);
		CHECK_EQUALS(TEXT("Rows[2]"), Rows[2], 3llu);
		CHECK_EQUALS(TEXT("Rows[3]"), Rows[3], 1llu);
		CHECK_EQUALS(TEXT("Rows[4]"), Rows[4], 2llu);
		CHECK_EQUALS(TEXT("Rows[5]"), Rows[5], 3llu);
	}

	SECTION("Merge two nodes sorted")
	{
		RowHandle ValueArray[] = { 1, 2, 3 }; 
		FRowHandleArrayView Values = FRowHandleArrayView( ValueArray,
			FRowHandleArrayView::EFlags::IsSorted | FRowHandleArrayView::EFlags::IsUnique);
		TSharedPtr<IRowNode> View0 = MakeShared<FRowViewNode>(Values);
		TSharedPtr<IRowNode> View1 = MakeShared<FRowViewNode>(Values);
		
		FRowMergeNode Node({ View0, View1 }, FRowMergeNode::EMergeApproach::Sorted);

		FRowHandleArrayView Rows = Node.GetRows();
		CHECK_MESSAGE(TEXT("Expected rows to be sorted."), Rows.IsSorted());
		CHECK_EQUALS(TEXT("Size"), Rows.Num(), 6);
		CHECK_EQUALS(TEXT("Rows[0]"), Rows[0], 1llu);
		CHECK_EQUALS(TEXT("Rows[1]"), Rows[1], 1llu);
		CHECK_EQUALS(TEXT("Rows[2]"), Rows[2], 2llu);
		CHECK_EQUALS(TEXT("Rows[3]"), Rows[3], 2llu);
		CHECK_EQUALS(TEXT("Rows[4]"), Rows[4], 3llu);
		CHECK_EQUALS(TEXT("Rows[5]"), Rows[5], 3llu);
	}

	SECTION("Uniquely merge two nodes")
	{
		RowHandle ValueArray[] = { 1, 2, 3 };
		FRowHandleArrayView Values = FRowHandleArrayView( ValueArray,
			FRowHandleArrayView::EFlags::IsSorted | FRowHandleArrayView::EFlags::IsUnique);
		TSharedPtr<IRowNode> View0 = MakeShared<FRowViewNode>(Values);
		TSharedPtr<IRowNode> View1 = MakeShared<FRowViewNode>(Values);

		FRowMergeNode Node({ View0, View1 }, FRowMergeNode::EMergeApproach::Unique);

		FRowHandleArrayView Rows = Node.GetRows();
		CHECK_MESSAGE(TEXT("Expected rows to be sorted."), Rows.IsSorted());
		CHECK_EQUALS(TEXT("Size"), Rows.Num(), 3);
		CHECK_EQUALS(TEXT("Rows[0]"), Rows[0], 1llu);
		CHECK_EQUALS(TEXT("Rows[1]"), Rows[1], 2llu);
		CHECK_EQUALS(TEXT("Rows[2]"), Rows[2], 3llu);
	}

	SECTION("Repeating of data in two nodes")
	{
		RowHandle ValueArray0[] = { 1, 2, 3 };
		RowHandle ValueArray1[] = { 2, 3, 4 };
		FRowHandleArrayView Values0 = FRowHandleArrayView( ValueArray0,
			FRowHandleArrayView::EFlags::IsSorted | FRowHandleArrayView::EFlags::IsUnique);
		FRowHandleArrayView Values1 = FRowHandleArrayView( ValueArray1,
			FRowHandleArrayView::EFlags::IsSorted | FRowHandleArrayView::EFlags::IsUnique);
		TSharedPtr<IRowNode> View0 = MakeShared<FRowViewNode>(Values0);
		TSharedPtr<IRowNode> View1 = MakeShared<FRowViewNode>(Values1);

		FRowMergeNode Node({ View0, View1 }, FRowMergeNode::EMergeApproach::Repeating);

		FRowHandleArrayView Rows = Node.GetRows();
		CHECK_MESSAGE(TEXT("Expected rows to be sorted."), Rows.IsSorted());
		CHECK_EQUALS(TEXT("Size"), Rows.Num(), 2);
		CHECK_EQUALS(TEXT("Rows[0]"), Rows[0], 2llu);
		CHECK_EQUALS(TEXT("Rows[1]"), Rows[1], 3llu);
	}

	SECTION("Repeating of data in three nodes")
	{
		RowHandle ValueArray0[] = { 1, 2, 3 };
		RowHandle ValueArray1[] = { 2, 3, 4 };
		RowHandle ValueArray2[] = { 1, 3 };

		FRowHandleArrayView Values0 = FRowHandleArrayView(ValueArray0,
			FRowHandleArrayView::EFlags::IsSorted | FRowHandleArrayView::EFlags::IsUnique);
		FRowHandleArrayView Values1 = FRowHandleArrayView(ValueArray1,
			FRowHandleArrayView::EFlags::IsSorted | FRowHandleArrayView::EFlags::IsUnique);
		FRowHandleArrayView Values2 = FRowHandleArrayView(ValueArray2,
			FRowHandleArrayView::EFlags::IsSorted | FRowHandleArrayView::EFlags::IsUnique);
		TSharedPtr<IRowNode> View0 = MakeShared<FRowViewNode>(Values0);
		TSharedPtr<IRowNode> View1 = MakeShared<FRowViewNode>(Values1);
		TSharedPtr<IRowNode> View2 = MakeShared<FRowViewNode>(Values2);

		FRowMergeNode Node({ View0, View1, View2 }, FRowMergeNode::EMergeApproach::Repeating);

		FRowHandleArrayView Rows = Node.GetRows();
		CHECK_MESSAGE(TEXT("Expected rows to be sorted."), Rows.IsSorted());
		CHECK_EQUALS(TEXT("Size"), Rows.Num(), 3);
		CHECK_EQUALS(TEXT("Rows[0]"), Rows[0], 1llu);
		CHECK_EQUALS(TEXT("Rows[1]"), Rows[1], 2llu);
		CHECK_EQUALS(TEXT("Rows[2]"), Rows[2], 3llu);
	}

	SECTION("Updating multiple nodes")
	{
		RowHandle Values[] = {1, 2, 3};

		static constexpr int32 ViewCount = 42;
		TArray<TSharedPtr<FRowViewNode>> Views;
		Views.Reserve(ViewCount);
		TArray<TSharedPtr<IRowNode>> Nodes;
		Nodes.Reserve(ViewCount);
		for (int32 Count = 0; Count < ViewCount; ++Count)
		{
			TSharedPtr<FRowViewNode> Node = MakeShared<FRowViewNode>(
				FRowHandleArrayView(Values, FRowHandleArrayView::EFlags::IsSorted | FRowHandleArrayView::EFlags::IsUnique));
			Views.Add(Node);
			Nodes.Add(Node);
		}

		FRowMergeNode Node(Nodes, FRowMergeNode::EMergeApproach::Sorted);

		Views[5]->MarkDirty();
		Node.Update();

		Views[14]->MarkDirty();
		Node.Update();

		Views[32]->MarkDirty();
		Node.Update();
	}
}

#endif // #if WITH_TESTS
