// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "Dataflow/DataflowToolRegistry.h"
#include "ChaosClothAsset/ClothEditorToolBuilders.h"
#include "ChaosClothAsset/SelectionNode.h"
#include "ChaosClothAsset/TransferSkinWeightsNode.h"
#include "ChaosClothAsset/WeightMapNode.h"
#include "ChaosClothAsset/ClothToolActionCommandBindings.h"

#define LOCTEXT_NAMESPACE "ChaosClothAssetEditorToolsModule"

namespace UE::Chaos::ClothAsset
{
	class FChaosClothAssetEditorToolsModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override
		{
			UE::Dataflow::FDataflowToolRegistry& ToolRegistry = UE::Dataflow::FDataflowToolRegistry::Get();
		
			const TSharedRef<const FClothToolActionCommandBindings> ClothToolActions = MakeShared<FClothToolActionCommandBindings>();

			ToolRegistry.AddNodeToToolMapping(FChaosClothAssetWeightMapNode::StaticType(),          // Node type
				NewObject<UClothEditorWeightMapPaintToolBuilder>(),                                 // Tool builder
				ClothToolActions,                                                                   // Tool action bindings
				FSlateIcon(FName("ClothStyle"), FName("ChaosClothAssetEditor.AddWeightMapNode")),   // Button icon (see FChaosClothAssetEditorStyle)
				LOCTEXT("AddWeightMapNodeButtonText", "Cloth Weight Map"), FName("Cloth"));                               // Button text

			ToolRegistry.AddNodeToToolMapping(FChaosClothAssetSelectionNode_v2::StaticType(), 
				NewObject<UClothMeshSelectionToolBuilder>(), 
				ClothToolActions, 
				FSlateIcon(FName("ClothStyle"), FName("ChaosClothAssetEditor.AddMeshSelectionNode")),
				LOCTEXT("AddSelectionNodeButtonText", "Cloth Mesh Selection"), FName("Cloth"));

			ToolRegistry.AddNodeToToolMapping(FChaosClothAssetTransferSkinWeightsNode::StaticType(), 
				NewObject<UClothTransferSkinWeightsToolBuilder>(), 
				ClothToolActions, 
				FSlateIcon(FName("ClothStyle"), FName("ChaosClothAssetEditor.AddTransferSkinWeightsNode")),
				LOCTEXT("AddTransferSkinWeightNodeButtonText", "Cloth Skinning Transfer"), FName("Cloth"));
		}

		virtual void ShutdownModule() override
		{
			UE::Dataflow::FDataflowToolRegistry& ToolRegistry = UE::Dataflow::FDataflowToolRegistry::Get();
			ToolRegistry.RemoveNodeToToolMapping(FChaosClothAssetWeightMapNode::StaticType());
			ToolRegistry.RemoveNodeToToolMapping(FChaosClothAssetSelectionNode_v2::StaticType());
			ToolRegistry.RemoveNodeToToolMapping(FChaosClothAssetTransferSkinWeightsNode::StaticType());
		}
	};
}

IMPLEMENT_MODULE(UE::Chaos::ClothAsset::FChaosClothAssetEditorToolsModule, ChaosClothAssetEditorTools)

#undef LOCTEXT_NAMESPACE
