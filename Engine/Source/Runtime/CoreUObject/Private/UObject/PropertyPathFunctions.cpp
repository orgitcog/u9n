// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyPathFunctions.h"

#include "UObject/Class.h"
#include "UObject/PropertyOptional.h"
#include "UObject/PropertyPathName.h"
#include "UObject/UnrealType.h"

namespace UE
{

const FName NAME_Key(ANSITEXTVIEW("Key"));
const FName NAME_Value(ANSITEXTVIEW("Value"));

FProperty* FindPropertyByNameAndTypeName(const UStruct* Struct, FName Name, FPropertyTypeName TypeName)
{
	for (FProperty* Property = Struct->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (Property->GetFName() == Name && Property->CanSerializeFromTypeName(TypeName))
		{
			return Property;
		}
	}
	return nullptr;
}

inline static const UStruct* FindStructFromProperty(const FProperty* Property)
{
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		return StructProperty->Struct;
	}
	return nullptr;
}

FPropertyValueInContainer TryResolvePropertyPath(const FPropertyPathName& Path, UObject* Object)
{
	const UStruct* NextStruct = Object ? Object->GetClass() : nullptr;
	void* NextContainer = Object;

	FPropertyValueInContainer Value;
	for (int32 Index = 0, Count = Path.GetSegmentCount(); Index < Count; ++Index)
	{
		// Fail if the previous segment failed to resolve the struct or container for this segment.
		if (!NextStruct || !NextContainer)
		{
			return {};
		}

		const FPropertyPathNameSegment Segment = Path.GetSegment(Index);
		const FProperty* Property = FindPropertyByNameAndTypeName(NextStruct, Segment.Name, Segment.Type);

		if (!Property)
		{
			return {};
		}

		Value.Property = Property;
		Value.Struct = NextStruct;
		Value.Container = NextContainer;
		Value.ArrayIndex = 0;

		// Check the bounds and assign the index for static arrays.
		if (Property->ArrayDim > 1)
		{
			if (Segment.Index < 0 || Segment.Index >= Property->ArrayDim)
			{
				return {};
			}
			Value.ArrayIndex = Segment.Index;
		}

		// Resolve the struct and container for the next segment if there is one.
		NextStruct = FindStructFromProperty(Property);
		NextContainer = Property->ContainerPtrToValuePtr<uint8>(NextContainer, Value.ArrayIndex);

		// Resolve optionals to the struct and container of their value if they have one.
		if (const FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(Property))
		{
			FOptionalPropertyLayout OptionalHelper(OptionalProperty->GetValueProperty());
			NextStruct = FindStructFromProperty(OptionalHelper.GetValueProperty());
			NextContainer = OptionalHelper.GetValuePointerForReadOrReplaceIfSet(NextContainer);
		}

		// Scalar values and static containers are finished resolving.
		if (Property->ArrayDim > 1 || Segment.Index == INDEX_NONE)
		{
			continue;
		}

		// Resolve dynamic containers, which have no struct when resolving directly to an element.
		Value.Struct = nullptr;

		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, NextContainer);
			if (!ArrayHelper.IsValidIndex(Segment.Index))
			{
				return {};
			}

			NextStruct = FindStructFromProperty(ArrayProperty->Inner);
			NextContainer = ArrayHelper.GetRawPtr(Segment.Index);
			Value.Property = ArrayProperty->Inner;
			Value.Container = NextContainer;
		}
		else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
		{
			FScriptSetHelper SetHelper(SetProperty, NextContainer);
			if (!SetHelper.IsValidIndex(Segment.Index))
			{
				return {};
			}

			NextStruct = FindStructFromProperty(SetProperty->ElementProp);
			NextContainer = SetHelper.GetElementPtr(Segment.Index);
			Value.Property = SetProperty->ElementProp;
			Value.Container = NextContainer;
		}
		else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
		{
			FScriptMapHelper MapHelper(MapProperty, NextContainer);
			if (!MapHelper.IsValidIndex(Segment.Index) || ++Index == Count)
			{
				return {};
			}

			// A Key or Value segment with no type or index is required to distinguish which property to resolve.
			const FPropertyPathNameSegment MapSegment = Path.GetSegment(Index);
			if (!MapSegment.Type.IsEmpty() || MapSegment.Index != INDEX_NONE)
			{
				return {};
			}

			if (MapSegment.Name == NAME_Key)
			{
				NextStruct = FindStructFromProperty(MapProperty->KeyProp);
				NextContainer = MapHelper.GetKeyPtr(Segment.Index);
				Value.Property = MapProperty->KeyProp;
			}
			else if (MapSegment.Name == NAME_Value)
			{
				NextStruct = FindStructFromProperty(MapProperty->ValueProp);
				NextContainer = MapHelper.GetValuePtr(Segment.Index);
				Value.Property = MapProperty->ValueProp;
			}
			else
			{
				return {};
			}

			// The key and value property both have an offset relative to the pair.
			Value.Container = MapHelper.GetPairPtr(Segment.Index);
		}
		else
		{
			return {};
		}
	}
	return Value;
}

} // UE
