// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MaterialTypes.h"
#include "Misc/TVariant.h"
#include "Misc/NotNull.h"

#include "MetaHumanMaterialPipelineCommon.generated.h"

// Material parameters that can be changed at runtime
//
// A subset of EMaterialParameterType
UENUM()
enum class EMetaHumanRuntimeMaterialParameterType
{
	Toggle,
	Scalar,
	Vector,
	DoubleVector,
	Texture,
	TextureCollection,
	Font,
	RuntimeVirtualTexture,
	SparseVolumeTexture
};

/**
 * Used to determine how to obtain material interface for the given parameter.
 */
UENUM()
enum class EMetaHumanRuntimeMaterialParameterSlotTarget
{
	SlotNames,
	SlotIndices,
};

/**
 * Used to describe material parameter that we can modify on the material
 * obtained from the slot name or index.
 */
USTRUCT()
struct FMetaHumanMaterialParameter
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Material")
	FName InstanceParameterName;

	UPROPERTY(EditAnywhere, Category = "Material")
	EMetaHumanRuntimeMaterialParameterSlotTarget SlotTarget = EMetaHumanRuntimeMaterialParameterSlotTarget::SlotNames;

	UPROPERTY(EditAnywhere, Category = "Material", meta = (EditCondition = "SlotTarget == EMetaHumanRuntimeMaterialParameterSlotTarget::SlotNames"))
	TArray<FName> SlotNames;

	UPROPERTY(EditAnywhere, Category = "Material", meta = (EditCondition = "SlotTarget == EMetaHumanRuntimeMaterialParameterSlotTarget::SlotIndices"))
	TArray<int32> SlotIndices;
	
	UPROPERTY(EditAnywhere, Category = "Material")
	FMaterialParameterInfo MaterialParameter;

	UPROPERTY(EditAnywhere, Category = "Material")
	EMetaHumanRuntimeMaterialParameterType ParameterType = EMetaHumanRuntimeMaterialParameterType::Scalar;

	UPROPERTY(EditAnywhere, Category = "Material")
	TMap<FName, FString> PropertyMetadata;
};

class UMaterialInstanceDynamic;
struct FInstancedPropertyBag;

namespace UE::MetaHuman::MaterialUtils
{
	/**
	 * Updates materials from the given material parameters.
	 * 
	 * @param InMaterialParameters Parameters that will be applied onto the material (doesn't contain the actual data)
	 * @param InMaterialInstanceMapping Materials to updated
	 * @param InAvailableSlots Necessary when parameter specifies the slot index instead of the slot name.
	 * @param InPropertyBag Values for the material parameters.
	 */
	void SetInstanceParameters(
		const TArray<FMetaHumanMaterialParameter>& InMaterialParameters,
		const TMap<FName, TObjectPtr<UMaterialInstanceDynamic>>& InMaterialInstanceMapping,
		const TArray<FName>& InAvailableSlots,
		const struct FInstancedPropertyBag& InPropertyBag);

	/**
	 * Outputs the property bag with parameters that are present on the given material.
	 * 
	 * @param InMaterial Material to get values from
	 * @param InMaterialParameters Parameters to look for
	 * @param InOutPropertyBag Output result containing property name and the material parameter value
	 * @return true if any of the parameters were adeded or already in the bag.
	 */
	bool ParametersToPropertyBag(
		TNotNull<const UMaterialInstanceDynamic*> InMaterial,
		const TArray<FMetaHumanMaterialParameter>& InMaterialParameters,
		FInstancedPropertyBag& InOutPropertyBag);

	/**
	 * Converts the given property name to the parameter type.
	 */
	EMetaHumanRuntimeMaterialParameterType PropertyToParameterType(TNotNull<FProperty*> InProperty);

#if WITH_EDITOR
	/**
	 * Reads metadata from the given property.
	 */
	TMap<FName, FString> CopyMetadataFromProperty(TNotNull<FProperty*> InProperty);
#endif
}