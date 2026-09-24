// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <IHasContext.h>

#include "CoreMinimal.h"
#include "IChooserParameterBase.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "ChooserParameterPoseHistoryBase.generated.h"

USTRUCT()
struct FChooserParameterPoseHistoryBase : public FChooserParameterBase
{
	GENERATED_BODY()
	virtual bool GetValue(FChooserEvaluationContext& Context, FPoseHistoryReference& OutResult) const { return false; }
	virtual bool IsBound() const { return false; }
};