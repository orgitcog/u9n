// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "TedsQueryStackInterfaces.h"

namespace UE::Editor::DataStorage::QueryStack
{
	/**
	 * Stores a view to a list of rows. The container that view is pointing to needs to be kept alive for
	 * as long as this Query Stack node is alive.
	 */
	class FRowViewNode : public IRowNode
	{
	public:
		FRowViewNode() = default;
		TEDSQUERYSTACK_API explicit FRowViewNode(FRowHandleArrayView Rows);
		virtual ~FRowViewNode() override = default;

		TEDSQUERYSTACK_API void MarkDirty();
		TEDSQUERYSTACK_API void ResetView(FRowHandleArrayView InRows);

		TEDSQUERYSTACK_API virtual RevisionId GetRevision() const override;
		TEDSQUERYSTACK_API virtual void Update() override;
		TEDSQUERYSTACK_API virtual FRowHandleArrayView GetRows() const override;
		TEDSQUERYSTACK_API virtual FRowHandleArray* GetMutableRows() override;

	private:
		FRowHandleArrayView Rows;
		RevisionId Revision = 0;
	};
} // namespace UE::Editor::DataStorage::QueryStack
