// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserParameterBool.h"
#include "IChooserParameterFloat.h"
#include "IChooserParameterEnum.h"
#include "PoseSearch/Chooser/ChooserParameterPoseHistoryBase.h"
#include "PoseHistoryChooserParameter.generated.h"

USTRUCT(DisplayName = "Pose History Anim Param")
struct FPoseHistoryAnimProperty :  public FChooserParameterPoseHistoryBase
{
	GENERATED_BODY()
public:

	UPROPERTY(DisplayName = "Variable", EditAnywhere, Category="Variable")
	FName VariableName;

	bool GetValue(FChooserEvaluationContext& Context, FPoseHistoryReference& OutResult) const override;
	virtual bool IsBound() const { return !VariableName.IsNone(); }
};