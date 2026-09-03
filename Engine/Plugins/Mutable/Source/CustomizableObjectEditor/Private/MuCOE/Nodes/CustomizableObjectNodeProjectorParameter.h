// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeParameter.h"

#include "CustomizableObjectNodeProjectorParameter.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeProjectorParameter : public UCustomizableObjectNodeParameter
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=CustomizableObject, meta = (ShowOnlyInnerProperties))
	FCustomizableObjectProjector DefaultValue;

	UPROPERTY(EditAnywhere, Category = CustomizableObject, Meta = (DisplayName = "Projection Angle (degrees)"))
	float ProjectionAngle = 360.0f;

	UPROPERTY()
	uint32 ReferenceSkeletonIndex_DEPRECATED = 0;

	UPROPERTY(EditAnywhere, Category = ProjectorSnapToBone)
	FName ReferenceSkeletonComponent;

	UPROPERTY(EditAnywhere, Category = ProjectorSnapToBone)
	FName ProjectorBone;

	/** Temporary variable where to put the location information for bone combo box selection changes (in FCustomizableObjectNodeProjectorParameterDetails)*/
	FVector BoneComboBoxLocation = FVector::ZeroVector;

	/** Temporary variable where to put the direction information for bone combo box selection changes (in FCustomizableObjectNodeProjectorParameterDetails)*/
	FVector BoneComboBoxForwardDirection = FVector::ZeroVector;

	/** Temporary variable where to put the direction information for bone combo box selection changes (in FCustomizableObjectNodeProjectorParameterDetails)*/
	FVector BoneComboBoxUpDirection = FVector::ZeroVector;

	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	
	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	// CustomizableObjectNodeParameter interface
	virtual FName GetCategory() const override;

	// Own interface

	ECustomizableObjectProjectorType GetProjectorType() const;

	FVector GetProjectorDefaultPosition() const;
	
	void SetProjectorDefaultPosition(const FVector& Position);

	FVector GetProjectorDefaultDirection() const;

	void SetProjectorDefaultDirection(const FVector& Direction);

	FVector GetProjectorDefaultUp() const;

	void SetProjectorDefaultUp(const FVector& Up);

	FVector GetProjectorDefaultScale() const;

	void SetProjectorDefaultScale(const FVector& Scale);

	float GetProjectorDefaultAngle() const;

	void SetProjectorDefaultAngle(float Angle);

private:

	UPROPERTY()
	ECustomizableObjectProjectorType ProjectionType_DEPRECATED;
};

