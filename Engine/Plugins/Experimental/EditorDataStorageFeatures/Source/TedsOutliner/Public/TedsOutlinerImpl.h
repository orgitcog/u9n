// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "ISceneOutlinerHierarchy.h"
#include "ISceneOutlinerMode.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

namespace UE::Editor::DataStorage
{
	class IUiProvider;
	class ICompatibilityProvider;
} // namespace UE::Editor::DataStorage

struct FTypedElementWidgetConstructor;
class SWidget;

namespace UE::Editor::Outliner
{
// Struct storing information on how hierarchies are handled in the TEDS Outliner
struct FTedsOutlinerHierarchyData
{
	/** A delegate used to get the parent row handle for a given row */
	DECLARE_DELEGATE_RetVal_OneParam(DataStorage::RowHandle, FGetParentRowHandle, const void* /* InColumnData */);

	/** A delegate used to get the children row handles for a given row */
	DECLARE_DELEGATE_RetVal_OneParam(TArrayView<DataStorage::RowHandle>, FGetChildrenRowsHandles, void* /* InColumnData */);
	
	/** A delegate used to set the parent row handle for a given row */
	DECLARE_DELEGATE_TwoParams(FSetParentRowHandle, void* /* InColumnData */, DataStorage::RowHandle /* InParentRowHandle */);

	FTedsOutlinerHierarchyData(const UScriptStruct* InHierarchyColumn, const FGetParentRowHandle& InGetParent, const FSetParentRowHandle& InSetParent, const FGetChildrenRowsHandles& InGetChildren)
		: HierarchyColumn(InHierarchyColumn)
		, GetParent(InGetParent)
		, GetChildren(InGetChildren)
		, SetParent(InSetParent)
	{
	
	}

	// The column that contains the parent row handle for rows
	const UScriptStruct* HierarchyColumn;

	// Function to get parent row handle
	FGetParentRowHandle GetParent;

	// Function to get the children row handle
	FGetChildrenRowsHandles GetChildren;

	// Function to set the parent row handle
	FSetParentRowHandle SetParent;
	
	// Get the default hierarchy data for the TEDS Outliner that uses FTableRowParentColumn to get the parent
	static FTedsOutlinerHierarchyData GetDefaultHierarchyData()
	{
		const FGetParentRowHandle RowHandleGetter = FGetParentRowHandle::CreateLambda([](const void* InColumnData)
			{
				if(const FTableRowParentColumn* ParentColumn = static_cast<const FTableRowParentColumn *>(InColumnData))
				{
					return ParentColumn->Parent;
				}

				return DataStorage::InvalidRowHandle;
			});

		const FSetParentRowHandle RowHandleSetter = FSetParentRowHandle::CreateLambda([](void* InColumnData,
			DataStorage::RowHandle InRowHandle)
			{
				if(FTableRowParentColumn* ParentColumn = static_cast<FTableRowParentColumn *>(InColumnData))
				{
					ParentColumn->Parent = InRowHandle;
				}
			});
		
		return FTedsOutlinerHierarchyData(FTableRowParentColumn::StaticStruct(), RowHandleGetter, RowHandleSetter, FGetChildrenRowsHandles());
	}
};

struct FTedsOutlinerParams
{
	TEDSOUTLINER_API FTedsOutlinerParams(SSceneOutliner* InSceneOutliner);

	SSceneOutliner* SceneOutliner;

	// The query description that will be used to populate rows in the TEDS-Outliner
	TAttribute<DataStorage::FQueryDescription> QueryDescription;
	
	// TEDS queries that will be used to create filters in this Outliner
	// TEDS-Outliner TODO: Can we consolidate this with the SceneOutliner API to create filters? Currently has to be separate because FTEDSOutlinerFilter
	// needs a reference to the mode which is not possible since filters with the Outliner API are added before the mode is init
	TMap<FName, const DataStorage::FQueryDescription> FilterQueries;

	// If true, this Outliner will automatically add all TEDS tags and columns as filters
	bool bUseDefaultTedsFilters;

	// If true, this Outliner will include a column for row handle
	bool bShowRowHandleColumn;

	// If true, parent nodes will be remain visible if a child passes all filters even if the parent fails a filter
	bool bForceShowParents;

	// If true, the Teds Outliner will create observers to track addition/removal of rows and update the Outliner
	bool bUseDefaultObservers;

	// If specified, this is how the TEDS Outliner will handle hierarchies. If not specified - there will be no hierarchies shown as a
	// parent-child relation in the tree view
	TOptional<FTedsOutlinerHierarchyData> HierarchyData;

	// The selection set to use for this Outliner, unset = don't propagate tree selection to the TEDS column
	TOptional<FName> SelectionSetOverride;

	// The purpose to use when generating widgets for row/column pairs through TEDS UI
	DataStorage::IUiProvider::FPurposeID CellWidgetPurpose;

	// The purpose to use when generating widgets for column headers through TEDS UI
	DataStorage::IUiProvider::FPurposeID HeaderWidgetPurpose;

	// The purpose to use when generating widgets for the "Item Label" column through TEDS UI
	DataStorage::IUiProvider::FPurposeID LabelWidgetPurpose;
};


// This class is meant to be a model to hold functionality to create a "table viewer" in TEDS that can be
// attached to any view/UI.
// TEDS-Outliner TODO: This class still has a few outliner implementation details leaking in that should be removed
class TEDSOUTLINER_API FTedsOutlinerImpl : public TSharedFromThis<FTedsOutlinerImpl>
{

public:
	// Helper function to create a label widget for a given row
	static TSharedRef<SWidget> CreateLabelWidget(
		DataStorage::ICoreProvider& Storage,
		DataStorage::IUiProvider& StorageUi,
		ISceneOutliner& Outliner,
		DataStorage::IUiProvider::FPurposeID LabelWidgetPurpose,
		DataStorage::RowHandle Row,
		const STableRow<FSceneOutlinerTreeItemPtr>& RowItem,
		bool bIsInteractable = true);

	FTedsOutlinerImpl(const FTedsOutlinerParams& InParams, ISceneOutlinerMode* InMode);
	virtual ~FTedsOutlinerImpl();

	void Init();

	// TEDS construct getters
	DataStorage::ICoreProvider* GetStorage() const;
	DataStorage::IUiProvider* GetStorageUI() const;
	DataStorage::ICompatibilityProvider* GetStorageCompatibility() const;

	TOptional<FName> GetSelectionSetName() const;

	// Delegate fired when the selection in TEDS changes, only if SelectionSetName is set
	DECLARE_MULTICAST_DELEGATE(FOnTedsOutlinerSelectionChanged)
	FOnTedsOutlinerSelectionChanged& OnSelectionChanged();

	// Delegate fired when the hierarchy changes due to item addition/removal/move
	ISceneOutlinerHierarchy::FHierarchyChangedEvent& OnHierarchyChanged();

	// Delegate to check if a certain outliner item is compatible with this TEDS Outliner Impl - set by the system using FTedsOutlinerImpl
	DECLARE_DELEGATE_RetVal_OneParam(bool, FIsItemCompatible, const ISceneOutlinerTreeItem&)
	FIsItemCompatible& IsItemCompatible();

	// Update the selection in TEDS to the input rows, only if SelectionSetName is set
	void SetSelection(const TArray<DataStorage::RowHandle>& InSelectedRows);

	// Helper function to create a label widget for a given row
	TSharedRef<SWidget> CreateLabelWidgetForItem(DataStorage::RowHandle InRowHandle, const STableRow<FSceneOutlinerTreeItemPtr>& InRow, bool bIsInteractable = true) const;

	// Get the hierarchy data associated with this table viewer
	const TOptional<FTedsOutlinerHierarchyData>& GetHierarchyData();
	
	// Add an external query to the Outliner
	void AddExternalQuery(FName QueryName, const DataStorage::FQueryDescription& InQueryDescription);
	void RemoveExternalQuery(FName QueryName);

	// Append all external queries into the given query
	void AppendExternalQueries(DataStorage::FQueryDescription& OutQuery);

	// TEDS-Outliner TODO: This should live in TEDS long term
	// Funtion to combine 2 queries (adds to second query to the first)
	static void AppendQuery(DataStorage::FQueryDescription& Query1, const DataStorage::FQueryDescription& Query2);

	// Outliner specific functionality
	void CreateItemsFromQuery(TArray<FSceneOutlinerTreeItemPtr>& OutItems, ISceneOutlinerMode* InMode) const;
	void CreateChildren(const FSceneOutlinerTreeItemPtr& Item, TArray<FSceneOutlinerTreeItemPtr>& OutChildren) const;

	// Get the parent row for a given row
	DataStorage::RowHandle GetParentRow(DataStorage::RowHandle InRowHandle);

	bool ShouldForceShowParentRows();

	// Recompile all queries used by this table viewer
	void RecompileQueries();

	// Unregister all queries used by this table viewer
	void UnregisterQueries();

protected:
	
	void OnItemAdded(DataStorage::RowHandle ItemRowHandle);
	void OnItemRemoved(DataStorage::RowHandle ItemRowHandle);
	void ClearSelection() const;
	void Tick();

	void CreateFilterQueries();

	// Check if this row can be displayed in this table viewer
	bool CanDisplayRow(DataStorage::RowHandle ItemRowHandle) const;

protected:
	// TEDS Storage Constructs
	DataStorage::ICoreProvider* Storage{ nullptr };
	DataStorage::IUiProvider* StorageUi{ nullptr };
	DataStorage::ICompatibilityProvider* StorageCompatibility{ nullptr };

	FTedsOutlinerParams CreationParams;

	// The purpose to use when generating widgets for row/column pairs through TEDS UI
	DataStorage::IUiProvider::FPurposeID CellWidgetPurpose;

	// The purpose to use when generating widgets for the "Item Label" column through TEDS UI
	DataStorage::IUiProvider::FPurposeID LabelWidgetPurpose;
	
	// Initial query provided by user
	TAttribute<DataStorage::FQueryDescription> InitialQueryDescription;

	// External queries that are currently active (e.g Filters)
	TMap<FName, DataStorage::FQueryDescription> ExternalQueries;

	// Optional Hierarchy Data
	TOptional<FTedsOutlinerHierarchyData> HierarchyData;

	// Querys to track row handle collection, addition and removal
	DataStorage::QueryHandle RowHandleQuery = DataStorage::InvalidQueryHandle;
	DataStorage::QueryHandle RowAdditionQuery = DataStorage::InvalidQueryHandle;
	DataStorage::QueryHandle RowRemovalQuery = DataStorage::InvalidQueryHandle;

	// Query to get all child rows
	DataStorage::QueryHandle ChildRowHandleQuery = DataStorage::InvalidQueryHandle;

	// Query to track when a row's parent gets changed
	DataStorage::QueryHandle UpdateParentQuery = DataStorage::InvalidQueryHandle;

	// Query to get all selected rows, track selection added, track selection removed
	DataStorage::QueryHandle SelectedRowsQuery = DataStorage::InvalidQueryHandle;
	DataStorage::QueryHandle SelectionAddedQuery = DataStorage::InvalidQueryHandle;
	DataStorage::QueryHandle SelectionRemovedQuery = DataStorage::InvalidQueryHandle;

	// Query to track when a row's label gets changed
	DataStorage::QueryHandle LabelUpdateQuery = DataStorage::InvalidQueryHandle;
	
	TOptional<FName> SelectionSetName;
	bool bSelectionDirty = false;

	// If true, parent nodes will remain visible if a child passes all filters even if the parent fails a filter
	bool bForceShowParents = true;
	
	// Ticker for selection updates so we don't fire the delegate multiple times in one frame for multi select
	FTSTicker::FDelegateHandle TickerHandle;
	
	FOnTedsOutlinerSelectionChanged OnTedsOutlinerSelectionChanged;

	// Scene Outliner specific constructors
	ISceneOutlinerMode* SceneOutlinerMode;
	SSceneOutliner* SceneOutliner;

	// Event fired when the hierarchy changes (addition/removal/move)
	ISceneOutlinerHierarchy::FHierarchyChangedEvent HierarchyChangedEvent;

	// Delegate to check if an item is compatible with this table viewer
	FIsItemCompatible IsItemCompatibleWithTeds;

	// Whether the row query is using the FConditions syntax (TColumn<FMyColumn>()) or the old syntax (.All().Any().None())
	bool bUsingQueryConditionsSyntax = false;

	// Addition and Label Updates are deferred because they access data storage implicitly
	// Deletion is currently not deferred to work nicely with object lifecycles in some cases - but can be once everything goes through the query stack
	TSet<DataStorage::RowHandle> RowsPendingAddition;
	TSet<DataStorage::RowHandle> RowsPendingLabelUpdate;
};
} // namsepace UE::Editor::Outliner
