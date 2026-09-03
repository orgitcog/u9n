// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMNativeStructInline.h"
#include "VerseVM/Inline/VVMVerseClassInline.h"
#include "VerseVM/VVMNativeConstructorWrapper.h"
#include "VerseVM/VVMUniqueString.h"

namespace Verse
{
inline void VNativeConstructorWrapper::MarkFieldAsInitialized(FAllocationContext Context, VUniqueString& FieldName)
{
	// We use `GlobalFalse` here to mean "initialized" since `VMap` requires that the value cannot be an uninitialized `VValue`.
	FieldsInitialized->Add(Context, FieldName, GlobalFalse());

	/*
	  For the case of a native Verse class that has default values:
	  ```
	  c := class { X:int = 5 }
	  O := c{ X := 10}
	  ```

	  If this is a native object, the object might already have a value for the field `X`, because
	  during archetype instantiation, the `UVerseClass` CDO created will get the `5` default value in the entry for its shape,
	  and the object created from the CDO will thus get that `5` value in its data. When we attempt to unify `10` with
	  `X` after that, it would fail since the object already has that value.

	  We don't want to modify the CDO since that should have its default values for other purposes (e.g. in-editor `@editable`s),
	  so here we just reset the value in the object to a placeholder, so that the unify instruction right after will work.
	 */
	if (VNativeStruct* NativeStruct = WrappedObject().DynamicCast<VNativeStruct>())
	{
		VEmergentType* EmergentType = NativeStruct->GetEmergentType();
		if (VShape* Shape = EmergentType->Shape.Get())
		{
			const VShape::VEntry* Field = Shape->GetField(FieldName);
			if (Field->Type == EFieldType::FVerseProperty)
			{
				Field->UProperty->ContainerPtrToValuePtr<VRestValue>(NativeStruct->GetData(*EmergentType->CppClassInfo))->Reset(0);
			}
		}
		else
		{
			V_DIE("Cannot initialize a field of an imported struct");
		}
	}
	else if (UObject* UEObject = WrappedObject().ExtractUObject(); UEObject)
	{
		if (UVerseClass* VerseClass = Cast<UVerseClass>(UEObject->GetClass()))
		{
			const VShape::VEntry* Field = VerseClass->Shape.Get()->GetField(FieldName);
			if (Field->Type == EFieldType::FVerseProperty)
			{
				Field->UProperty->ContainerPtrToValuePtr<VRestValue>(UEObject)->Reset(0);
			}
		}
		else
		{
			V_DIE("Cannot initialize a field of an imported class");
		}
	}
}

inline bool VNativeConstructorWrapper::CreateField(FAllocationContext Context, VUniqueString& FieldName)
{
	VValue Entry = FieldsInitialized->Find(Context, FieldName);
	if (Entry.IsUninitialized())
	{
		MarkFieldAsInitialized(Context, FieldName);
		return true;
	}
	return false;
}

inline VValue VNativeConstructorWrapper::WrappedObject() const
{
	return NativeObject.Get();
}
} // namespace Verse
#endif // WITH_VERSE_VM
