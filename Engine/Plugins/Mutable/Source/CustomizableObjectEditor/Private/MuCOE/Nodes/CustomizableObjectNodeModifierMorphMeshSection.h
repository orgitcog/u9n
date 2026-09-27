// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeModifierEditMeshSectionBase.h"

#include "CustomizableObjectNodeModifierMorphMeshSection.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeModifierMorphMeshSection: public UCustomizableObjectNodeModifierEditMeshSectionBase
{

	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, Category = MeshMorph)
	FString MorphTargetName;

public:

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual FString GetRefreshMessage() const override;
	virtual bool IsSingleOutputNode() const override;
	virtual bool CreateStaticStringPinWidget() const override;
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	UEdGraphPin* FactorPin() const
	{
		return FindPin(TEXT("Factor"));
	}

	UEdGraphPin* MorphTargetNamePin() const;

private:

	UPROPERTY()
	FEdGraphPinReference MorphTargetNamePinRef;

};

