// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeObjectGroup.generated.h"

enum class ECustomizableObjectGroupType : uint8;
namespace ENodeTitleType { enum Type : int; }

class FArchive;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeObjectGroup : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeObjectGroup();

	UPROPERTY(EditAnywhere, Category=GroupInfo, meta = (DisplayName = "Type"))
	ECustomizableObjectGroupType GroupType;
	
	UPROPERTY(EditAnywhere, Category=GroupInfo)
	FString DefaultValue;

	UPROPERTY(EditAnywhere, Category = UI, meta = (DisplayName = "Parameter UI Metadata"))
	FMutableParamUIMetadata ParamUIMetadata;

	// The sockets defined in meshes deriving from this node will inherit this socket priority. When in the generated merged mesh there
	// are clashes with socket names, the one with higher priority will be kept and the other discarded.
	UPROPERTY(EditAnywhere, Category = MeshSockets)
	int32 SocketPriority = 0;
	
	UPROPERTY()
	FEdGraphPinReference NamePin;

	// UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual bool GetCanRenameNode() const override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual bool IsSingleOutputNode() const override;
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	// Own interface
	UEdGraphPin* ObjectsPin() const;
	UEdGraphPin* GroupProjectorsPin() const;
	UEdGraphPin* GroupPin() const;
	FString GetGroupName(TArray<const UCustomizableObjectNodeMacroInstance*>* MacroContext = nullptr) const;
	void SetGroupName(const FString& Name);

private:

	UPROPERTY(EditAnywhere, Category = GroupInfo, meta = (DisplayName = "Name"))
	FString GroupName;

	FString LastGroupName;

};

