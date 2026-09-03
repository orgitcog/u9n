// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/ContainersFwd.h"

class UStruct;
class FProperty;
class UFunction;
class UEnum;

namespace Verse
{

struct FAllocationContext;
struct VValue;
class CAttributeValue;
class ICustomAttributeHandler;

struct FAttributeElement
{
	void* UeDefinition;
	bool (*InvokeHandler)(ICustomAttributeHandler* Handler, const CAttributeValue& Payload, void* UeDefinition, TArray<FString>& OutErrors);

	template <typename DefinitionType>
	FAttributeElement(DefinitionType* InUeDefinition);

	void Apply(FAllocationContext Context, VValue AttributeValue, TArray<FString>& OutErrors);
};

extern template FAttributeElement::FAttributeElement(UStruct* InUeDefinition);
extern template FAttributeElement::FAttributeElement(FProperty* InUeDefinition);
extern template FAttributeElement::FAttributeElement(UFunction* InUeDefinition);
extern template FAttributeElement::FAttributeElement(UEnum* InUeDefinition);

};     // namespace Verse
#endif // WITH_VERSE_VM
