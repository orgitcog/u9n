// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextAssetWorkspaceAssetUserData.h"
#include "Entries/AnimNextVariableEntry.h"
#include "Graph/AnimNextAnimationGraph.h"

#include "AnimNextAnimGraphWorkspaceAssetUserData.generated.h"

USTRUCT()
struct FAnimNextAnimationGraphOutlinerData : public FAnimNextRigVMAssetOutlinerData
{
	GENERATED_BODY()

	FAnimNextAnimationGraphOutlinerData() = default;

	UAnimNextAnimationGraph* GetAnimationGraph() const
	{
		return Cast<UAnimNextAnimationGraph>(GetAsset());
	}
};

UCLASS()
class UAnimNextAnimGraphWorkspaceAssetUserData : public UAssetUserData
{
public:
	virtual bool IsEditorOnly() const override { return true; }

private:
	GENERATED_BODY()

	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
};
