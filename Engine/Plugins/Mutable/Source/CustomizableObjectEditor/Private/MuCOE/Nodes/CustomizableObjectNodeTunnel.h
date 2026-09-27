// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByName.h"

#include "CustomizableObjectNodeTunnel.generated.h"


/** Base class for all Tunnel Pins. */
UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTunnelPinData : public UCustomizableObjectNodePinData
{
	GENERATED_BODY()

public:

	/** Id of the variable associated to a tunnel node pin */
	UPROPERTY()
	FGuid VariableId;

};


UCLASS()
class UCustomizableObjectNodeTunnelRemapPins : public UCustomizableObjectNodeRemapPinsByName
{
	GENERATED_BODY()
public:

	// Specific method to decide when two pins are equal
	virtual bool Equal(const UCustomizableObjectNode& Node, const UEdGraphPin& OldPin, const UEdGraphPin& NewPin) const override;

	// Method to use in the RemapPins step of the node reconstruction process
	virtual void RemapPins(const UCustomizableObjectNode& Node, const TArray<UEdGraphPin*>& OldPins, const TArray<UEdGraphPin*>& NewPins, TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap, TArray<UEdGraphPin*>& PinsToOrphan) override;
};



UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTunnel : public UCustomizableObjectNode
{
public:

	/** Indicates if this node is an input or output node. */
	UPROPERTY()
	bool bIsInputNode = false;

	/** Pointer to the macro where this node belongs. */
	UPROPERTY()
	TObjectPtr<UCustomizableObjectMacro> ParentMacro;

public:

	GENERATED_BODY()

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual bool GetCanRenameNode() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;


	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual UCustomizableObjectNodeTunnelRemapPins* CreateRemapPinsDefault() const override;
	virtual bool CreateStaticStringPinWidget() const override;

};