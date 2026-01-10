// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WorkflowOrientedApp/ApplicationMode.h"

class URigVMBlueprint;
class FRigVMNewEditor;

struct RIGVMEDITOR_API FRigVMNewEditorApplicationModes
{
	// Mode identifiers
	static const FName StandardRigVMEditorMode();
	static const FName RigVMDefaultsMode();
	
private:
	FRigVMNewEditorApplicationModes() {}
};

class RIGVMEDITOR_API FRigVMNewEditorMode : public FApplicationMode
{
public:
	FRigVMNewEditorMode(const TSharedRef<FRigVMNewEditor>& InRigVMEditor); 

	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

	void RegisterToolbarTab(const TSharedRef<class FTabManager>& TabManager);

	virtual void PostActivateMode() override;

protected:
	// Set of spawnable tabs
	FWorkflowAllowedTabSet TabFactories;

	TWeakPtr<FRigVMNewEditor> Editor;
};
