// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphUncookedOnlyUtils.h"

#include "AnimNextEdGraphNode.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "Graph/RigUnit_AnimNextTraitStack.h"
#include "AnimNextController.h"
#include "TraitCore/TraitRegistry.h"
#include "UncookedOnlyUtils.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "Graph/AnimNextAnimationGraph_EditorData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AnimNextTraitStackUnitNode.h"

namespace UE::AnimNext::UncookedOnly
{

bool FAnimGraphUtils::IsTraitStackNode(const URigVMNode* InModelNode)
{
	if (const URigVMUnitNode* VMNode = Cast<URigVMUnitNode>(InModelNode))
	{
		const UScriptStruct* ScriptStruct = VMNode->GetScriptStruct();
		return ScriptStruct == FRigUnit_AnimNextTraitStack::StaticStruct();
	}

	return false;
}

void FAnimGraphUtils::SetupAnimGraph(const FName EntryName, URigVMController* InController, bool bSetupUndoRedo /*= true*/,  bool bPrintPythonCommand /*= false*/ )
{
	// Clear the graph
	InController->RemoveNodes(InController->GetGraph()->GetNodes(), bSetupUndoRedo, bPrintPythonCommand);

	// Add root node
	URigVMUnitNode* MainEntryPointNode = InController->AddUnitNode(FRigUnit_AnimNextGraphRoot::StaticStruct(), FRigUnit_AnimNextGraphRoot::EventName, FVector2D(-400.0f, 0.0f), FString(), bSetupUndoRedo, bPrintPythonCommand);
	if(MainEntryPointNode == nullptr)
	{
		return;
	}

	URigVMPin* BeginExecutePin = MainEntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphRoot, Result));
	if(BeginExecutePin == nullptr)
	{
		return;
	}

	URigVMPin* EntryPointPin = MainEntryPointNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphRoot, EntryPoint));
	if(EntryPointPin == nullptr)
	{
		return;
	}

	InController->SetPinDefaultValue(EntryPointPin->GetPinPath(), EntryName.ToString(), true, bSetupUndoRedo, true, bPrintPythonCommand);
}

void FAnimGraphUtils::GetAssetManifestNodesRegistryExports(const UAnimNextRigVMAssetEditorData* InEditorData, FAnimNextAssetRegistryExports& OutExports)
{
	if (const UAnimNextAnimationGraph_EditorData* EditorData = Cast<UAnimNextAnimationGraph_EditorData>(InEditorData))
	{
		TArray<UAnimNextEdGraphNode*> AllNodes;
		EditorData->GetAllNodesOfClass(AllNodes);

		OutExports.ManifestNodes.Reset();
		OutExports.ManifestNodes.Reserve(AllNodes.Num());

		for (const UAnimNextEdGraphNode* EdNode : AllNodes)
		{
			if (const URigVMNode* ModelNode = EdNode->GetModelNode())
			{
				if (UE::AnimNext::UncookedOnly::FAnimGraphUtils::IsExposedToManifest(ModelNode))
				{
					const UPackage* Package = EditorData->GetOuter()->GetPackage();
					FString PackageName;
					Package->GetName().Split(FString(TEXT("/")), nullptr, &PackageName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

					OutExports.ManifestNodes.Add(FAnimNextAssetRegistryManifestNode(
						ModelNode->GetGraph(),
						*ModelNode->GetName(),
						PackageName, // for now we set the package as category
						EdNode->GetNodeTitle(ENodeTitleType::MenuTitle).ToString(),
						ModelNode->GetToolTipText().ToString()));
				}
			}
		}
	}
}

bool FAnimGraphUtils::GetExportedManifestNodesFromAssetRegistry(TArray<FAnimNextAssetRegistryExports>& OutExports)
{
	TArray<FAssetData> AssetData;
	IAssetRegistry::GetChecked().GetAssetsByTags({ UE::AnimNext::ExportsAnimNextAssetRegistryTag }, AssetData);

	for (const FAssetData& Asset : AssetData)
	{
		const FString TagValue = Asset.GetTagValueRef<FString>(UE::AnimNext::ExportsAnimNextAssetRegistryTag);
		FAnimNextAssetRegistryExports AssetExports;
		if (FAnimNextAssetRegistryExports::StaticStruct()->ImportText(*TagValue, &AssetExports, nullptr, PPF_None, nullptr, FAnimNextAssetRegistryExports::StaticStruct()->GetName()) != nullptr)
		{
			if (AssetExports.ManifestNodes.Num() > 0)
			{
				OutExports.Add(MoveTemp(AssetExports));
			}
		}
	}

	return OutExports.Num() > 0;
}

bool FAnimGraphUtils::IsExposedToManifest(const URigVMNode* InModelNode)
{
	bool bIsExposedToManifest = false;

	if (InModelNode != nullptr)
	{
		const UAnimNextTraitStackUnitNode* AnimNextUnitNode = Cast<UAnimNextTraitStackUnitNode>(InModelNode);
		if (AnimNextUnitNode != nullptr)
		{
			bIsExposedToManifest = AnimNextUnitNode->IsExposedToManifest();
		}
	}

	return bIsExposedToManifest;
}

bool FAnimGraphUtils::RequestVMAutoRecompile(UAnimNextRigVMAssetEditorData* EditorData)
{
	if (EditorData)
	{
		if (IRigVMClientHost* ClientHost = CastChecked<IRigVMClientHost>(EditorData))
		{
			ClientHost->RequestAutoVMRecompilation();
			return true;
		}
	}
	return false;
}

} // end namespace
