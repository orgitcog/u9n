// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "UObject/UnrealType.h" // For FProperty
#include "UObject/VerseValueProperty.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMNativeConverter.h"
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/VVMNativeRef.h"
#include "VerseVM/VVMNativeStruct.h"
#include "VerseVM/VVMObject.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMUnreachable.h"
#include "VerseVM/VVMVar.h"

namespace Verse
{
template <typename TBinaryFunction>
bool VObject::AllFields(FAllocationContext Context, TBinaryFunction F)
{
	VEmergentType* EmergentType = GetEmergentType();
	for (auto I = EmergentType->Shape->CreateFieldsIterator(); I; ++I)
	{
		if (!::Invoke(F, I->Key->AsStringView(), LoadField(Context, *EmergentType, &I->Value)))
		{
			return false;
		}
	}
	return true;
}

inline FOpResult VObject::LoadField(FAllocationContext Context, const VUniqueString& Name, FCacheCase* OutCacheCase)
{
	VEmergentType* EmergentType = GetEmergentType();
	return LoadField(Context, *EmergentType, EmergentType->Shape->GetField(Name), OutCacheCase);
}

inline FOpResult VObject::SetField(FAllocationContext Context, const VShape& Shape, const VUniqueString& Name, void* Data, VValue Value)
{
	const VShape::VEntry* Field = Shape.GetField(Name);
	V_DIE_IF(Field == nullptr);
	return SetField(Context, *Field, Data, Value);
}

inline FOpResult VObject::SetField(FAllocationContext Context, const VShape::VEntry& Field, void* Data, VValue Value)
{
	switch (Field.Type)
	{
		case EFieldType::Offset:
			BitCast<VRestValue*>(Data)[Field.Index].Set(Context, Value);
			return {FOpResult::Return};
		case EFieldType::FProperty:
			return VNativeRef::Set<false>(Context, nullptr, Data, Field.UProperty, Value);
		case EFieldType::FPropertyVar:
			return VNativeRef::Set<false>(Context, nullptr, Data, Field.UProperty, Value.StaticCast<VVar>().Get(Context));
		case EFieldType::FVerseProperty:
			Field.UProperty->ContainerPtrToValuePtr<VRestValue>(Data)->Set(Context, Value);
			return {FOpResult::Return};
		case EFieldType::Constant:
		default:
			VERSE_UNREACHABLE(); // This shouldn't happen since such field's data should be on the shape, not the object.
			break;
	}
}

inline FOpResult VObject::SetField(FAllocationContext Context, const VUniqueString& Name, VValue Value)
{
	const VEmergentType* EmergentType = GetEmergentType();
	return SetField(Context, *EmergentType->Shape, Name, GetData(*EmergentType->CppClassInfo), Value);
}

} // namespace Verse
#endif // WITH_VERSE_VM
