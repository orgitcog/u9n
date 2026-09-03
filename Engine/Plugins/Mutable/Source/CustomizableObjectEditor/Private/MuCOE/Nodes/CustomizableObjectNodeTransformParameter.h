// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeParameter.h"

#include "CustomizableObjectNodeTransformParameter.generated.h"

namespace ENodeTitleType { enum Type : int; }


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTransformParameter : public UCustomizableObjectNodeParameter
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category=CustomizableObject, meta = (DontUpdateWhileEditing))
	FTransform DefaultValue = FTransform::Identity;

	// Begin EdGraphNode interface
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	// CustomizableObjectNodeParameter interface
	virtual FName GetCategory() const override;
	
};
