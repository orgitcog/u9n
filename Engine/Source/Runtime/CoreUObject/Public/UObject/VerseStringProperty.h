// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/UnrealType.h"
#include "VerseVM/VVMNativeString.h"

#define UE_API COREUOBJECT_API

Expose_TNameOf(Verse::FNativeString)

typedef TProperty_WithEqualityAndSerializer<Verse::FNativeString, FProperty> FVerseStringProperty_Super;

// FVerseStringProperty ?
class FVerseStringProperty : public FVerseStringProperty_Super
{
	DECLARE_FIELD_API(FVerseStringProperty, FVerseStringProperty_Super, CASTCLASS_FVerseStringProperty, UE_API)
public:
	typedef FVerseStringProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	FProperty* Inner = nullptr;

	FVerseStringProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
		: Super(InOwner, InName, InObjectFlags)
	{
	}

	FVerseStringProperty(FFieldVariant InOwner, const UECodeGen_Private::FVerseStringPropertyParams& Prop)
		: Super(InOwner, (const UECodeGen_Private::FPropertyParamsBaseWithOffset&)Prop)
	{
	}

	UE_API virtual ~FVerseStringProperty();
	
	// FField interface
	UE_API virtual void Serialize(FArchive& Ar) override;
	UE_API virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	UE_API virtual void AddCppProperty(FProperty* Property) override;
	// End of FField interface

	// FProperty interface
protected:
	UE_API virtual void ExportText_Internal(FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const override;
	UE_API virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const override;
	UE_API uint32 GetValueTypeHashInternal(const void* Src) const override;
	UE_API virtual bool LoadTypeName(UE::FPropertyTypeName Type, const FPropertyTag* Tag = nullptr) override;
	UE_API virtual void SaveTypeName(UE::FPropertyTypeNameBuilder& Type) const override;
	// End of FProperty interface
};

#undef UE_API
