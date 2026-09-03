// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/Accessors/PCGPropertyAccessor.h"

#include "UObject/EnumProperty.h"

TArray<const void*> PCGPropertyAccessor::GetContainerKeys(int32 Index, int32 Range, const IPCGAttributeAccessorKeys& Keys)
{
	TArray<const void*> ContainerKeys;
	ContainerKeys.SetNumUninitialized(Range);
	TArrayView<const void*> ContainerKeysView(ContainerKeys);
	if (!Keys.GetKeys(Index, ContainerKeysView))
	{
		return TArray<const void*>{};
	}

	return ContainerKeys;
}

TArray<void*> PCGPropertyAccessor::GetContainerKeys(int32 Index, int32 Range, IPCGAttributeAccessorKeys& Keys)
{
	TArray<void*> ContainerKeys;
	ContainerKeys.SetNumUninitialized(Range);
	TArrayView<void*> ContainerKeysView(ContainerKeys);
	if (!Keys.GetKeys(Index, ContainerKeysView))
	{
		return TArray<void*>{};
	}

	return ContainerKeys;
}

IPCGPropertyChain::IPCGPropertyChain(const FProperty* Property, TArray<const FProperty*>&& ExtraProperties)
	: PropertyChain(std::move(ExtraProperties))
{
	// Fix property chain
	if (PropertyChain.IsEmpty() || PropertyChain.Last() != Property)
	{
		PropertyChain.Add(Property);
	}
}

const UStruct* IPCGPropertyChain::GetTopPropertyStruct() const
{
	if (!PropertyChain.IsEmpty() && PropertyChain[0])
	{
		return PropertyChain[0]->GetOwnerStruct();
	}
	else
	{
		return nullptr;
	}
}

bool FPCGEnumPropertyAccessor::GetRangeImpl(TArrayView<int64> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
{
	return PCGPropertyAccessor::IterateGet(GetPropertyChain(), OutValues, Index, Keys, [this](const void* PropertyAddressData) -> Type
		{
			return Property->GetUnderlyingProperty()->GetSignedIntPropertyValue(PropertyAddressData);
		});
}

bool FPCGEnumPropertyAccessor::SetRangeImpl(TArrayView<const int64> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags)
{
	return PCGPropertyAccessor::IterateSet(GetPropertyChain(), InValues, Index, Keys, [this](void* PropertyAddressData, const int64& Value) -> void
		{
			Property->GetUnderlyingProperty()->SetIntPropertyValue(PropertyAddressData, Value);
		});
}