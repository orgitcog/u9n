// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FMetaHumanIdentityEditorCommands
	: public TCommands<FMetaHumanIdentityEditorCommands>
{
public:

	FMetaHumanIdentityEditorCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> ComponentsFromMesh;
	TSharedPtr<FUICommandInfo> ComponentsFromFootage;

	TSharedPtr<FUICommandInfo> TrackCurrent;
	TSharedPtr<FUICommandInfo> TrackAll;

	TSharedPtr<FUICommandInfo> ActivateMarkersForCurrent;
	TSharedPtr<FUICommandInfo> ActivateMarkersForAll;

	TSharedPtr<FUICommandInfo> ResetTemplateMesh;
	TSharedPtr<FUICommandInfo> IdentitySolve;
	TSharedPtr<FUICommandInfo> MeshToMetaHumanDNAOnly;
	TSharedPtr<FUICommandInfo> ImportDNA;
	TSharedPtr<FUICommandInfo> ExportDNA;
	TSharedPtr<FUICommandInfo> FitTeeth;
	TSharedPtr<FUICommandInfo> PrepareForPerformance;

	TSharedPtr<FUICommandInfo> RigidFitCurrent;
	TSharedPtr<FUICommandInfo> RigidFitAll;

	TSharedPtr<FUICommandInfo> PromoteFrame;
	TSharedPtr<FUICommandInfo> DemoteFrame;

	TSharedPtr<FUICommandInfo> ToggleConformalMesh;
	TSharedPtr<FUICommandInfo> ToggleRig;
	TSharedPtr<FUICommandInfo> ToggleCurrentPose;

	TSharedPtr<FUICommandInfo> TogglePlayback;
	TSharedPtr<FUICommandInfo> ExportTemplateMesh;
};