// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/VVMValueObject.h"

namespace Verse
{

inline VValueObject& VValueObject::NewUninitialized(FAllocationContext Context, VEmergentType& InEmergentType)
{
	return *new (AllocateCell(Context, *InEmergentType.CppClassInfo, InEmergentType.Shape->NumIndexedFields)) VValueObject(Context, InEmergentType);
}

inline std::byte* VValueObject::AllocateCell(FAllocationContext Context, VCppClassInfo& CppClassInfo, uint64 NumIndexedFields)
{
	return Context.AllocateFastCell(DataOffset(CppClassInfo) + NumIndexedFields * sizeof(VRestValue));
}

inline VValueObject::VValueObject(FAllocationContext Context, VEmergentType& InEmergentType)
	: VObject(Context, InEmergentType)
{
	// We only need to allocate space for indexed fields since we are raising constants to the shape
	// and not storing their data on per-object instances.
	VRestValue* Data = GetFieldData(*InEmergentType.CppClassInfo);
	const uint64 NumIndexedFields = InEmergentType.Shape->NumIndexedFields;
	for (uint64 Index = 0; Index < NumIndexedFields; ++Index)
	{
		// NOTE: (yiliang.siew) We are allocating this uninitialized since we need to rely on the invariant of
		// checking "is this field uninitialized" rather than "is this field a root" when determining the shape
		// of the object.
		new (&Data[Index]) VRestValue();
	}
}

inline bool VValueObject::CreateField(const VUniqueString& Name)
{
	// TODO: (yiliang.siew) In the future, when the emergent type cache is not limited to the class, this will also need
	// to consider the type of the field, not just the name of the field itself, because a field being checked if it's
	// in the shape versus in the object should not be considered the same field when checking if it's already been
	// initialized.
	const VEmergentType* EmergentType = GetEmergentType();
	V_DIE_UNLESS(EmergentType);
	const VShape* Shape = EmergentType->Shape.Get();
	V_DIE_UNLESS(Shape);
	const VShape::VEntry* Field = Shape->GetField(Name);
	// TODO: (yiliang.siew) We shouldn't be able to hit this today, but in the future when we allow adding fields dynamically
	// to objects, we should just return `false` if we don't have the field yet.
	V_DIE_UNLESS(Field);
	V_DIE_IF_MSG(Field->IsProperty(), "`VValueObject::CreateField` was called for a native property: %s! This should be done through `VNativeConstructorWrapper::CreateField` instead!", *Name.AsString());
	if (Field->Type == EFieldType::Constant) // Field data lives in the shape, so we shouldn't bother running any initialization code here.
	{
		V_DIE_IF(Field->Value.Get().IsUninitialized());
		return false;
	}
	else if (Field->Type == EFieldType::Offset) // Field data lives in the object
	{
		// For a `VValueObject` this should have been an uninitialized `VRestValue` when the object was first created.
		VRestValue& Datum = GetFieldData(*EmergentType->CppClassInfo)[Field->Index];
		if (Datum.IsUninitialized())
		{
			Datum.Reset(0); // Reset to be a root placeholder so that it can be unified with now that the field is created.
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		V_DIE("%s has an unsupported field type!", *Name.AsString());
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
