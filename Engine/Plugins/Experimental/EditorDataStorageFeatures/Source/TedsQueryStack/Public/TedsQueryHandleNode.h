// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TedsQueryStackInterfaces.h"

namespace UE::Editor::DataStorage::QueryStack
{
	/**
	 * Stores a handle to a query. This should be used to pass an externally created query into the Query Stack.
	 */
	class FQueryHandleNode : public IQueryNode
	{
	public:
		TEDSQUERYSTACK_API FQueryHandleNode() = default;
		TEDSQUERYSTACK_API explicit FQueryHandleNode(QueryHandle Query);
		virtual ~FQueryHandleNode() override = default;

		TEDSQUERYSTACK_API void SetQuery(QueryHandle InQuery);

		TEDSQUERYSTACK_API virtual RevisionId GetRevision() const override;
		TEDSQUERYSTACK_API virtual void Update() override;
		TEDSQUERYSTACK_API virtual QueryHandle GetQuery() const override;

	private:
		QueryHandle Query = InvalidQueryHandle;
		RevisionId Revision = 0;
	};
} // namespace UE::Editor::DataStorage::QueryStack
