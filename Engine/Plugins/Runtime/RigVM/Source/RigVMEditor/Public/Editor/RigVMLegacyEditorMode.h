// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_RIGVMLEGACYEDITOR

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "RigVMBlueprint.h"
#include "BlueprintEditorModes.h"

class URigVMBlueprint;
class FRigVMLegacyEditor;

class FRigVMLegacyEditorMode : public FBlueprintEditorApplicationMode
{
public:
	FRigVMLegacyEditorMode(const TSharedRef<FRigVMLegacyEditor>& InRigVMEditor);

	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

protected:
	// Set of spawnable tabs
	FWorkflowAllowedTabSet TabFactories;

private:
	TWeakObjectPtr<URigVMBlueprint> RigVMBlueprintPtr;
};

#endif