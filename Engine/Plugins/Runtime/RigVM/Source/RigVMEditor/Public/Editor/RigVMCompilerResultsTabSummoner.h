// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

#define LOCTEXT_NAMESPACE "RigVMEditor"

//////////////////////////////////////////////////////////////////////////
// FCompilerResultsSummoner

struct RIGVMEDITOR_API FRigVMCompilerResultsTabSummoner : public FWorkflowTabFactory
{

public:

	static const FName TabID() { static FName ID = TEXT("RigVM Compiler Results"); return ID; }
	
	FRigVMCompilerResultsTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override
	{
		return LOCTEXT("CompilerResultsTooltip", "The compiler results tab shows any errors or warnings generated when compiling this Blueprint.");
	}
};

#undef LOCTEXT_NAMESPACE