// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Framework/TypedElementRowHandleArray.h"
#include "Elements/Framework/TypedElementRowHandleArrayView.h"

namespace UE::Editor::DataStorage::QueryStack
{
	/** Base interface for a all query stack nodes. */
	class INode
	{
	public:
		using RevisionId = uint32;

		virtual ~INode() = default;
		
		virtual RevisionId GetRevision() const = 0;
		virtual void Update() = 0;
	};

	/** Query stack node that works on queries handles. These nodes are typically run in some fashion to be turned into a row node. */
	class IQueryNode : public INode
	{
	public:
		virtual ~IQueryNode() = default;
		/** Returns the handle to the query this node represents. */
		virtual QueryHandle GetQuery() const = 0;
	};

	/** Query stack node that works on row handles. */
	class IRowNode : public INode
	{
	public:
		virtual ~IRowNode() = default;
		/** Retrieve access to the rows used by this node. */
		virtual FRowHandleArrayView GetRows() const = 0;
		/** Retrieve write access to the rows used by this node. Is allowed to return null if write access can't be granted. */
		virtual FRowHandleArray* GetMutableRows() = 0;
	};
} // UE::Editor::DataStorage::QueryStack
