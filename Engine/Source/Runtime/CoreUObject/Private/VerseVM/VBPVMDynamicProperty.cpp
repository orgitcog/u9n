// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VBPVMDynamicProperty.h"

#if WITH_VERSE_BPVM
#include "UObject/GarbageCollectionSchema.h"
#include "VerseVM/VBPVMRuntimeType.h"

IMPLEMENT_FIELD(FVerseDynamicProperty);

FVerseDynamicProperty::FVerseDynamicProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: TProperty(InOwner, InName, InObjectFlags)
{
}

FVerseDynamicProperty::FVerseDynamicProperty(FFieldVariant InOwner, const UECodeGen_Private::FVerseValuePropertyParams& Prop)
	: TProperty(InOwner, reinterpret_cast<const UECodeGen_Private::FPropertyParamsBaseWithOffset&>(Prop))
{
}

FString FVerseDynamicProperty::GetCPPMacroType(FString& ExtendedTypeText) const
{
	return TEXT("FDYNAMICALLYTYPEDVALUE");
}

void FVerseDynamicProperty::InstanceSubobjects(void* Data, void const* DefaultData, TNotNull<UObject*> InOwner, FObjectInstancingGraph* InstanceGraph)
{
	UE::Verse::FRuntimeTypeDynamic::Get().InstanceSubobjects(Data, DefaultData, InOwner, InstanceGraph);
}

bool FVerseDynamicProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	return B && UE::Verse::FRuntimeTypeDynamic::Get().AreIdentical(A, B);
}

void FVerseDynamicProperty::SerializeItem(FStructuredArchive::FSlot Slot, void* InValue, void const* Defaults) const
{
	if (Slot.GetUnderlyingArchive().IsLoading())
	{
		// FRuntimeType::SerializeValue assumes when loading that the value is uninitialized.
		UE::Verse::FRuntimeTypeDynamic::Get().DestroyValue(InValue);
	}
	UE::Verse::FRuntimeTypeDynamic::Get().SerializeValue(Slot, InValue, Defaults);
}

void FVerseDynamicProperty::ExportText_Internal(FString& ValueStr, const void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	const void* Value = PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
	UE::Verse::FRuntimeTypeDynamic::Get().ExportValueToText(ValueStr, Value, DefaultValue, Parent, ExportRootScope);
}

const TCHAR* FVerseDynamicProperty::ImportText_Internal(const TCHAR* InputString, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const
{
	void* Value = PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
	const TCHAR* InputCursor = InputString;
	if (UE::Verse::FRuntimeTypeDynamic::Get().ImportValueFromText(InputCursor, Value, OwnerObject, ErrorText))
	{
		return InputCursor;
	}
	else
	{
		return InputString;
	}
}

bool FVerseDynamicProperty::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType) const
{
	return true;
}

void FVerseDynamicProperty::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
	Schema.Add(UE::GC::DeclareMember(DebugPath, BaseOffset + GetOffset_ForGC(), UE::GC::EMemberType::DynamicallyTypedValue));
}

uint32 FVerseDynamicProperty::GetValueTypeHashInternal(const void* Src) const
{
	return UE::Verse::FRuntimeTypeDynamic::Get().GetValueHash(Src);
}
#endif