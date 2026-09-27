// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeParameter.h"

#include "CustomizableObjectNodeEnumParameter.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObjectNodeRemapPins;
class UObject;
struct FPropertyChangedEvent;


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectNodeEnumValue
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	FString Name;

	UPROPERTY(EditAnywhere, Category = UI, meta = (DisplayName = "Parameter UI Metadata"))
	FMutableParamUIMetadata ParamUIMetadata;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeEnumParameter : public UCustomizableObjectNodeParameter
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	int32 DefaultIndex = 0;

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TArray<FCustomizableObjectNodeEnumValue> Values;


	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// Begin EdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	// CustomizableObjectNodeParameter interface
	virtual FName GetCategory() const override;

};

