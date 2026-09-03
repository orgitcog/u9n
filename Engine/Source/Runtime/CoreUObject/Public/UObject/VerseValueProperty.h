// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "HAL/Platform.h"
#include "UObject/UnrealType.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMWriteBarrier.h"

template <>
inline const TCHAR* TPropertyTypeFundamentals<Verse::TWriteBarrier<Verse::VCell>>::GetTypeName()
{
	return TEXT("Verse::TWriteBarrier<Verse::VCell>");
}

template <>
inline const TCHAR* TPropertyTypeFundamentals<Verse::TWriteBarrier<Verse::VValue>>::GetTypeName()
{
	return TEXT("Verse::TWriteBarrier<Verse::VValue>");
}

template <typename T>
struct TIsZeroConstructType<Verse::TWriteBarrier<T>>
{
	enum { Value = true };
};

template <>
inline const TCHAR* TPropertyTypeFundamentals<Verse::VRestValue>::GetTypeName()
{
	return TEXT("Verse::VRestValue");
}

template <>
inline Verse::VRestValue TPropertyTypeFundamentals<Verse::VRestValue>::GetDefaultPropertyValue()
{
	return Verse::VRestValue(Verse::EDefaultConstructVRestValue::UnsafeDoNotUse);
}

template <>
inline Verse::VRestValue* TPropertyTypeFundamentals<Verse::VRestValue>::InitializePropertyValue(void* A)
{
	return new (A) Verse::VRestValue(Verse::EDefaultConstructVRestValue::UnsafeDoNotUse);
}

template <>
struct TIsZeroConstructType<Verse::VRestValue>
{
	enum { Value = true };
};

// Template base class for the verse property types
template <typename InTCppType>
class TFVersePropertyBase : public TProperty<InTCppType, FProperty>
{
public:
	using Super = TProperty<InTCppType, FProperty>;
	using TCppType = InTCppType;

	TFVersePropertyBase(EInternal InInternal, FFieldClass* InClass)
		: Super(EC_InternalUseOnlyConstructor, InClass)
	{
	}

	TFVersePropertyBase(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
		: Super(InOwner, InName, InObjectFlags)
	{
	}

	TFVersePropertyBase(FFieldVariant InOwner, const UECodeGen_Private::FVerseValuePropertyParams& Prop)
		: Super(InOwner, reinterpret_cast<const UECodeGen_Private::FPropertyParamsBaseWithOffset&>(Prop))
	{
	}

#if WITH_EDITORONLY_DATA
	explicit TFVersePropertyBase(UField* InField)
		: Super(InField)
	{
	}
#endif // WITH_EDITORONLY_DATA

	// UHT interface
	COREUOBJECT_API virtual FString GetCPPMacroType(FString& ExtendedTypeText) const override;
	// End of UHT interface

	// FProperty interface
	COREUOBJECT_API virtual bool Identical(const void* A, const void* B, uint32 PortFlags) const override;
	COREUOBJECT_API virtual void SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const override;
	COREUOBJECT_API virtual void ExportText_Internal(FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const override;
	COREUOBJECT_API virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const override;
	COREUOBJECT_API virtual bool ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType = EPropertyObjectReferenceType::Strong) const override;
	COREUOBJECT_API virtual void EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath) override;

	virtual bool HasIntrusiveUnsetOptionalState() const override
	{
		return false;
	}
	// End of FProperty interface
};

class FVCellProperty : public TFVersePropertyBase<Verse::TWriteBarrier<Verse::VCell>>
{
	DECLARE_FIELD_API(FVCellProperty, TFVersePropertyBase<Verse::TWriteBarrier<Verse::VCell>>, CASTCLASS_FVCellProperty, COREUOBJECT_API)

public:
	COREUOBJECT_API FVCellProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);

	/**
	 * Constructor used for constructing compiled in properties
	 * @param InOwner Owner of the property
	 * @param Prop Pointer to the compiled in structure describing the property
	 **/
	COREUOBJECT_API FVCellProperty(FFieldVariant InOwner, const UECodeGen_Private::FVerseValuePropertyParams& Prop);
};

//
// Metadata for a property of FVValueProperty type.
//
class FVValueProperty : public TFVersePropertyBase<Verse::TWriteBarrier<Verse::VValue>>
{
	DECLARE_FIELD_API(FVValueProperty, TFVersePropertyBase<Verse::TWriteBarrier<Verse::VValue>>, CASTCLASS_FVValueProperty, COREUOBJECT_API)

public:
	COREUOBJECT_API FVValueProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);

	/**
	 * Constructor used for constructing compiled in properties
	 * @param InOwner Owner of the property
	 * @param Prop Pointer to the compiled in structure describing the property
	 **/
	COREUOBJECT_API FVValueProperty(FFieldVariant InOwner, const UECodeGen_Private::FVerseValuePropertyParams& Prop);
};

//
// Metadata for a property of FVRestValueProperty type.
//
class FVRestValueProperty : public TFVersePropertyBase<Verse::VRestValue>
{
	DECLARE_FIELD_API(FVRestValueProperty, TFVersePropertyBase<Verse::VRestValue>, CASTCLASS_FVRestValueProperty, COREUOBJECT_API)

public:
	COREUOBJECT_API FVRestValueProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);

	/**
	 * Constructor used for constructing compiled in properties
	 * @param InOwner Owner of the property
	 * @param Prop Pointer to the compiled in structure describing the property
	 **/
	COREUOBJECT_API FVRestValueProperty(FFieldVariant InOwner, const UECodeGen_Private::FVerseValuePropertyParams& Prop);
};

COREUOBJECT_API inline bool IsVerseProperty(const FProperty* Property)
{
	return Property->IsA<FVRestValueProperty>() || Property->IsA<FVValueProperty>() || Property->IsA<FVCellProperty>();
}

COREUOBJECT_API void VersePropertyToJSON(FUtf8StringBuilderBase& OutJSON, const FProperty* Property, const void* InValue, const uint32 RecursionDepth = 0);

#endif // WITH_VERSE_VM
