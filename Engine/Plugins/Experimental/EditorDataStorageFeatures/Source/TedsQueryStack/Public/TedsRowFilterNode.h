// Copyright Epic Games, Inc. All Rights Reserved
 
#pragma once

#include "Elements/Common/TypedElementCommonTypes.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "TedsQueryStackInterfaces.h"

namespace UE::Editor::DataStorage
{
	class ICoreProvider;
}

namespace UE::Editor::DataStorage::QueryStack
{
	/**
	 * A specialized query stack node that takes another row node as input and only keeps rows that contain (or don't contain) a specific row
	 */
	template<TColumnType ColumnType>
	class FRowFilterNode : public IRowNode
	{
	public:

		/**
		 * Construct an FHierarchyTopLevelRowNode
		 *
		 * @param InStorage	The root elements of the new graph to be generated
		 * @param InParentRowNode Different visualization settings, such as whether it should display the referencers or the dependencies of the NewGraphRootIdentifiers
		 * @param bInRowsShouldHaveColumn If true, only keep rows that have the required column. If false, only keep rows that DON'T have the required column 
		 */
		FRowFilterNode(ICoreProvider* InStorage, const TSharedPtr<IRowNode>& InParentRowNode, bool bInRowsShouldHaveColumn)
		: Storage(InStorage)
		, ParentRowNode(InParentRowNode)
		, CachedParentRevisionID(InParentRowNode->GetRevision())
		, bRowsShouldHaveColumn(bInRowsShouldHaveColumn)
		{
			
		}
		
		virtual ~FRowFilterNode() override = default;

		// IRowNode interface
		virtual FRowHandleArrayView GetRows() const override
		{
			return Rows.GetRows();
		}
		
		virtual FRowHandleArray* GetMutableRows() override
		{
			return nullptr;
		}
		
		virtual RevisionId GetRevision() const override
		{
			return ParentRowNode->GetRevision();
		}
		
		virtual void Update() override
		{
			if (CachedParentRevisionID != ParentRowNode->GetRevision())
			{
				UpdateRows();
				CachedParentRevisionID = ParentRowNode->GetRevision();
			}
		}
		// ~IRowNode interface

	protected:

		// Update our list of top level rows
		void UpdateRows()
		{
			Rows.Reset();

			FRowHandleArrayView ParentRows = ParentRowNode->GetRows();

			for (RowHandle Row : ParentRows)
			{
				const bool bHasColumn = Storage->HasColumns<ColumnType>(Row);

				if (bRowsShouldHaveColumn ? bHasColumn : !bHasColumn)
				{
					Rows.Add(Row);
				}
			}
		}

	protected:

		FRowHandleArray Rows;
		ICoreProvider* Storage;
		TSharedPtr<IRowNode> ParentRowNode;
		RevisionId CachedParentRevisionID;
		bool bRowsShouldHaveColumn;
	};
}
