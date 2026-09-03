// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextFunctionItemDetails.h"

#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "StructUtils/InstancedStruct.h"
#include "AnimNextAssetWorkspaceAssetUserData.h"
#include "RigVMModel/RigVMGraph.h"
#include "WorkspaceItemMenuContext.h"
#include "IWorkspaceEditor.h"
#include "ScopedTransaction.h"
#include "RigVMModel/RigVMClient.h"
#include "EdGraph/RigVMEdGraph.h"
#include "ToolMenus.h"
#include "Framework/Commands/GenericCommands.h"
#include "Module/AnimNextModule_EditorData.h"

#define LOCTEXT_NAMESPACE "FAnimNextFunctionItemDetails"

namespace UE::AnimNext::Editor
{

bool FAnimNextFunctionItemDetails::HandleDoubleClick(const FToolMenuContext& ToolMenuContext) const
{
	const UWorkspaceItemMenuContext* WorkspaceItemContext = ToolMenuContext.FindContext<UWorkspaceItemMenuContext>();
	const UAssetEditorToolkitMenuContext* AssetEditorContext = ToolMenuContext.FindContext<UAssetEditorToolkitMenuContext>();
	if (WorkspaceItemContext && AssetEditorContext)
	{
		if (const TSharedPtr<UE::Workspace::IWorkspaceEditor> WorkspaceEditor = StaticCastSharedPtr<UE::Workspace::IWorkspaceEditor>(AssetEditorContext->Toolkit.Pin()))
		{
			const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = WorkspaceItemContext->SelectedExports[0].GetResolvedExport().GetData();
			if (Data.IsValid() && Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
			{
				const FAnimNextGraphFunctionOutlinerData& FunctionData = Data.Get<FAnimNextGraphFunctionOutlinerData>();
				if (FunctionData.SoftEditorObject.IsValid())
				{
					WorkspaceEditor->OpenObjects({ FunctionData.SoftEditorObject.Get() });
					return true;
				}
			}
		}
	}
	return false;
}

bool FAnimNextFunctionItemDetails::HandleSelected(const FToolMenuContext& ToolMenuContext) const
{
	const UWorkspaceItemMenuContext* WorkspaceItemContext = ToolMenuContext.FindContext<UWorkspaceItemMenuContext>();
	const UAssetEditorToolkitMenuContext* AssetEditorContext = ToolMenuContext.FindContext<UAssetEditorToolkitMenuContext>();
	if (WorkspaceItemContext && AssetEditorContext)
	{
		if (const TSharedPtr<UE::Workspace::IWorkspaceEditor> WorkspaceEditor = StaticCastSharedPtr<UE::Workspace::IWorkspaceEditor>(AssetEditorContext->Toolkit.Pin()))
		{
			const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = WorkspaceItemContext->SelectedExports[0].GetResolvedExport().GetData();
			if (Data.IsValid() && Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
			{
				const FAnimNextGraphFunctionOutlinerData& FunctionData = Data.Get<FAnimNextGraphFunctionOutlinerData>();
				if (FunctionData.SoftEditorObject.IsValid())
				{
					WorkspaceEditor->SetDetailsObjects({ FunctionData.SoftEditorObject.Get() });
					return true;
				}
			}
		}
	}

	return false;
}

bool FAnimNextFunctionItemDetails::CanDelete(const FWorkspaceOutlinerItemExport& Export) const
{
	const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = Export.GetData();
	if (Data.IsValid() && Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
	{
		const FAnimNextGraphFunctionOutlinerData& FunctionData = Export.GetData().Get<FAnimNextGraphFunctionOutlinerData>();
		if (FunctionData.SoftEditorObject.IsValid())
		{
			URigVMEdGraph* EdGraph = FunctionData.SoftEditorObject.Get();
			if (EdGraph->bAllowDeletion)
			{
				return true;
			}
		}
	}
	return false;
}

void FAnimNextFunctionItemDetails::Delete(TConstArrayView<FWorkspaceOutlinerItemExport> Exports) const
{
	TMap<UAnimNextRigVMAssetEditorData*, TArray<UAnimNextRigVMAssetEntry*>> EntriesToDelete;
	for (const FWorkspaceOutlinerItemExport& Export : Exports)
	{
		const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = Export.GetData();
		if (Data.IsValid() && Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
		{
			const FAnimNextGraphFunctionOutlinerData& FunctionData = Export.GetData().Get<FAnimNextGraphFunctionOutlinerData>();
			if (FunctionData.SoftEditorObject.IsValid())
			{
				if(FunctionData.SoftEdGraphNode.IsValid())
				{
					const URigVMEdGraphNode* EdGraphNode = FunctionData.SoftEdGraphNode.Get();
					if (EdGraphNode->CanUserDeleteNode())
					{
						if (URigVMGraph* Model = EdGraphNode->GetModel())
						{
							if (URigVMNode* ModelNode = Model->FindNodeByName(*EdGraphNode->GetModelNodePath()))
							{
								FScopedTransaction Transaction(LOCTEXT("DeleteFunctionInOutliner", "Delete Function"));
								EdGraphNode->GetController()->RemoveNode(ModelNode, true, true);
							}
						}
					}
				}
				else
				{
					URigVMEdGraph* EdGraph = FunctionData.SoftEditorObject.Get();
					if (EdGraph->bAllowDeletion)
					{
						if(IRigVMClientHost* ClientHost = EdGraph->GetImplementingOuter<IRigVMClientHost>())
						{
							if(URigVMController* Controller = ClientHost->GetController(ClientHost->GetLocalFunctionLibrary()))
							{
								FScopedTransaction Transaction(LOCTEXT("DeleteFunctionInOutliner", "Delete Function"));
								URigVMLibraryNode* LibraryNode = CastChecked<URigVMLibraryNode>(EdGraph->GetModel()->GetOuter());
								Controller->RemoveFunctionFromLibrary(LibraryNode->GetFName(), true, true);
							}
						}
					}
				}
			}
		}
	}
}

bool FAnimNextFunctionItemDetails::CanRename(const FWorkspaceOutlinerItemExport& Export) const
{
	const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = Export.GetData();
	if (Data.IsValid() && Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
	{
		const FAnimNextGraphFunctionOutlinerData& FunctionData = Export.GetData().Get<FAnimNextGraphFunctionOutlinerData>();
		if (FunctionData.SoftEditorObject.IsValid())
		{
			URigVMEdGraph* EdGraph = FunctionData.SoftEditorObject.Get();
			if (EdGraph->bAllowRenaming)
			{
				return true;
			}
		}
	}

	return false;
}

void FAnimNextFunctionItemDetails::Rename(const FWorkspaceOutlinerItemExport& Export, const FText& InName) const
{
	const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = Export.GetData();
	if (Data.IsValid() && Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
	{
		const FAnimNextGraphFunctionOutlinerData& FunctionData = Export.GetData().Get<FAnimNextGraphFunctionOutlinerData>();
		if (FunctionData.SoftEditorObject.IsValid())
		{
			URigVMEdGraph* EdGraph = FunctionData.SoftEditorObject.Get();
			if (EdGraph->bAllowRenaming)
			{
				if (const UEdGraphSchema* GraphSchema = EdGraph->GetSchema())
				{
					FGraphDisplayInfo DisplayInfo;
					GraphSchema->GetGraphDisplayInformation(*EdGraph, DisplayInfo);

					// Check if the name is unchanged
					if (InName.EqualTo(DisplayInfo.PlainName))
					{
						return;
					}

					FScopedTransaction Transaction(LOCTEXT("RenameFunctionInOutliner", "Rename Function"));
					FRigVMControllerCompileBracketScope CompileScope(EdGraph->GetController());
					if (GraphSchema->TryRenameGraph(EdGraph, *InName.ToString()))
					{
						return;
					}
				}
			}
		}
	}
}

bool FAnimNextFunctionItemDetails::ValidateName(const FWorkspaceOutlinerItemExport& Export, const FText& InName, FText& OutErrorMessage) const
{
	const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = Export.GetData();
	if (Data.IsValid() && Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
	{
		return true;
	}

	OutErrorMessage = LOCTEXT("UnsupportedTypeRenameError", "Element type is not supported for rename");
	return false;
}

UPackage* FAnimNextFunctionItemDetails::GetPackage(const FWorkspaceOutlinerItemExport& Export) const
{
	const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = Export.GetData();
	if (Data.IsValid() && Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
	{
		const FAnimNextGraphFunctionOutlinerData& FunctionData = Data.Get<FAnimNextGraphFunctionOutlinerData>();
		if (FunctionData.SoftEditorObject.IsValid())
		{
			return FunctionData.SoftEditorObject->GetPackage();
		}
	}
	return nullptr;
}

const FSlateBrush* FAnimNextFunctionItemDetails::GetItemIcon(const FWorkspaceOutlinerItemExport& Export) const
{
	return FAppStyle::GetBrush(TEXT("GraphEditor.Function_24x"));
}

void FAnimNextFunctionItemDetails::RegisterToolMenuExtensions()
{
}

void FAnimNextFunctionItemDetails::UnregisterToolMenuExtensions()
{
}

} // UE::AnimNext::Editor

#undef LOCTEXT_NAMESPACE // "FAnimNextFunctionItemDetails"
