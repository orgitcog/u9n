// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeAnimationPose.generated.h"

namespace ENodeTitleType { enum Type : int; }

class FString;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
class UPoseAsset;
struct FPropertyChangedEvent;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeAnimationPose : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeAnimationPose();

	UPROPERTY(EditAnywhere, Category=NoCategory)
	TObjectPtr<UPoseAsset> PoseAsset;
	
	// Begin EdGraphNode interface
	UEdGraphPin* GetInputMeshPin() const;
	UEdGraphPin* GetTablePosePin() const;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	// End EdGraphNode interface

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;
	
	// Retrieve the pose information from the PoseAsset
	static void StaticRetrievePoseInformation(UPoseAsset* PoseAsset, class USkeletalMesh* RefSkeletalMesh, TArray<FName>& OutArrayBoneName, TArray<FTransform>& OutArrayTransform);
};

