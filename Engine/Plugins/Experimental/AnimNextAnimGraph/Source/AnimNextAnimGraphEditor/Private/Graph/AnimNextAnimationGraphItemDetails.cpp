// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAnimationGraphItemDetails.h"

#include "AnimNextAnimGraphWorkspaceAssetUserData.h"
#include "ClassIconFinder.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "Entries/AnimNextAnimationGraphEntry.h"
#include "InstancedStruct.h"
#include "WorkspaceItemMenuContext.h"
#include "IWorkspaceEditor.h"
#include "ScopedTransaction.h"
#include "RigVMModel/RigVMClient.h"
#include "ToolMenus.h"
#include "Entries/AnimNextDataInterfaceEntry.h"
#include "Graph/AnimNextAnimationGraph_EditorData.h"

#define LOCTEXT_NAMESPACE "FAnimNextAnimationGraphItemDetails"

namespace UE::AnimNext::Editor
{
const FSlateBrush* FAnimNextAnimationGraphItemDetails::GetItemIcon(const FWorkspaceOutlinerItemExport& Export) const
{
	return FAppStyle::GetBrush(TEXT("GraphEditor.EventGraph_24x"));
}

}

#undef LOCTEXT_NAMESPACE // "FAnimNextGraphItemDetails"