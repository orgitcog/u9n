// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"

#include "CustomizableObjectNodeModifierClipWithMesh.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObject;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FGuid;

DECLARE_MULTICAST_DELEGATE_OneParam(OnTransformChanged, const FTransform&);

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeModifierClipWithMesh : public UCustomizableObjectNodeModifierBase
{
	GENERATED_BODY()

public:

	UPROPERTY()
	TArray<FString> Tags_DEPRECATED;

	//!< If assigned, then a material inside this CO will be clipped by this node.
    //!< If several materials with the same name, all are considered (to cover all LOD levels)
    UPROPERTY()
	TObjectPtr<UCustomizableObject> CustomizableObjectToClipWith_DEPRECATED = nullptr;

    //!< Array with the Guids of the nodes with the same material inside the CustomizableObjectToClipWith CO (if any is assigned)
    UPROPERTY()
    TArray<FGuid> ArrayMaterialNodeToClipWithID_DEPRECATED;

	/** Transform to apply to the clip mesh before clipping. */
	UPROPERTY(EditAnywhere, Category = ClipMesh)
	FTransform Transform;

	UPROPERTY(EditAnywhere, Category = ClipMesh)
	EFaceCullStrategy FaceCullStrategy = EFaceCullStrategy::AllVerticesCulled;

	/**
	 * Delegate invoked each time the BoundingMeshTransform value gets modified
	 */
	OnTransformChanged TransformChangedDelegate;
	
public:

	UCustomizableObjectNodeModifierClipWithMesh();

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;

	// Own interface
	virtual UEdGraphPin* GetOutputPin() const override;

	UEdGraphPin* GetClipMeshPin() const;

};

