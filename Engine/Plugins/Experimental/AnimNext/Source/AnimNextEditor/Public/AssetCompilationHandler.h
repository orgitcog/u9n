// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetCompilationHandler.h"

namespace UE::AnimNext::Editor
{

// Asset compiler that can compile assets based on UAnimNextRigVMAsset
class ANIMNEXTEDITOR_API FAssetCompilationHandler : public IAssetCompilationHandler
{
public:
	explicit FAssetCompilationHandler(UObject* InAsset);

protected:
	// IAssetCompilationHandler interface
	virtual void Compile(TSharedRef<Workspace::IWorkspaceEditor> InWorkspaceEditor, UObject* InAsset) override;
	virtual void SetAutoCompile(TSharedRef<Workspace::IWorkspaceEditor> InWorkspaceEditor, UObject* InAsset, bool bInAutoCompile) override;
	virtual bool GetAutoCompile(TSharedRef<Workspace::IWorkspaceEditor> InWorkspaceEditor, const UObject* InAsset) const override;
	virtual ECompileStatus GetCompileStatus(TSharedRef<Workspace::IWorkspaceEditor> InWorkspaceEditor, const UObject* InAsset) const override;

	int32 NumErrors = 0;
	int32 NumWarnings = 0;
};

}
