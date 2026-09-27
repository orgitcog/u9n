// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class URigVMNode;
class UAnimNextController;
class URigVMController;
class UAnimNextRigVMAssetEditorData;
struct FAnimNextAssetRegistryExports;

namespace UE::AnimNext::UncookedOnly
{

struct ANIMNEXTANIMGRAPHUNCOOKEDONLY_API FAnimGraphUtils
{
	/** Set up a simple animation graph */
	static void SetupAnimGraph(const FName EntryName, URigVMController* InController, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	/** Check whether the supplied node is a trait stack */
	static bool IsTraitStackNode(const URigVMNode* InModelNode);

	// Gets the exported manifest nodes that are used by a RigVM asset
	static void GetAssetManifestNodesRegistryExports(const UAnimNextRigVMAssetEditorData* InEditorData, FAnimNextAssetRegistryExports& OutExports);

	// Gets all the manifest node defs that are exported to the asset registry
	static bool GetExportedManifestNodesFromAssetRegistry(TArray<FAnimNextAssetRegistryExports>& OutExports);

	// Returns true if the node is exposed to the manifest
	static bool IsExposedToManifest(const URigVMNode* InModelNode);

	static bool RequestVMAutoRecompile(UAnimNextRigVMAssetEditorData* EditorData);
};

}