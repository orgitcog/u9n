// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FRigVMNewEditor;

struct RIGVMEDITOR_API FRigVMDetailsInspectorTabSummoner : public FWorkflowTabFactory
{
public:

	static const FName TabID() { static FName ID = TEXT("RigVM Details"); return ID; }

public:
	FRigVMDetailsInspectorTabSummoner(const TSharedRef<FRigVMNewEditor>& InRigVMEditor);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedRef<SDockTab> SpawnTab(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<FRigVMNewEditor> RigVMEditor;
};
