// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorModule.h"

#include "Dataflow/DataflowConstructionVisualization.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEngineRendering.h"
#include "Dataflow/DataflowFreezeActionsCustomization.h"
#include "Dataflow/DataflowFunctionProperty.h"
#include "Dataflow/DataflowFunctionPropertyCustomization.h"
#include "Dataflow/DataflowSNodeFactories.h"
#include "Dataflow/MeshStatsConstructionVisualization.h"
#include "Dataflow/MeshConstructionVisualization.h"
#include "Dataflow/ScalarVertexPropertyGroupCustomization.h"
#include "Dataflow/DataflowToolRegistry.h"
#include "DataflowEditorTools/DataflowEditorWeightMapPaintTool.h"
#include "Dataflow/DataflowCollectionAddScalarVertexPropertyNode.h"

#include "Dataflow/DataflowInstanceDetails.h"

#include "PropertyEditorModule.h"
#include "EditorModeRegistry.h"
#include "Dataflow/DataflowCollectionEditSkinWeightsNode.h"
#include "DataflowEditorTools/DataflowEditorSkinWeightsPaintTool.h"
#include "DataflowEditorTools/DataflowEditorCorrectSkinWeightsNode.h"

#define LOCTEXT_NAMESPACE "DataflowEditor"

namespace UE::Dataflow::Private
{
	static const FName ScalarVertexPropertyGroupName = TEXT("ScalarVertexPropertyGroup");
	static const FName DataflowFunctionPropertyName = TEXT("DataflowFunctionProperty");
	static const FName DataflowVariableOverridesName = TEXT("DataflowVariableOverrides");
	static const FName DataflowFreezeActionsName = TEXT("DataflowFreezeActions");

	class FDataflowEditorWeightMapPaintToolActionCommands : public TInteractiveToolCommands<FDataflowEditorWeightMapPaintToolActionCommands>
	{
	public:
		FDataflowEditorWeightMapPaintToolActionCommands() : 
			TInteractiveToolCommands<FDataflowEditorWeightMapPaintToolActionCommands>(
				TEXT("DataflowEditorWeightMapPaintToolContext"),
				LOCTEXT("DataflowEditorWeightMapPaintToolContext", "Dataflow Weight Map Paint Tool Context"),
				NAME_None,
				FAppStyle::GetAppStyleSetName())
		{}

		virtual void GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs) override
		{
			ToolCDOs.Add(GetMutableDefault<UDataflowEditorWeightMapPaintTool>());
		}
	};

	class FDataflowEditorSkinWeightPaintToolActionCommands : public TInteractiveToolCommands<FDataflowEditorSkinWeightPaintToolActionCommands>
	{
	public:
		FDataflowEditorSkinWeightPaintToolActionCommands() : 
			TInteractiveToolCommands<FDataflowEditorSkinWeightPaintToolActionCommands>(
				TEXT("DataflowEditorSkinWeightPaintToolContext"),
				LOCTEXT("DataflowEditorSkinWeightPaintToolContext", "Dataflow Skin weight Paint Tool Context"),
				NAME_None,
				FAppStyle::GetAppStyleSetName())
		{}

		virtual void GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs) override
		{
			ToolCDOs.Add(GetMutableDefault<UDataflowEditorSkinWeightsPaintTool>());
		}
	};

	class FDataflowToolActionCommandBindings : public UE::Dataflow::FDataflowToolRegistry::IDataflowToolActionCommands
	{
	public:
		FDataflowToolActionCommandBindings()
		{
			FDataflowEditorWeightMapPaintToolActionCommands::Register();
			FDataflowEditorSkinWeightPaintToolActionCommands::Register();
		}

		virtual void UnbindActiveCommands(const TSharedPtr<FUICommandList>& UICommandList) const override
		{
			checkf(FDataflowEditorWeightMapPaintToolActionCommands::IsRegistered(), TEXT("Expected WeightMapPaintTool actions to have been registered"));
			FDataflowEditorWeightMapPaintToolActionCommands::Get().UnbindActiveCommands(UICommandList);

			checkf(FDataflowEditorSkinWeightPaintToolActionCommands::IsRegistered(), TEXT("Expected SkinWeightPaintTool actions to have been registered"));
			FDataflowEditorSkinWeightPaintToolActionCommands::Get().UnbindActiveCommands(UICommandList);
		}

		virtual void BindCommandsForCurrentTool(const TSharedPtr<FUICommandList>& UICommandList, UInteractiveTool* Tool) const override
		{
			if (ExactCast<UDataflowEditorWeightMapPaintTool>(Tool))
			{
				checkf(FDataflowEditorWeightMapPaintToolActionCommands::IsRegistered(), TEXT("Expected WeightMapPaintTool actions to have been registered"));
				FDataflowEditorWeightMapPaintToolActionCommands::Get().BindCommandsForCurrentTool(UICommandList, Tool);
			}
			if (ExactCast<UDataflowEditorSkinWeightsPaintTool>(Tool))
			{
				checkf(FDataflowEditorSkinWeightPaintToolActionCommands::IsRegistered(), TEXT("Expected SkinWeightPaintTool actions to have been registered"));
				FDataflowEditorSkinWeightPaintToolActionCommands::Get().BindCommandsForCurrentTool(UICommandList, Tool);
			}
		}
	};
}

const FColor FDataflowEditorModule::SurfaceColor = FLinearColor(0.6, 0.6, 0.6).ToRGBE();

void FDataflowEditorModule::StartupModule()
{
	using namespace UE::Dataflow::Private;

	FDataflowEditorStyle::Get();
	
	// Register type customizations
	if (FPropertyEditorModule* const PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->RegisterCustomPropertyTypeLayout(ScalarVertexPropertyGroupName, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&UE::Dataflow::FScalarVertexPropertyGroupCustomization::MakeInstance));
		PropertyModule->RegisterCustomPropertyTypeLayout(DataflowFunctionPropertyName, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&UE::Dataflow::FFunctionPropertyCustomization::MakeInstance));
		PropertyModule->RegisterCustomPropertyTypeLayout(DataflowVariableOverridesName, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataflowVariableOverridesDetails::MakeInstance));
		PropertyModule->RegisterCustomPropertyTypeLayout(DataflowFreezeActionsName, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&UE::Dataflow::FFreezeActionsCustomization::MakeInstance));
	}

	UE::Dataflow::RenderingCallbacks();

	UE::Dataflow::FDataflowToolRegistry& ToolRegistry = UE::Dataflow::FDataflowToolRegistry::Get();
	TSharedRef<const FDataflowToolActionCommandBindings> Actions = MakeShared<FDataflowToolActionCommandBindings>();
	
	ToolRegistry.AddNodeToToolMapping(FDataflowCollectionAddScalarVertexPropertyNode::StaticType(),
		NewObject<UDataflowEditorWeightMapPaintToolBuilder>(), Actions,
		FSlateIcon(FName("DataflowEditorStyle"), FName("Dataflow.PaintWeightMap")), LOCTEXT("AddWeightMapNodeButtonText", "Paint Weight Map"));

	ToolRegistry.AddNodeToToolMapping(FDataflowCollectionEditSkinWeightsNode::StaticType(), 
		NewObject<UDataflowEditorSkinWeightsPaintToolBuilder>(), Actions,
		FSlateIcon(FName("DataflowEditorStyle"), FName("Dataflow.EditSkinWeights")), LOCTEXT("AddSkinWeightNodeButtonText", "Edit Skin Weights"));
		
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowCorrectSkinWeightsNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowGetSkinningSelectionNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowSetSkinningSelectionNode);
	
	UE::Dataflow::FDataflowConstructionVisualizationRegistry& ConstructionVisualizationRegistry = UE::Dataflow::FDataflowConstructionVisualizationRegistry::GetInstance();
	ConstructionVisualizationRegistry.RegisterVisualization(MakeUnique<UE::Dataflow::FMeshStatsConstructionVisualization>());
	ConstructionVisualizationRegistry.RegisterVisualization(MakeUnique<UE::Dataflow::FMeshConstructionVisualization>());

	FDataflowEditorCommands::Register();
}

void FDataflowEditorModule::ShutdownModule()
{
	using namespace UE::Dataflow::Private;

	FEditorModeRegistry::Get().UnregisterMode(UDataflowEditorMode::EM_DataflowEditorModeId);

	// Deregister type customizations
	if (FPropertyEditorModule* const PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomPropertyTypeLayout(ScalarVertexPropertyGroupName);
		PropertyModule->UnregisterCustomPropertyTypeLayout(DataflowFunctionPropertyName);
		PropertyModule->UnregisterCustomPropertyTypeLayout(DataflowVariableOverridesName);
		PropertyModule->UnregisterCustomPropertyTypeLayout(DataflowFreezeActionsName);
	}

	FDataflowEditorCommands::Unregister();

	UE::Dataflow::FDataflowToolRegistry& ToolRegistry = UE::Dataflow::FDataflowToolRegistry::Get();
	ToolRegistry.RemoveNodeToToolMapping(FDataflowCollectionAddScalarVertexPropertyNode::StaticType());
	ToolRegistry.RemoveNodeToToolMapping(FDataflowCollectionEditSkinWeightsNode::StaticType());

	UE::Dataflow::FDataflowConstructionVisualizationRegistry& ConstructionVisualizationRegistry = UE::Dataflow::FDataflowConstructionVisualizationRegistry::GetInstance();
	ConstructionVisualizationRegistry.DeregisterVisualization(UE::Dataflow::FMeshStatsConstructionVisualization::Name);
	ConstructionVisualizationRegistry.DeregisterVisualization(UE::Dataflow::FMeshConstructionVisualization::Name);
}

IMPLEMENT_MODULE(FDataflowEditorModule, DataflowEditor)


#undef LOCTEXT_NAMESPACE
