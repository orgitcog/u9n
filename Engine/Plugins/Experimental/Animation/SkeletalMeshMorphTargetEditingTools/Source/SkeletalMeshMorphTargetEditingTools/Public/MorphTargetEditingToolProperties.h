// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "InteractiveTool.h"

#include "MorphTargetEditingToolProperties.generated.h"

UENUM()
enum class EMorphTargetEditorOperation
{
	// Add new MorphTarget, select a morph target before activating the tool to edit
	New,
	// Edit existing MorphTarget, deselect morph targets before activating the tool to create a new morph target
	Edit, 
};

UCLASS()
class UMorphTargetEditingToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	// Select a morph target before activating the tool to edit, if nothing is selected, a new morph target is created
	UPROPERTY(VisibleAnywhere, Category = "MorphTarget")
	EMorphTargetEditorOperation Operation;

	UPROPERTY(EditAnywhere, Category = "MorphTarget", meta = (DisplayName = "Name", EditCondition = "Operation == EMorphTargetEditorOperation::New", EditConditionHides))
	FName NewMorphTargetName;
	
	UPROPERTY(VisibleAnywhere, Category = "MorphTarget", meta = (GetOptions = GetMorphTargetNames, NoResetToDefault, DisplayName = "Name", EditCondition = "Operation == EMorphTargetEditorOperation::Edit", EditConditionHides))
	FName EditMorphTargetName;

	UPROPERTY(EditAnywhere, Category = "MorphTarget", meta = (DisplayName = "Weight", UIMin = "0.0", UIMax = "1.0"))
	float MorphTargetWeight = 1.f;

	UFUNCTION()
	TArray<FName> GetMorphTargetNames();

	UPROPERTY()
	TArray<FName> MorphTargetNames;

	FName GetEditingMorphTargetName();

	
};