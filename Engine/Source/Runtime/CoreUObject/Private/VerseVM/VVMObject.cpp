// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMObject.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMDebuggerVisitor.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VObject);
DEFINE_TRIVIAL_VISIT_REFERENCES(VObject);

FOpResult VObject::LoadField(FAllocationContext Context, VEmergentType& EmergentType, const VShape::VEntry* Field, FCacheCase* OutCacheCase)
{
	V_DIE_IF(Field == nullptr);

	const VCppClassInfo& CppClassInfo = *EmergentType.CppClassInfo;

	switch (Field->Type)
	{
		case EFieldType::Offset:
		{
			VRestValue* Value = &GetFieldData(CppClassInfo)[Field->Index];
			if (OutCacheCase)
			{
				uint64 Offset = BitCast<char*>(Value) - BitCast<char*>(this);
				*OutCacheCase = FCacheCase::Offset(&EmergentType, Offset);
			}
			V_RETURN(Value->Get(Context));
		}
		case EFieldType::FProperty:
			return VNativeRef::Get(Context, GetData(CppClassInfo), Field->UProperty);
		case EFieldType::FPropertyVar:
			V_RETURN(VNativeRef::New(Context, this->DynamicCast<VNativeStruct>(), Field->UProperty));
		case EFieldType::FVerseProperty:
			V_RETURN(Field->UProperty->ContainerPtrToValuePtr<VRestValue>(GetData(CppClassInfo))->Get(Context));
		case EFieldType::Constant:
		{
			VValue FieldValue = Field->Value.Get();
			V_DIE_IF(FieldValue.IsCellOfType<VProcedure>());
			if (VFunction* Function = FieldValue.DynamicCast<VFunction>(); Function && !Function->HasSelf())
			{
				if (OutCacheCase)
				{
					*OutCacheCase = FCacheCase::Function(&EmergentType, Function);
				}
				// NOTE: (yiliang.siew) Update the function-without-`Self` to point to the current object instance.
				// We only do this if the function doesn't already have a `Self` bound - in the case of fields that
				// are pointing to functions, we don't want to overwrite that `Self` which was already previously-bound.
				V_RETURN(Function->Bind(Context, *this));
			}
			if (VNativeFunction* NativeFunction = FieldValue.DynamicCast<VNativeFunction>(); NativeFunction && !NativeFunction->HasSelf())
			{
				if (OutCacheCase)
				{
					*OutCacheCase = FCacheCase::NativeFunction(&EmergentType, NativeFunction);
				}
				V_RETURN(NativeFunction->Bind(Context, *this));
			}
			if (OutCacheCase)
			{
				*OutCacheCase = FCacheCase::Constant(&EmergentType, FieldValue);
			}
			V_RETURN(FieldValue);
		}
		default:
			VERSE_UNREACHABLE();
			break;
	}
}

void VObject::VisitMembersImpl(FAllocationContext Context, FDebuggerVisitor& Visitor)
{
	Visitor.VisitObject([this, Context, &Visitor] {
		VEmergentType* EmergentType = GetEmergentType();
		for (auto It = EmergentType->Shape->CreateFieldsIterator(); It; ++It)
		{
			Visitor.Visit(LoadField(Context, *EmergentType, &It->Value), It->Key->AsStringView());
		}
	});
}
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
