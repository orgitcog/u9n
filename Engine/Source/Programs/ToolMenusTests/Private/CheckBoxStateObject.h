// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateTypes.h"
#include "ToolMenuContext.h"

#include "CheckBoxStateObject.generated.h"

UCLASS()
class UCheckBoxStateObject : public UObject
{
	GENERATED_BODY()

public:
	UCheckBoxStateObject() = default;

	void SetStateToReturn(const ECheckBoxState InState)
	{
		StateToReturn = InState;
	}

	UFUNCTION()
	ECheckBoxState GetActionCheckState(const FToolMenuContext& InContext) const
	{
		return StateToReturn;
	}

private:
	ECheckBoxState StateToReturn;
};
