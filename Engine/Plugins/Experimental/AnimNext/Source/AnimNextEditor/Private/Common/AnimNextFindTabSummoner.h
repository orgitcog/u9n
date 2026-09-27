// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "IMessageLogListing.h"

namespace UE::Workspace
{
	class IWorkspaceEditor;
}	// end namespace UE::Workspace

namespace UE::AnimNext::Editor
{
	class SFindInAnimNextRigVMAsset;
}	// end namespace UE::AnimNext::Editor

namespace UE::AnimNext::Editor
{
struct FAnimNextFindTabSummoner : public FWorkflowTabFactory
{
public:
	FAnimNextFindTabSummoner(TSharedPtr<UE::Workspace::IWorkspaceEditor> InHostingApp);

private:
	// FWorkflowTabFactory interface
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

	// The widget this tab spawner wraps
	TSharedPtr<SFindInAnimNextRigVMAsset> AnimNextFindResultsWidget;
};

} // end namespace UE::AnimNext::Editor
