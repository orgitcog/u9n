// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Elements/Framework/TypedElementRowHandleArray.h"
#include "TedsQueryStackInterfaces.h"
#include "Misc/EnumClassFlags.h"

namespace UE::Editor::DataStorage
{
	class ICoreProvider;
} // namespace UE::Editor::DataStorage

namespace UE::Editor::DataStorage::QueryStack
{
	/**
	 * Used to convert an IQueryNode into an IRowNode by extracting the rows the query references optionally during update.
	 * This node is cheap to setup, but has diminishing returns when the number of rows increases and when updates happen frequently
	 * as it has to fully extract all rows from TEDS whenever Refresh is called. Use on small tables when the number of calls to 
	 * Refresh can be minimized.
	 */
	class FRowQueryResultsNode : public IRowNode
	{
	public:
		enum class ESyncFlags
		{
			None = 0,
			/** 
			 * Update the row list whenever "Update" is called. This can take several milliseconds for large lists. It's
			 * recommended to use a monitoring node to detect changes for large lists.
			 */
			RefreshOnUpdate = 1 << 0,
			/** Update the row list whenever the parent query changes. */
			RefreshOnQueryChange = 1 << 1,
			/** 
			 * Compares the previous row list with the current and only updates the revision if there are differences.
			 * This requires additional sorting and comparing, making the node substantially more expensive.
			 */
			IncrementWhenDifferent = 1 << 2
		};

		TEDSQUERYSTACK_API FRowQueryResultsNode(ICoreProvider& Storage, TSharedPtr<IQueryNode> InQueryNode, ESyncFlags SyncFlags);
		virtual ~FRowQueryResultsNode() override = default;

		TEDSQUERYSTACK_API void Refresh();

		TEDSQUERYSTACK_API virtual RevisionId GetRevision() const override;
		TEDSQUERYSTACK_API virtual void Update() override;
		TEDSQUERYSTACK_API virtual FRowHandleArrayView GetRows() const override;
		TEDSQUERYSTACK_API virtual FRowHandleArray* GetMutableRows() override;

	private:
		void RefreshInternal(FRowHandleArray& TargetRows);

		FRowHandleArray Rows;
		TSharedPtr<IQueryNode> QueryNode;
		ICoreProvider& Storage;
		RevisionId QueryRevision = 0;
		RevisionId Revision = 0;
		ESyncFlags SyncFlags = ESyncFlags::None;
	};

	ENUM_CLASS_FLAGS(FRowQueryResultsNode::ESyncFlags);
} // namespace UE::Editor::DataStorage::QueryStack
