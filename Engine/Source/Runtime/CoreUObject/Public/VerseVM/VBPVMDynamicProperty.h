// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_BPVM
#include "Misc/NotNull.h"
#include "UObject/DynamicallyTypedValue.h"
#include "UObject/UnrealType.h"

//
// Metadata for a property of FVerseValue type.
//
class FVerseDynamicProperty : public TProperty<UE::FDynamicallyTypedValue, FProperty>
{
	DECLARE_FIELD_API(FVerseDynamicProperty, TProperty, CASTCLASS_None, COREUOBJECT_API)

public:
	COREUOBJECT_API FVerseDynamicProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);

	/**
	 * Constructor used for constructing compiled in properties
	 * @param InOwner Owner of the property
	 * @param Prop Pointer to the compiled in structure describing the property
	 **/
	COREUOBJECT_API FVerseDynamicProperty(FFieldVariant InOwner, const UECodeGen_Private::FVerseValuePropertyParams& Prop);

	// UHT interface
	COREUOBJECT_API virtual FString GetCPPMacroType(FString& ExtendedTypeText) const override;
	// End of UHT interface

	// FProperty interface
	COREUOBJECT_API virtual void InstanceSubobjects(void* Data, void const* DefaultData, TNotNull<UObject*> Owner, struct FObjectInstancingGraph* InstanceGraph) override;
	COREUOBJECT_API virtual bool Identical(const void* A, const void* B, uint32 PortFlags) const override;
	COREUOBJECT_API virtual void SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const override;
	COREUOBJECT_API virtual void ExportText_Internal(FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const override;
	COREUOBJECT_API virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const override;
	COREUOBJECT_API virtual bool ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType = EPropertyObjectReferenceType::Strong) const override;
	COREUOBJECT_API virtual void EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath) override;
	COREUOBJECT_API virtual uint32 GetValueTypeHashInternal(const void* Src) const override;
	virtual bool HasIntrusiveUnsetOptionalState() const override
	{
		return false;
	}
	// End of FProperty interface
};
#endif
