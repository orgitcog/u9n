// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/AnimNextAnimationGraph_EditorData.h"

#include "AnimNextStateTree_EditorData.generated.h"

UCLASS()
class ANIMNEXTSTATETREEUNCOOKEDONLY_API UAnimNextStateTree_EditorData : public UAnimNextAnimationGraph_EditorData
{
	GENERATED_BODY()
	
protected:
	virtual TSubclassOf<UAssetUserData> GetAssetUserDataClass() const override;

	friend class UAnimNextStateTreeFactory;

	// IRigVMClientHost interface
	virtual void RecompileVM() override;
	
	// UAnimNextRigVMAssetEditorData interface
	virtual TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> GetEntryClasses() const override;

	// Allows this asset to generate variables to be injected at compilation time, separate method to allow programmatic graphs to use these vars
	virtual void OnPreCompileGetProgrammaticFunctionHeaders(const FRigVMCompileSettings& InSettings, FAnimNextGetFunctionHeaderCompileContext& OutCompileContext) override;

	// Allows this asset to generate graphs to be injected at compilation time
	virtual void OnPreCompileGetProgrammaticGraphs(const FRigVMCompileSettings& InSettings, FAnimNextGetGraphCompileContext& OutCompileContext) override;
};