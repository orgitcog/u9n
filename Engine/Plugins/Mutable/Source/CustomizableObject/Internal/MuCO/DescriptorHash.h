// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectInstanceDescriptor.h"

#define UE_API CUSTOMIZABLEOBJECT_API


/** Hash of the Descriptor.
* Can change and is not backwards compatible. Do not serialize. */
class FDescriptorHash
{
public:
	FDescriptorHash() = default;

	UE_API explicit FDescriptorHash(const FCustomizableObjectInstanceDescriptor& Descriptor);
	
	/** Return true if this Hash is a subset of the other Hash (i.e., this Descriptor is a subset of the other Descriptor). */
	UE_API bool IsSubset(const FDescriptorHash& Other) const;

	UE_API FString ToString() const;

private:
	uint32 Hash = 0;

public:
	// MinLOD based on user input and quality settings. First LOD to generate.
	TMap<FName, uint8> MinLODs;

	// MinLOD based on quality settings. Used to trigger updates after changing the active quality level.
	TMap<FName, uint8> QualitySettingMinLODs;
	
	// Array of bitmasks that indicate which LODs of each component have been requested
	TMap<FName, uint8> FirstRequestedLOD;
};

#undef UE_API
