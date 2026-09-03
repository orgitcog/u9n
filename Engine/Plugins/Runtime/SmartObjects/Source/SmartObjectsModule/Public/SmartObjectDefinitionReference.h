// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/PropertyBag.h"
#include "SmartObjectDefinitionReference.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

class USmartObjectDefinition;

/**
 * Struct to hold reference to a SmartObjectDefinition asset along with values to parameterized it.
 */
USTRUCT()
struct FSmartObjectDefinitionReference
{
	GENERATED_BODY()

	FSmartObjectDefinitionReference() = default;
	explicit FSmartObjectDefinitionReference(const USmartObjectDefinition* Definition)
		: SmartObjectDefinition(Definition)
	{
	}

	bool operator==(const FSmartObjectDefinitionReference& RHS) const
	{
		return SmartObjectDefinition == RHS.SmartObjectDefinition
			&& Parameters.Identical(&RHS.Parameters, 0)
			&& PropertyOverrides == RHS.PropertyOverrides; 
	}

	bool operator!=(const FSmartObjectDefinitionReference& RHS) const
	{
		return !(*this == RHS);
	}

	/** @return true if the reference is set. */
	bool IsValid() const
	{
		return SmartObjectDefinition != nullptr;
	}

	/**
	 * Returns a variation of the USmartObjectDefinition based on the parameters defined in the reference.
	 * @return Pointer to an asset variation.
	 */
	UE_API USmartObjectDefinition* GetAssetVariation(UWorld* World) const;

	UE_DEPRECATED(5.6, "Use the overload taking a UWorld as parameter.")
	USmartObjectDefinition* GetAssetVariation() const
	{
		return nullptr;
	}

	/** Sets the SmartObject Definition asset and synchronize parameters. */
	void SetSmartObjectDefinition(USmartObjectDefinition* NewSmartObjectDefinition)
	{
		SmartObjectDefinition = NewSmartObjectDefinition;
		SyncParameters();
	}

	/** @return const pointer to the referenced SmartObject Definition asset. */
	const USmartObjectDefinition* GetSmartObjectDefinition() const
	{
		return SmartObjectDefinition;
	}

	/** @return pointer to the referenced SmartObject Definition asset. */
	UE_DEPRECATED(5.6, "Mutable version of the definition should not accessible and will be removed. Use GetSmartObjectDefinition instead.")
	USmartObjectDefinition* GetMutableSmartObjectDefinition()
	{
		return const_cast<USmartObjectDefinition*>(SmartObjectDefinition.Get());
	}

	/** @return reference to the parameters for the referenced SmartObject Definition asset. */
	const FInstancedPropertyBag& GetParameters() const
	{
		ConditionallySyncParameters();
		return Parameters;
	}

	/** @return reference to the parameters for the referenced SmartObject Definition asset. */
	FInstancedPropertyBag& GetMutableParameters()
	{
		ConditionallySyncParameters();
		return Parameters;
	}

	/**
	 * Enforce self parameters to be compatible with those exposed by the selected SmartObject Definition asset.
	 */
	UE_API void SyncParameters();

	/**
	 * Indicates if current parameters are compatible with those available in the selected SmartObject Definition asset.
	 * @return true when parameters requires to be synced to be compatible with those available in the selected SmartObject Definition asset, false otherwise.
	 */
	UE_API bool RequiresParametersSync() const;

	/** Sync parameters to match the asset if required. */
	UE_API void ConditionallySyncParameters() const;

	/** @return true if the property of specified ID is overridden. */
	bool IsPropertyOverridden(const FGuid PropertyID) const
	{
		return PropertyOverrides.Contains(PropertyID);
	}

	/** Sets the override status of specified property by ID. */
	UE_API void SetPropertyOverridden(const FGuid PropertyID, const bool bIsOverridden);

	UE_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

	/** @return a valid hash based on the associated asset path and overriden parameters if the asset is valid; 0 otherwise */
	friend uint32 GetTypeHash(const FSmartObjectDefinitionReference& DefinitionReference);

protected:
	UPROPERTY(EditAnywhere, Category = "")
	TObjectPtr<const USmartObjectDefinition> SmartObjectDefinition = nullptr;

	UPROPERTY(EditAnywhere, Category = "", meta = (FixedLayout))
	FInstancedPropertyBag Parameters;

	/** Array of overridden properties. Non-overridden properties will inherit the values from the SmartObjectDefinition default parameters. */
	UPROPERTY(EditAnywhere, Category = "")
	TArray<FGuid> PropertyOverrides;

	friend class FSmartObjectDefinitionReferenceDetails;
};

template<>
struct TStructOpsTypeTraits<FSmartObjectDefinitionReference> : public TStructOpsTypeTraitsBase2<FSmartObjectDefinitionReference>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};

#undef UE_API
