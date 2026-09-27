// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkspaceSchema.h"
#include "AnimNextWorkspaceSchema.generated.h"

// Workspace schema allowing all asset types
UCLASS()
class UAnimNextWorkspaceSchema : public UWorkspaceSchema
{
	GENERATED_BODY()

	// UWorkspaceSchema interface
	virtual FText GetDisplayName() const override;
	virtual TConstArrayView<FTopLevelAssetPath> GetSupportedAssetClassPaths() const override;
	virtual void OnSaveWorkspaceState(TSharedRef<UE::Workspace::IWorkspaceEditor> InWorkspaceEditor, FInstancedStruct& OutWorkspaceState) const override;
	virtual void OnLoadWorkspaceState(TSharedRef<UE::Workspace::IWorkspaceEditor> InWorkspaceEditor, const FInstancedStruct& InWorkspaceState) const override;
};