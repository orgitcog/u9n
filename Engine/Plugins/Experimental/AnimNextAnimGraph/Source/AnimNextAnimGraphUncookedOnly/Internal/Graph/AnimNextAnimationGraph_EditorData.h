// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextController.h"
#include "DataInterface/AnimNextDataInterface_EditorData.h"
#include "AnimNextExecuteContext.h"

#include "AnimNextAnimationGraph_EditorData.generated.h"

class FAnimationAnimNextRuntimeTest_GraphAddTrait;
class FAnimationAnimNextRuntimeTest_GraphExecute;
class FAnimationAnimNextRuntimeTest_GraphExecuteLatent;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
	struct FAnimGraphUtils;
}

namespace UE::AnimNext::Editor
{
	class FModuleEditor;
	class SAnimNextGraphView;
	struct FUtils;
}

// Script-callable editor API hoisted onto UAnimNextAnimationGraph
UCLASS()
class UAnimNextAnimationGraphLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds an animation graph to an AnimNext asset */
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Entries", meta=(ScriptMethod))
	static ANIMNEXTANIMGRAPHUNCOOKEDONLY_API UAnimNextAnimationGraphEntry* AddAnimationGraph(UAnimNextAnimationGraph* InAsset, FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
};

/** Editor data for AnimNext animation graphs */
UCLASS()
class ANIMNEXTANIMGRAPHUNCOOKEDONLY_API UAnimNextAnimationGraph_EditorData : public UAnimNextDataInterface_EditorData
{
	GENERATED_BODY()

	friend class UAnimNextAnimationGraphFactory;
	friend class UAnimNextEdGraph;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::Editor::FUtils;
	friend class UE::AnimNext::Editor::FModuleEditor;
	friend class UE::AnimNext::Editor::SAnimNextGraphView;
	friend struct UE::AnimNext::UncookedOnly::FAnimGraphUtils;
	friend struct FAnimNextGraphSchemaAction_RigUnit;
	friend struct FAnimNextGraphSchemaAction_DispatchFactory;
	friend class FAnimationAnimNextEditorTest_GraphAddTrait;
	friend class FAnimationAnimNextEditorTest_GraphTraitOperations;
	friend class FAnimationAnimNextRuntimeTest_GraphExecute;
	friend class FAnimationAnimNextRuntimeTest_GraphExecuteLatent;
	friend class FAnimationAnimNextEditorTest_GraphManifest;

public:
	/** Adds an animation graph to this asset */
	UAnimNextAnimationGraphEntry* AddAnimationGraph(FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

protected:
	// UAnimNextRigVMAssetEditorData interface
	virtual TSubclassOf<URigVMController> GetControllerClass() const override { return UAnimNextController::StaticClass(); }
	virtual UScriptStruct* GetExecuteContextStruct() const override { return FAnimNextExecuteContext::StaticStruct(); }
	virtual TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> GetEntryClasses() const override;
	virtual bool CanAddNewEntry(TSubclassOf<UAnimNextRigVMAssetEntry> InClass) const override;
	virtual TSubclassOf<UAssetUserData> GetAssetUserDataClass() const override;
	virtual void InitializeAssetUserData() override;
	virtual void OnPreCompileAsset(FRigVMCompileSettings& InSettings) override;
	virtual void OnPreCompileGetProgrammaticFunctionHeaders(const FRigVMCompileSettings& InSettings, FAnimNextGetFunctionHeaderCompileContext& OutCompileContext) override;
	virtual void OnPreCompileGetProgrammaticVariables(const FRigVMCompileSettings& InSettings, FAnimNextGetVariableCompileContext& OutCompileContext) override;
	virtual void OnPreCompileGetProgrammaticGraphs(const FRigVMCompileSettings& InSettings, FAnimNextGetGraphCompileContext& OutCompileContext) override;
	virtual void OnPreCompileProcessGraphs(const FRigVMCompileSettings& InSettings, FAnimNextProcessGraphCompileContext& OutCompileContext) override;
	virtual void OnPostCompileCleanup(const FRigVMCompileSettings& InSettings) override;

	// Super overrides
	virtual void GetAnimNextAssetRegistryTags(FAssetRegistryTagsContext& Context, FAnimNextAssetRegistryExports& OutExports) const override;
};
