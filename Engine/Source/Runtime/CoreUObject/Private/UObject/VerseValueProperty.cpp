// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/VerseValueProperty.h"
#include "UObject/GarbageCollectionSchema.h"
#include "VerseVM/Inline/VVMEnterVMInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMAbstractVisitor.h"
#include "VerseVM/VVMCell.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMStructuredArchiveVisitor.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMValuePrinting.h"
#include "VerseVM/VVMWriteBarrier.h"

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

FVCellProperty::FVCellProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: Super(InOwner, InName, InObjectFlags)
{
}

FVCellProperty::FVCellProperty(FFieldVariant InOwner, const UECodeGen_Private::FVerseValuePropertyParams& Prop)
	: Super(InOwner, Prop)
{
}

FVValueProperty::FVValueProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: Super(InOwner, InName, InObjectFlags)
{
}

FVValueProperty::FVValueProperty(FFieldVariant InOwner, const UECodeGen_Private::FVerseValuePropertyParams& Prop)
	: Super(InOwner, Prop)
{
}

FVRestValueProperty::FVRestValueProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: Super(InOwner, InName, InObjectFlags)
{
}

FVRestValueProperty::FVRestValueProperty(FFieldVariant InOwner, const UECodeGen_Private::FVerseValuePropertyParams& Prop)
	: Super(InOwner, Prop)
{
}

template <typename T>
FString TFVersePropertyBase<T>::GetCPPMacroType(FString& ExtendedTypeText) const
{
	ExtendedTypeText = FString();
	return FString();
}

template <typename T>
bool TFVersePropertyBase<T>::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	check(A);

	if (nullptr == B) // if the comparand is NULL, we just call this no-match
	{
		return false;
	}

	const TCppType* Lhs = reinterpret_cast<const TCppType*>(A);
	const TCppType* Rhs = reinterpret_cast<const TCppType*>(B);
	return *Lhs == *Rhs;
}

template <typename T>
void TFVersePropertyBase<T>::SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const
{
	Verse::FRunningContext Context = Verse::FRunningContextPromise{};
	Verse::FStructuredArchiveVisitor Visitor(Context, Slot);
	Visitor.Visit(*static_cast<TCppType*>(Value), TEXT(""));
}

template <typename T>
void TFVersePropertyBase<T>::ExportText_Internal(FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	check(false);
	return;
}

template <typename T>
const TCHAR* TFVersePropertyBase<T>::ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const
{
	check(false);
	return TEXT("");
}

template <typename T>
bool TFVersePropertyBase<T>::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType/* = EPropertyObjectReferenceType::Strong*/) const
{
	return true;
}

template <typename T>
void TFVersePropertyBase<T>::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
	for (int32 Idx = 0, Num = FProperty::ArrayDim; Idx < Num; ++Idx)
	{
		Schema.Add(UE::GC::DeclareMember(DebugPath, BaseOffset + FProperty::GetOffset_ForGC() + Idx * sizeof(TCppType), UE::GC::EMemberType::VerseValue));
	}
}

void VersePropertyToJSON(FUtf8StringBuilderBase& OutJSON, const FProperty* Property, const void* InValue, const uint32 RecursionDepth)
{
	Verse::FRunningContext Context = Verse::FRunningContextPromise{};
	AutoRTFM::Open([&] {
		Context.EnterVM([&] {
			if (const FVRestValueProperty* VRestValueProp = CastField<FVRestValueProperty>(Property))
			{
				const Verse::VRestValue* RestValue = static_cast<const Verse::VRestValue*>(InValue);
				RestValue->AppendToString(OutJSON, Context, Verse::EValueStringFormat::JSON, RecursionDepth + 1);
			}
			else if (const FVValueProperty* VValueProp = CastField<FVValueProperty>(Property))
			{
				const Verse::TWriteBarrier<Verse::VValue>* Value = static_cast<const Verse::TWriteBarrier<Verse::VValue>*>(InValue);
				Value->Get().AppendToString(OutJSON, Context, Verse::EValueStringFormat::JSON, RecursionDepth + 1);
			}
			else if (const FVCellProperty* VCellProp = CastField<FVCellProperty>(Property))
			{
				const Verse::TWriteBarrier<Verse::VCell>* Cell = static_cast<const Verse::TWriteBarrier<Verse::VCell>*>(InValue);
				Cell->Get()->AppendToString(OutJSON, Context, Verse::EValueStringFormat::JSON, RecursionDepth + 1);
			}
			else
			{
				V_DIE("Could not convert Verse property to string - Unknown property type!");
			}
		});
	});
}

IMPLEMENT_FIELD(FVCellProperty)
IMPLEMENT_FIELD(FVValueProperty)
IMPLEMENT_FIELD(FVRestValueProperty)

#endif // WITH_VERSE_VM