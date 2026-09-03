// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByName.h"

#include "CustomizableObjectNodeMacroInstance.generated.h"


/** Base class for all Macro Instance Pins. */
UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMacroInstancePinData : public UCustomizableObjectNodePinData
{
	GENERATED_BODY()

public:

	/** Id of the variable associated to a Macro Instance node pin */
	UPROPERTY()
	FGuid VariableId;

};


UCLASS()
class UCustomizableObjectNodeMacroInstanceRemapPins : public UCustomizableObjectNodeRemapPinsByName
{
	GENERATED_BODY()
public:

	// Specific method to decide when two pins are equal
	virtual bool Equal(const UCustomizableObjectNode& Node, const UEdGraphPin& OldPin, const UEdGraphPin& NewPin) const override;

	// Method to use in the RemapPins step of the node reconstruction process
	virtual void RemapPins(const UCustomizableObjectNode& Node, const TArray<UEdGraphPin*>& OldPins, const TArray<UEdGraphPin*>& NewPins, TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap, TArray<UEdGraphPin*>& PinsToOrphan) override;
};



UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMacroInstance : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	/** Macro Library where the macro to instantiate belongs. */
	UPROPERTY(EditAnywhere, Category = MacroInstance)
	TObjectPtr<UCustomizableObjectMacroLibrary> ParentMacroLibrary;

	/** Macro that represent that instantiates the node. */
	UPROPERTY(EditAnywhere, Category = MacroInstance)
	TObjectPtr<UCustomizableObjectMacro> ParentMacro;

public:

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual bool GetCanRenameNode() const override;
	virtual bool CanUserDeleteNode() const override;
	UObject* GetJumpTargetForDoubleClick() const override;
	virtual bool CanJumpToDefinition() const override;
	virtual void JumpToDefinition() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual UCustomizableObjectNodeMacroInstanceRemapPins* CreateRemapPinsDefault() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CreateStaticStringPinWidget() const override;
	virtual bool IsExperimental() const override;

	// Node MacroInstance Interface
	/** Returns the pin of the Macro's input/output node with the same name */
	UEdGraphPin* GetMacroIOPin(ECOMacroIOType IONodeType, const FName& PinName) const;

};