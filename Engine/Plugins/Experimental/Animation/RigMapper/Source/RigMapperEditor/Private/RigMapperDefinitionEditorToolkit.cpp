// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigMapperDefinitionEditorToolkit.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "RigMapperGraph/RigMapperDefinitionEditorGraphNode.h"
#include "RigMapperGraph/SRigMapperDefinitionGraphEditor.h"
#include "RigMapperEditorModule.h"

#include "DetailsViewObjectFilter.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "RigMapperGraph/SRigMapperDefinitionStructureView.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "RigMapperDefinitionEditorToolkit"

const FName FRigMapperDefinitionEditorToolkit::DefinitionEditorGraphTabId("RigMapperEditor_DefinitionGraphView");
const FName FRigMapperDefinitionEditorToolkit::DefinitionEditorStructureTabId("RigMapperEditor_DefinitionStructureView");
const FName FRigMapperDefinitionEditorToolkit::DefinitionEditorDetailsTabId("RigMapperEditor_DefinitionDetailsView");

const TMap<FName, ERigMapperNodeType> FRigMapperDefinitionEditorToolkit::PropertyNameToNodeTypeMapping = {
	{ GET_MEMBER_NAME_CHECKED(URigMapperDefinition, Inputs), ERigMapperNodeType::Input },
	{ GET_MEMBER_NAME_CHECKED(URigMapperDefinition, Features), ERigMapperNodeType::Invalid },
	{ GET_MEMBER_NAME_CHECKED(FRigMapperFeatureDefinitions, Multiply), ERigMapperNodeType::Multiply },
	{ GET_MEMBER_NAME_CHECKED(FRigMapperFeatureDefinitions, WeightedSums), ERigMapperNodeType::WeightedSum },
	{ GET_MEMBER_NAME_CHECKED(FRigMapperFeatureDefinitions, SDKs), ERigMapperNodeType::SDK },
	{ GET_MEMBER_NAME_CHECKED(URigMapperDefinition, Outputs), ERigMapperNodeType::Output },
	{ GET_MEMBER_NAME_CHECKED(URigMapperDefinition, NullOutputs), ERigMapperNodeType::NullOutput },
};

void FRigMapperDefinitionEditorToolkit::Initialize(URigMapperDefinition* InDefinition, EToolkitMode::Type InMode, TSharedPtr<IToolkitHost> InToolkitHost)
{
	Definition = InDefinition;

	// todo
	// GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.AddSP(this, &FSimpleAssetEditor::HandleAssetPostImport);
	// FCoreUObjectDelegates::OnObjectsReplaced.AddSP(this, &FSimpleAssetEditor::OnObjectsReplaced);
	
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("Standalone_RigMapperDefinitionEditor_Layout_v1")
	->AddArea
	(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(DefinitionEditorStructureTabId, ETabState::OpenedTab)
				->SetSizeCoefficient(0.2)
			)
			->Split
			(
				FTabManager::NewStack() 
				->SetHideTabWell(true)
				->AddTab(DefinitionEditorGraphTabId, ETabState::OpenedTab)
				->SetSizeCoefficient(0.6)
			)
			->Split
			(
				FTabManager::NewStack() 
				->AddTab(DefinitionEditorDetailsTabId, ETabState::OpenedTab)
				->SetSizeCoefficient(0.2)
			)
		)
	);

	InitAssetEditor(
		InMode,
		InToolkitHost,
		FRigMapperEditorModule::AppIdentifier,
		Layout,
		true /*bCreateDefaultStandaloneMenu*/,
		true /*bCreateDefaultToolbar*/,
		InDefinition
	);

	FRigMapperEditorModule::RegisterRigMapperDefinitionToolbarEntries();
}

void FRigMapperDefinitionEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("RigMapperDefinitionEditorTabGroup", "Rig Mapper Definition Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	
	InTabManager->RegisterTabSpawner(DefinitionEditorGraphTabId, FOnSpawnTab::CreateSP(this, &FRigMapperDefinitionEditorToolkit::SpawnGraphTab))
		.SetDisplayName(LOCTEXT("RigMapperDefinitionEditorGraphViewName","Graph"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

	InTabManager->RegisterTabSpawner(DefinitionEditorStructureTabId, FOnSpawnTab::CreateSP(this, &FRigMapperDefinitionEditorToolkit::SpawnStructureTab))
		.SetDisplayName(LOCTEXT("RigMapperDefinitionEditorStructureViewName", "Structure"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.Tabs.Palette"));

	InTabManager->RegisterTabSpawner(DefinitionEditorDetailsTabId, FOnSpawnTab::CreateSP(this, &FRigMapperDefinitionEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("RigMapperDefinitionEditorDetailsViewName", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FRigMapperDefinitionEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(DefinitionEditorGraphTabId);
	InTabManager->UnregisterTabSpawner(DefinitionEditorStructureTabId);
	InTabManager->UnregisterTabSpawner(DefinitionEditorDetailsTabId);
}

FName FRigMapperDefinitionEditorToolkit::GetToolkitFName() const
{
	return FName("RigMapperDefinitionEditor");
}

FText FRigMapperDefinitionEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("RigMapperDefinitionEditorToolkitBaseToolkitName", "Rig Mapper Definition Editor");
}

FString FRigMapperDefinitionEditorToolkit::GetWorldCentricTabPrefix() const
{
	return "Rig Mapper Definition ";
}

FLinearColor FRigMapperDefinitionEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.1f, 0.2f, 0.5f );
}

TSharedRef<SDockTab> FRigMapperDefinitionEditorToolkit::SpawnGraphTab(const FSpawnTabArgs& SpawnTabArgs)
{
	// todo: search and replace (easy rename inputs, outputs and features)
	// todo: error log
	GraphEditor = SNew(SRigMapperDefinitionGraphEditor, Definition);

	GraphEditor->OnSelectionChanged.BindRaw(this, &FRigMapperDefinitionEditorToolkit::HandleGraphSelectionChanged);

	Definition->OnRigMapperDefinitionUpdated.AddRaw(this, &FRigMapperDefinitionEditorToolkit::HandleRigMapperDefinitionLoaded);
	
	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
	.TabColorScale(GetTabColorScale())
	.Label(LOCTEXT("RigMapperDefinitionEditorToolkitGraphTab", "Graph"))
	[
		GraphEditor.ToSharedRef()
	];
	
	return DockTab;
}

void FRigMapperDefinitionEditorToolkit::HandleRigMapperDefinitionLoaded()
{
	if (GraphEditor)
	{
		GraphEditor->RebuildGraph();
	}
	if (StructureView)
	{
		StructureView->RebuildTree();
	}
}

TSharedRef<IDetailCustomization> FRigMapperDefinitionEditorToolkit::FDetailsViewCustomization::MakeInstance()
{
	return MakeShared<FDetailsViewCustomization>();
}

void FRigMapperDefinitionEditorToolkit::FDetailsViewCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);
	
	DetailLayout.EditCategory("Animation|Rig Mapper", LOCTEXT("RigMapperDefinitionElements", "Elements"));
	DetailLayout.HideCategory("Animation");
	DetailLayout.HideCategory("Rig Mapper");
}

TSharedRef<SDockTab> FRigMapperDefinitionEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& SpawnTabArgs)
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	DetailsView = PropertyModule.CreateDetailView(Args);
	DetailsView->RegisterInstancedCustomPropertyLayout(URigMapperDefinition::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FDetailsViewCustomization::MakeInstance));
	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateRaw(this, &FRigMapperDefinitionEditorToolkit::HandleIsPropertyVisible));
	DetailsView->OnFinishedChangingProperties().AddRaw(this, &FRigMapperDefinitionEditorToolkit::HandleFinishedChangingProperties);
	DetailsView->SetObject(Definition);
	if (Definition)
	{
		Definition->SetFlags(RF_Transactional);
	}

	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
	.TabColorScale(GetTabColorScale())
	.Label(LOCTEXT("RigMapperDefinitionEditorToolkitDetailsTab", "Details"))
	[
		DetailsView.ToSharedRef()
	];
		
	return DockTab;
}

TSharedRef<SDockTab> FRigMapperDefinitionEditorToolkit::SpawnStructureTab(const FSpawnTabArgs& SpawnTabArgs)
{
	StructureView = SNew(SRigMapperDefinitionStructureView, Definition);
	StructureView->OnSelectionChanged.BindRaw(this, &FRigMapperDefinitionEditorToolkit::HandleStructureSelectionChanged);
		
	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
	.TabColorScale(GetTabColorScale())
	.Label(LOCTEXT("RigMapperDefinitionEditorToolkitStructureTab", "Structure"))
	[
		StructureView.ToSharedRef()
	];

	return DockTab;
}


bool FRigMapperDefinitionEditorToolkit::HandleIsPropertyVisible(const FPropertyAndParent& PropertyAndParent)
{
	const int32 ArrayIndex = PropertyAndParent.ArrayIndex;
	const FName PropertyName = PropertyAndParent.Property.GetFName();
	
	if (StructureView->IsSelectionEmpty())
	{
		return true;
	}
	
	if (PropertyAndParent.ParentProperties.Num() > 2)
	{
		return true;
	}
	
	// TODO: turn off visibility logic in the Details Panel for now (MH-13360) as considered confusing to users; we may revisit this
	//if (PropertyNameToNodeTypeMapping.Contains(PropertyName))
	//{
	//	return StructureView->IsNodeOrChildSelected(PropertyNameToNodeTypeMapping[PropertyName], ArrayIndex);
	//}
	return true;
}

void FRigMapperDefinitionEditorToolkit::HandleFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	FPropertyChangedEvent Event = PropertyChangedEvent;
	Event.ObjectIteratorIndex = 0;
	
	const EPropertyChangeType::Type ChangeType = Event.ChangeType;
	const FName PropertyName = Event.GetPropertyName();
	const FName ParentName = Event.GetMemberPropertyName();
	const FName InputName = GET_MEMBER_NAME_CHECKED(URigMapperDefinition, Inputs);
	const FName OutputName = GET_MEMBER_NAME_CHECKED(URigMapperDefinition, Outputs);
	const FName FeaturesName = GET_MEMBER_NAME_CHECKED(URigMapperDefinition, Features);
	
	const FName NullOutputName = GET_MEMBER_NAME_CHECKED(URigMapperDefinition, NullOutputs);

	const bool bInput = PropertyName == InputName && ParentName == InputName;
	const bool bOutput = !bInput && (PropertyName == OutputName || PropertyName == OutputName.ToString() + TEXT("_Key")) && ParentName == OutputName; // PropertyName will be Outputs_Key if renaming an output
	const bool bName = !bInput && !bOutput && PropertyName == GET_MEMBER_NAME_CHECKED(FRigMapperFeature, Name);
	const bool bFeaturesChildProperty = ParentName == FeaturesName;
	const bool bSelectNew = (ChangeType == EPropertyChangeType::Duplicate || ChangeType == EPropertyChangeType::ArrayAdd);
	const bool bNullOutput = PropertyName == NullOutputName && ParentName == NullOutputName;
	const bool bRename = ChangeType == EPropertyChangeType::ValueSet && (bInput || bOutput || bName || bFeaturesChildProperty || bNullOutput);
	
	GraphEditor->RebuildGraph();
	StructureView->RebuildTree();
	
	if (bSelectNew || bRename)
	{
		if (bInput)
		{
			int32 InputIndex = Event.GetArrayIndex(PropertyName.ToString());
			StructureView->SelectNode(Definition->Inputs[InputIndex], ERigMapperNodeType::Input, true);
			GraphEditor->SelectNodes({ Definition->Inputs[InputIndex] }, {}, {}, {});
		}
		else if (bOutput)
		{
			int32 OutputIndex = Event.GetArrayIndex(PropertyName.ToString());
			TArray<FString> OutputsInOrder;
			Definition->Outputs.GenerateKeyArray(OutputsInOrder);
			StructureView->SelectNode(OutputsInOrder[OutputIndex], ERigMapperNodeType::Output, true);
			GraphEditor->SelectNodes({}, {}, { OutputsInOrder[OutputIndex] }, {});
		}
		if (bNullOutput)
		{
			int32 NullOutputIndex = Event.GetArrayIndex(PropertyName.ToString());
			StructureView->SelectNode(Definition->NullOutputs[NullOutputIndex], ERigMapperNodeType::NullOutput, true);
			GraphEditor->SelectNodes({ Definition->NullOutputs[NullOutputIndex] }, {}, {}, {});
		}
		else if (bFeaturesChildProperty)
		{
			if (const int32 MultiplyIndex = Event.GetArrayIndex(GET_MEMBER_NAME_CHECKED(FRigMapperFeatureDefinitions, Multiply).ToString()); MultiplyIndex != INDEX_NONE)
			{
				StructureView->SelectNode(Definition->Features.Multiply[MultiplyIndex].Name, ERigMapperNodeType::Multiply, true);
				GraphEditor->SelectNodes({}, { Definition->Features.Multiply[MultiplyIndex].Name }, {}, {});
			}
			else if (const int32 WsIndex = Event.GetArrayIndex(GET_MEMBER_NAME_CHECKED(FRigMapperFeatureDefinitions, WeightedSums).ToString()); WsIndex != INDEX_NONE)
			{
				StructureView->SelectNode(Definition->Features.WeightedSums[WsIndex].Name, ERigMapperNodeType::WeightedSum, true);
				GraphEditor->SelectNodes({}, { Definition->Features.WeightedSums[WsIndex].Name }, { }, {});
			}
			else if (const int32 SdkIndex = Event.GetArrayIndex(GET_MEMBER_NAME_CHECKED(FRigMapperFeatureDefinitions, SDKs).ToString()); SdkIndex != INDEX_NONE)
			{
				StructureView->SelectNode(Definition->Features.SDKs[SdkIndex].Name, ERigMapperNodeType::SDK, true);
				GraphEditor->SelectNodes({}, { Definition->Features.SDKs[SdkIndex].Name }, {}, {});
			}
		}
	}

	// todo: restore selection
	
	// todo: remove from selection if even remove / clear
}

void FRigMapperDefinitionEditorToolkit::HandleGraphSelectionChanged(const TSet<UObject*>& Nodes)
{
	StructureView->ClearSelection();
	
	for (const UObject* ObjectNode : Nodes)
	{
		if (const URigMapperDefinitionEditorGraphNode* Node = Cast<URigMapperDefinitionEditorGraphNode>(ObjectNode))
		{
			StructureView->SelectNode(Node->GetNodeName(), Node->GetNodeType(), true);
		}
	}
	
	DetailsView->ForceRefresh();
}

void FRigMapperDefinitionEditorToolkit::HandleStructureSelectionChanged(ESelectInfo::Type SelectInfo, TArray<FString> SelectedInputs, TArray<FString> SelectedFeatures, TArray<FString> SelectedOutputs, TArray<FString> SelectedNullOutputs)
{
	DetailsView->ForceRefresh();

	if (SelectInfo != ESelectInfo::Direct)
	{
		GraphEditor->SelectNodes(SelectedInputs, SelectedFeatures, SelectedOutputs, SelectedNullOutputs);
	};
}

#undef LOCTEXT_NAMESPACE
