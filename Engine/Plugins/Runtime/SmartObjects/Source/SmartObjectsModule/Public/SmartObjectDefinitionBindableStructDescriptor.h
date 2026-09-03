// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBindingBindableStructDescriptor.h"
#include "SmartObjectTypes.h"
#include "SmartObjectDefinitionBindableStructDescriptor.generated.h"

/**
 * Descriptor for a struct or class that can be a binding source or target.
 * e.g., Parameters, Slot, Slot Data, etc.
 */
USTRUCT()
struct FSmartObjectDefinitionBindableStructDescriptor : public FPropertyBindingBindableStructDescriptor
{
	GENERATED_BODY()

	FSmartObjectDefinitionBindableStructDescriptor() = default;

#if WITH_EDITOR
	FSmartObjectDefinitionBindableStructDescriptor(const FName& InName, const UStruct* InStruct, const FGuid& InGuid, const FSmartObjectDefinitionDataHandle& DataHandle)
		: FPropertyBindingBindableStructDescriptor(InName, InStruct, InGuid)
		, DataHandle(DataHandle)
	{
	}
#endif // WITH_EDITOR

	/** Handle allowing bindings to locate the structure in the definition that this descriptor represents. */
	UPROPERTY(VisibleAnywhere, Category = "SmartObject")
	FSmartObjectDefinitionDataHandle DataHandle = FSmartObjectDefinitionDataHandle::Invalid;
};
