// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowOutlinerView.h"
#include "Dataflow/DataflowEditorPreviewSceneBase.h"
#include "Dataflow/DataflowOutlinerMode.h"
#include "TedsOutlinerImpl.h"
#include "Compatibility/SceneOutlinerTedsBridge.h"
#include "Dataflow/DataflowContent.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Columns/TypedElementVisibilityColumns.h"
#include "Elements/Common/EditorDataStorageFeatures.h"
#include "Elements/Common/TypedElementQueryDescription.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"

FDataflowOutlinerView::FDataflowOutlinerView(FDataflowPreviewSceneBase* InConstructionScene, FDataflowPreviewSceneBase* InSimulationScene, TObjectPtr<UDataflowBaseContent> InContent)
	: FDataflowNodeView(InContent)
	, OutlinerWidget(nullptr)
	, ConstructionScene(InConstructionScene)
	, SimulationScene(InSimulationScene)
{
	check(InContent);
}

FDataflowOutlinerView::~FDataflowOutlinerView()
{}

TSharedPtr<ISceneOutliner> FDataflowOutlinerView::CreateWidget()
{
	using namespace UE::Editor::DataStorage::Queries;
	using namespace UE::Editor::Outliner;
	UE::Editor::DataStorage::FQueryDescription RowQueryDescription = 
		Select()
		.Where(TColumn<FDataflowSceneObjectTag>(GetEditorContent()->GetDataflowOwner().GetFName()) || TColumn<FDataflowSceneStructTag>(GetEditorContent()->GetDataflowOwner().GetFName()) )
		.Compile();

	UE::Editor::DataStorage::FQueryDescription ColumnQueryDescription =
		Select()
		.ReadOnly<FTypedElementClassTypeInfoColumn, FVisibleInEditorColumn>()
		.Compile();

	FSceneOutlinerInitializationOptions InitOptions;
	InitOptions.bShowHeaderRow = true;
	InitOptions.FilterBarOptions.bHasFilterBar = true;
	InitOptions.bShowTransient = false;
	InitOptions.OutlinerIdentifier = "DataflowOutliner";
	
	UE::Editor::Outliner::FTedsOutlinerParams Params(nullptr);
	Params.QueryDescription = RowQueryDescription;
	Params.bUseDefaultTedsFilters = false;
	Params.bShowRowHandleColumn = false;

	// Add outliner filter queries
	UE::Editor::DataStorage::FQueryDescription SimulationFilterQuery =
		Select()
		.Where(TColumn<FDataflowSimulationObjectTag>())
		.Compile();
	UE::Editor::DataStorage::FQueryDescription ConstructionFilterQuery =
		Select()
		.Where(TColumn<FDataflowConstructionObjectTag>())
		.Compile();
	UE::Editor::DataStorage::FQueryDescription ElementsFilterQuery =
		Select()
		.Where(TColumn<FDataflowSceneStructTag>(GetEditorContent()->GetDataflowOwner().GetFName()))
		.Compile();
	UE::Editor::DataStorage::FQueryDescription ComponentsFilterQuery =
		Select()
		.Where(TColumn<FDataflowSceneObjectTag>(GetEditorContent()->GetDataflowOwner().GetFName()))
		.Compile();

	Params.FilterQueries.Emplace("Dataflow Construction", ConstructionFilterQuery);
	Params.FilterQueries.Emplace("Dataflow Simulation", SimulationFilterQuery);
	Params.FilterQueries.Emplace("Dataflow Elements", ElementsFilterQuery);
	Params.FilterQueries.Emplace("Dataflow Components", ComponentsFilterQuery);
	
	// Empty selection set name is currently the level editor
	Params.SelectionSetOverride = FName("DataflowSelection");

	InitOptions.ModeFactory = FCreateSceneOutlinerMode::CreateLambda([this, &Params](SSceneOutliner* Outliner)
	{
		Params.SceneOutliner = Outliner;
		return new FDataflowOutlinerMode(Params, ConstructionScene, SimulationScene);
	});
	
	UE::Editor::DataStorage::QueryHandle InitialColumnQuery = GetMutableDataStorageFeature<ICoreProvider>(
		UE::Editor::DataStorage::StorageFeatureName)->RegisterQuery(MoveTemp(ColumnQueryDescription));

	InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10));
	OutlinerWidget = SNew(SSceneOutliner, InitOptions);
	
	FSceneOutlinerTedsQueryBinder::GetInstance().AssignQuery(InitialColumnQuery, OutlinerWidget, Params.CellWidgetPurpose, Params.HeaderWidgetPurpose);
	FSceneOutlinerTedsQueryBinder::GetInstance().RegisterTreeItemIDDealiaser(OutlinerWidget, FTreeItemIDDealiaser::CreateLambda([](RowHandle RowHandle)
	{
		return FSceneOutlinerTreeItemID(RowHandle);
	}));

	return OutlinerWidget;
}

void FDataflowOutlinerView::SetSupportedOutputTypes()
{
	GetSupportedOutputTypes().Empty();
	GetSupportedOutputTypes().Add("FManagedArrayCollection");
}

void FDataflowOutlinerView::RefreshView()
{
	UpdateViewData();
}

void FDataflowOutlinerView::UpdateViewData()
{
	if(OutlinerWidget)
	{
		OutlinerWidget->CollapseAll();
		OutlinerWidget->FullRefresh();
	}
}

void FDataflowOutlinerView::ConstructionViewSelectionChanged(const TArray<UPrimitiveComponent*>& SelectedComponents, const TArray<FDataflowBaseElement*>& SelectedElements)
{
	OutlinerWidget->ClearSelection();
	
	using namespace UE::Editor::DataStorage;
	TArray<RowHandle> SelectedRowHandles;
	if (const ICompatibilityProvider* Compatibility = GetDataStorageFeature<ICompatibilityProvider>(CompatibilityFeatureName))
	{
		// Transfer components selection to outliner
		for(UPrimitiveComponent* SelectedComponent : SelectedComponents)
		{
			if (FSceneOutlinerTreeItemPtr SelectedTreeItem = OutlinerWidget->GetTreeItem(
				Compatibility->FindRowWithCompatibleObject(SelectedComponent), true))
			{
				OutlinerWidget->AddToSelection(SelectedTreeItem);
				OutlinerWidget->ScrollItemIntoView(SelectedTreeItem);
			}
		}
		// Transfer elements selection to outliner
		for(FDataflowBaseElement* SelectedElement : SelectedElements)
		{
			if (FSceneOutlinerTreeItemPtr SelectedTreeItem = OutlinerWidget->GetTreeItem(
				Compatibility->FindRowWithCompatibleObject(SelectedElement), true))
			{
				OutlinerWidget->AddToSelection(SelectedTreeItem);
				OutlinerWidget->ScrollItemIntoView(SelectedTreeItem);
			}
		}
	}
}

void FDataflowOutlinerView::SimulationViewSelectionChanged(const TArray<UPrimitiveComponent*>& SelectedComponents, const TArray<FDataflowBaseElement*>& SelectedElements)
{
	ConstructionViewSelectionChanged(SelectedComponents, SelectedElements);
}

void FDataflowOutlinerView::AddReferencedObjects(FReferenceCollector& Collector)
{
	FDataflowNodeView::AddReferencedObjects(Collector);
}
