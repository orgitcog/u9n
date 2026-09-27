// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectExtension.h"
#include "UObject/SoftObjectPtr.h"

#include "RigLogicMutableExtension.generated.h"

class UDNAAsset;

/** Used as ExtensionData to represent a DNA Asset in a Customizable Object graph */
USTRUCT()
struct RIGLOGICMUTABLE_API FDNAPinData
{
	GENERATED_BODY()

public:
	FDNAPinData() = default;

	/**
	 * Direct copying is not allowed, because the DNA Asset can only be owned by one struct.
	 *
	 * To make a copy of this, create a new default instance and call CopyFromDNAAsset on it to
	 * copy the DNA Asset into it.
	 */
	FDNAPinData(const FDNAPinData& Source) = delete;
	FDNAPinData& operator=(const FDNAPinData& Source) = delete;

	/** Move constructor transfers ownership of the DNA */
	FDNAPinData(FDNAPinData&& Source);
	FDNAPinData& operator=(FDNAPinData&& Source);

	/**
	 * Assign the DNAAsset member variable to the PinData.
	 *
	 * If SourceAsset is nullptr, DNAAsset will be set to nullptr.
	 */
	void SetDNAAsset(UDNAAsset* SourceAsset);

	const UDNAAsset* GetDNAAsset() const { return DNAAsset; }

	/** The name of the mesh component this DNA will be attached to */
	UPROPERTY()
	FName ComponentName;

private:
	/** Points to a DNA Asset that is owned by this struct */
	UPROPERTY()
	TObjectPtr<UDNAAsset> DNAAsset;
};

template<>
struct TStructOpsTypeTraits<FDNAPinData> : public TStructOpsTypeTraitsBase2<FDNAPinData>
{
	enum
	{
		/** Tell Unreal's reflection system not to generate code that calls the copy constructor */
		WithCopy = false
	};
};

/** An extension for Mutable that allows users to bring RigLogic DNA into their Customizable Objects */
UCLASS(MinimalAPI)
class URigLogicMutableExtension : public UCustomizableObjectExtension
{
	GENERATED_BODY()

public:
	/** UCustomizableObjectExtension interface */
	virtual TArray<FCustomizableObjectPinType> GetPinTypes() const override;
	virtual TArray<FObjectNodeInputPin> GetAdditionalObjectNodePins() const override;
	virtual void OnSkeletalMeshCreated(
		const TArray<FInputPinDataContainer>& InputPinData,
		FName ComponentName,
		USkeletalMesh* SkeletalMesh) const override;

#if WITH_EDITOR
	virtual void MovePrivateReferencesToContainer(FInstancedStruct& Struct, UObject* Container) const;
#endif

	/**
	 * Makes a copy of the Source asset and returns it.
	 *
	 * The copy's Outer will be set to OuterForCopy.
	 */
	static UDNAAsset* CopyDNAAsset(const UDNAAsset* Source, UObject* OuterForCopy);

	RIGLOGICMUTABLE_API static const FName DNAPinType;
	RIGLOGICMUTABLE_API static const FName DNABaseNodePinName;
	RIGLOGICMUTABLE_API static const FText DNANodeCategory;
};
