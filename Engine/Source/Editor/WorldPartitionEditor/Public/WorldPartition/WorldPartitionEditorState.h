// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorState/WorldEditorState.h"
#include "WorldPartitionEditorState.generated.h"

UCLASS(MinimalAPI)
class UWorldPartitionEditorState : public UWorldDependantEditorState
{
	GENERATED_UCLASS_BODY()

	//~ Begin UEditorState Interface
	virtual FText GetCategoryText() const override;
private:
	virtual FOperationResult CaptureState() override;
	virtual FOperationResult RestoreState() const override;
	//~ End UEditorState Interface

private:
	UPROPERTY(EditAnywhere, Category = WorldState)
	TArray<FBox> LoadedEditorRegions;

	UPROPERTY(EditAnywhere, Category = WorldState)
	TArray<FName> LoadedEditorLocationVolumes;
};
