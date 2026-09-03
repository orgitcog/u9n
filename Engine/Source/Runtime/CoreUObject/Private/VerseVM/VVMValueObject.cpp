// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMValueObject.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/Inline/VVMValueObjectInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VValueObject);
TGlobalTrivialEmergentTypePtr<&VValueObject::StaticCppClassInfo> VValueObject::GlobalTrivialEmergentType;

template <typename TVisitor>
void VValueObject::VisitReferencesImpl(TVisitor& Visitor)
{
	const VEmergentType* EmergentType = GetEmergentType();
	Visitor.Visit(GetFieldData(*EmergentType->CppClassInfo), EmergentType->Shape->NumIndexedFields, TEXT("IndexedFields"));
}

ECompares VValueObject::EqualImpl(FAllocationContext Context, VCell* Other, const TFunction<void(VValue, VValue)>& HandlePlaceholder)
{
	if (!IsStruct())
	{
		return (this == Other) ? ECompares::Eq : ECompares::Neq;
	}

	if (!Other->IsA<VObject>())
	{
		return ECompares::Neq;
	}

	VEmergentType* EmergentType = GetEmergentType();
	const VEmergentType* OtherEmergentType = Other->GetEmergentType();

	if (EmergentType->Type != OtherEmergentType->Type)
	{
		return ECompares::Neq;
	}

	if (EmergentType->Shape->Fields.Num() != OtherEmergentType->Shape->Fields.Num())
	{
		return ECompares::Neq;
	}

	// TODO: Optimize for when objects share emergent type
	VObject& OtherObject = Other->StaticCast<VObject>();
	for (VShape::FieldsMap::TConstIterator It = EmergentType->Shape->Fields; It; ++It)
	{
		FOpResult Field = LoadField(Context, *EmergentType, &It->Value);
		if (!Field.IsReturn())
		{
			V_DIE_UNLESS(Field.IsError());
			return ECompares::RuntimeError;
		}

		FOpResult OtherField = OtherObject.LoadField(Context, *It.Key().Get());
		if (!OtherField.IsReturn())
		{
			V_DIE_UNLESS(Field.IsError());
			return ECompares::RuntimeError;
		}

		if (ECompares Cmp = VValue::Equal(Context, OtherField.Value, Field.Value, HandlePlaceholder); Cmp != ECompares::Eq)
		{
			return Cmp;
		}
	}
	return ECompares::Eq;
}

// TODO: Make this (And all other container TypeHash funcs) handle placeholders appropriately
uint32 VValueObject::GetTypeHashImpl()
{
	if (!IsStruct())
	{
		return PointerHash(this);
	}

	const VEmergentType* EmergentType = GetEmergentType();
	VRestValue* Data = GetFieldData(*EmergentType->CppClassInfo);

	// Hash nominal type
	uint32 Result = PointerHash(EmergentType->Type.Get());
	for (VShape::FieldsMap::TConstIterator It = EmergentType->Shape->Fields; It; ++It)
	{
		// Hash Field Name
		Result = ::HashCombineFast(Result, GetTypeHash(It.Key()));

		// Hash Value
		if (It.Value().Type == EFieldType::Constant)
		{
			Result = ::HashCombineFast(Result, GetTypeHash(It.Value().Value));
		}
		else
		{
			Result = ::HashCombineFast(Result, GetTypeHash(Data[It.Value().Index]));
		}
	}
	return Result;
}

VValue VValueObject::MeltImpl(FAllocationContext Context)
{
	V_DIE_UNLESS(IsStruct());

	VEmergentType& EmergentType = *GetEmergentType();
	VEmergentType& NewEmergentType = EmergentType.GetOrCreateMeltTransition(Context);

	VValueObject& NewObject = NewUninitialized(Context, NewEmergentType);
	NewObject.SetIsStruct();
	if (&EmergentType == &NewEmergentType)
	{
		VRestValue* Data = GetFieldData(*EmergentType.CppClassInfo);
		VRestValue* TargetData = NewObject.GetFieldData(*EmergentType.CppClassInfo);
		uint64 NumIndexedFields = EmergentType.Shape->NumIndexedFields;
		for (uint64 I = 0; I < NumIndexedFields; ++I)
		{
			VValue MeltResult = VValue::Melt(Context, Data[I].Get(Context));
			if (MeltResult.IsPlaceholder())
			{
				return MeltResult;
			}

			TargetData[I].Set(Context, MeltResult);
		}
	}
	else
	{
		for (auto It = EmergentType.Shape->CreateFieldsIterator(); It; ++It)
		{
			FOpResult LoadResult = LoadField(Context, EmergentType, &It->Value);
			V_DIE_UNLESS(LoadResult.IsReturn());

			VValue MeltResult = VValue::Melt(Context, LoadResult.Value);
			if (MeltResult.IsPlaceholder())
			{
				return MeltResult;
			}
			FOpResult Result = NewObject.SetField(Context, *It->Key.Get(), MeltResult);
			V_DIE_UNLESS(Result.IsReturn());
		}
	}

	return VValue(NewObject);
}

FOpResult VValueObject::FreezeImpl(FAllocationContext Context)
{
	V_DIE_UNLESS(IsStruct());

	VEmergentType& EmergentType = *GetEmergentType();
	VValueObject& NewObject = NewUninitialized(Context, EmergentType);
	NewObject.SetIsStruct();

	// Mutable structs have all fields as indexed fields in the object.
	uint64 NumIndexedFields = EmergentType.Shape->NumIndexedFields;
	V_DIE_UNLESS(NumIndexedFields == EmergentType.Shape->GetNumFields());

	VRestValue* Data = GetFieldData(*EmergentType.CppClassInfo);
	VRestValue* TargetData = NewObject.GetFieldData(*EmergentType.CppClassInfo);
	for (uint64 I = 0; I < NumIndexedFields; ++I)
	{
		FOpResult Result = VValue::Freeze(Context, Data[I].Get(Context));
		V_DIE_UNLESS(Result.IsReturn()); // Verse objects should always contain valid data.
		TargetData[I].Set(Context, Result.Value);
	}
	V_RETURN(NewObject);
}

void VValueObject::SerializeLayout(FAllocationContext Context, VValueObject*& This, FStructuredArchiveVisitor& Visitor)
{
	uint32 NumIndexedFields = 0;
	if (!Visitor.IsLoading())
	{
		VEmergentType* EmergentType = This->GetEmergentType();
		NumIndexedFields = EmergentType->Shape->NumIndexedFields;
	}

	Visitor.Visit(NumIndexedFields, TEXT("NumIndexedFields"));
	if (Visitor.IsLoading())
	{
		VEmergentType& TrivialEmergentType = GlobalTrivialEmergentType.Get(Context, true);
		This = new (AllocateCell(Context, StaticCppClassInfo, NumIndexedFields)) VValueObject(Context, TrivialEmergentType);
		for (uint64 Index = 0; Index < NumIndexedFields; ++Index)
		{
			new (This->GetFieldData(StaticCppClassInfo) + Index) VRestValue();
		}
	}
}

void VValueObject::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	VClass* Class = nullptr;
	TArray<TPair<TWriteBarrier<VUniqueString>, TWriteBarrier<VValue>>> Fields;
	if (!Visitor.IsLoading())
	{
		VEmergentType* EmergentType = GetEmergentType();
		Class = &EmergentType->Type->StaticCast<VClass>();
		Fields.Reserve(EmergentType->Shape->NumIndexedFields);
		for (auto It = EmergentType->Shape->CreateFieldsIterator(); It; ++It)
		{
			if (It->Value.Type == EFieldType::Offset)
			{
				TPair<TWriteBarrier<VUniqueString>, TWriteBarrier<VValue>>& Field = Fields.Emplace_GetRef();
				Field.Key.Set(Context, *It->Key);
				Field.Value.Set(Context, GetFieldData(StaticCppClassInfo)[It->Value.Index].Get(Context));
			}
		}
	}

	Visitor.Visit(reinterpret_cast<VCell*&>(Class), TEXT("Class"));
	int32 NumIndexedFields = Fields.Num();
	Visitor.Visit(NumIndexedFields, TEXT("NumIndexedFields"));
	if (Visitor.IsLoading())
	{
		Fields.SetNum(NumIndexedFields);
	}
	Visitor.Visit(Fields.GetData(), Fields.Num(), TEXT("Fields"));
	if (Visitor.IsLoading())
	{
		TArray<VArchetype::VEntry> Entries;
		Entries.Reserve(Fields.Num());
		for (TPair<TWriteBarrier<VUniqueString>, TWriteBarrier<VValue>>& Field : Fields)
		{
			Entries.Add(VArchetype::VEntry::Field(Context, *Field.Key, false, false, false, VValue()));
		}
		VArchetype& Archetype = VArchetype::New(Context, Entries);
		Archetype.Class.Set(Context, *Class);
		Archetype.NextArchetype.Set(Context, Class->GetArchetype());

		VEmergentType& EmergentType = Class->GetOrCreateEmergentTypeForVObject(Context, &StaticCppClassInfo, Archetype);
		SetEmergentType(Context, &EmergentType);
		if (Class->IsStruct())
		{
			SetIsStruct();
		}
		for (TPair<TWriteBarrier<VUniqueString>, TWriteBarrier<VValue>>& Field : Fields)
		{
			bool bCreated = CreateField(*Field.Key);
			V_DIE_UNLESS(bCreated);
			Verse::FOpResult Result = SetField(Context, *Field.Key, Field.Value.Get());
			V_DIE_UNLESS(Result.IsReturn());
		}

		// TODO(FORT-881643): Run the class constructor to initialize new defaulted fields.
		//
		// Classes exported from the same package cannot introduce new fields without also
		// recooking the object, so this only matters for imported classes.
		//
		// However, the class constructor is not guaranteed to be invokable by this point.
		// This guarantee may be simpler to provide for imported classes only.
	}
}
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
