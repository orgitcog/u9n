// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VBPVMRuntimeType.h"

#if WITH_VERSE_BPVM
#include "VerseVM/VVMEngineEnvironment.h"
#include "VerseVM/VVMVerse.h"

namespace UE::Verse
{

bool AreEquivalent(const FRuntimeType& TypeA, const void* DataA, const FDynamicallyTypedValue& ValueB)
{
	const FRuntimeType& ValueTypeB = static_cast<const FRuntimeType&>(ValueB.GetType());
	return TypeA.AreEquivalent(DataA, ValueTypeB, ValueB.GetDataPointer());
}

//
// FRuntimeTypeDynamic
//

VERSE_IMPLEMENT_GLOBAL_RUNTIME_TYPE(FRuntimeTypeDynamic)

void FRuntimeTypeDynamic::AppendDiagnosticString(FUtf8StringBuilderBase& Builder, const void* Data, uint32 RecursionDepth) const
{
	const UE::FDynamicallyTypedValue& Value = *static_cast<const UE::FDynamicallyTypedValue*>(Data);
	if (&Value.GetType() == &UE::FDynamicallyTypedValue::NullType())
	{
		Builder.Append("Uninitialized");
		return;
	}
	UE::Verse::FRuntimeType& Type = static_cast<UE::Verse::FRuntimeType&>(Value.GetType());
	Type.AppendDiagnosticString(Builder, Value.GetDataPointer(), RecursionDepth);
}

void FRuntimeTypeDynamic::MarkValueReachable(void* Data, FReferenceCollector& Collector) const
{
	FDynamicallyTypedValue& Value = *static_cast<FDynamicallyTypedValue*>(Data);
	// Mark both the type and the value reachable.
	Value.GetType().MarkReachable(Collector);
	Value.GetType().MarkValueReachable(Value.GetDataPointer(), Collector);
}

void FRuntimeTypeDynamic::DestroyValue(void* Data) const
{
	static_cast<FDynamicallyTypedValue*>(Data)->~FDynamicallyTypedValue();
}

void FRuntimeTypeDynamic::InitializeValue(void* Data) const
{
	new (Data) FDynamicallyTypedValue();
}

void FRuntimeTypeDynamic::InitializeValueFromCopy(void* DestData, const void* SourceData) const
{
	const FDynamicallyTypedValue* SourceValue = static_cast<const FDynamicallyTypedValue*>(SourceData);
	new (DestData) FDynamicallyTypedValue(*SourceValue);
}

void FRuntimeTypeDynamic::SerializeValue(FStructuredArchive::FSlot Slot, void* Data, const void* DefaultData) const
{
	FDynamicallyTypedValue& Value = Slot.GetUnderlyingArchive().IsLoading()
									  ? *new (Data) FDynamicallyTypedValue()
									  : *static_cast<FDynamicallyTypedValue*>(Data);
	const FDynamicallyTypedValue* DefaultValue = static_cast<const FDynamicallyTypedValue*>(DefaultData);
	FArchive& Ar = Slot.GetUnderlyingArchive();

	FStructuredArchive::FRecord Record = Slot.EnterRecord();

	// If the type is not the null type, serialize it.
	FDynamicallyTypedValueType* Type = &Value.GetType();
	TOptional<FStructuredArchive::FSlot> MaybeTypeSlot = Record.TryEnterField(
		TEXT("Type"),
		Type != &FDynamicallyTypedValue::NullType());
	if (MaybeTypeSlot.IsSet())
	{
		::Verse::IEngineEnvironment* Engine = ::Verse::VerseVM::GetEngineEnvironment();
		check(Engine);
		Engine->ArchiveType(*MaybeTypeSlot, *reinterpret_cast<UE::Verse::FRuntimeType**>(&Type));
		if (Ar.IsLoading())
		{
			Value.InitializeAsType(*Type);
		}
	}
	else
	{
		Value.SetToNull();
	}

	// If the type is not the null type, ask it to serialize the value.
	if (Type != &FDynamicallyTypedValue::NullType())
	{
		Type->SerializeValue(
			Record.EnterField(TEXT("Value")),
			Value.GetDataPointer(),
			DefaultValue && &DefaultValue->GetType() == Type
				? DefaultValue->GetDataPointer()
				: nullptr);
	}
}

void FRuntimeTypeDynamic::ExportValueToText(FString& OutputString, const void* Data, const void* DefaultData, UObject* Parent, UObject* ExportRootScope) const
{
	const FDynamicallyTypedValue& Value = *static_cast<const FDynamicallyTypedValue*>(Data);
	const FDynamicallyTypedValue* DefaultValue = static_cast<const FDynamicallyTypedValue*>(DefaultData);

	::Verse::IEngineEnvironment* Engine = ::Verse::VerseVM::GetEngineEnvironment();
	check(Engine);
	Engine->ExportRuntimeTypeToText(OutputString, static_cast<FRuntimeType&>(Value.GetType()));
	OutputString += TEXT('(');
	static_cast<FRuntimeType&>(Value.GetType()).ExportValueToText(OutputString, Value.GetDataPointer(), DefaultValue && &DefaultValue->GetType() == &Value.GetType() ? DefaultValue->GetDataPointer() : nullptr, Parent, ExportRootScope);
	OutputString += TEXT(')');
}

bool FRuntimeTypeDynamic::ImportValueFromText(const TCHAR*& InputCursor, void* Data, UObject* Parent, FOutputDevice* ErrorText) const
{
	FDynamicallyTypedValue& Value = *static_cast<FDynamicallyTypedValue*>(Data);

	// Parse the type and dispatch to the appropriate runtime type.
	::Verse::IEngineEnvironment* Engine = ::Verse::VerseVM::GetEngineEnvironment();
	check(Engine);
	FRuntimeType* RuntimeType = Engine->ImportRuntimeTypeFromText(InputCursor, ErrorText);
	if (!RuntimeType)
	{
		return false;
	}

	if (*InputCursor != TEXT('('))
	{
		return false;
	}
	++InputCursor;

	Value.InitializeAsType(*RuntimeType);
	if (!RuntimeType->ImportValueFromText(InputCursor, Value.GetDataPointer(), Parent, ErrorText))
	{
		return false;
	}

	if (*InputCursor != TEXT(')'))
	{
		return false;
	}
	++InputCursor;

	return true;
}

uint32 FRuntimeTypeDynamic::GetValueHash(const void* Data) const
{
	const FDynamicallyTypedValue& Value = *static_cast<const FDynamicallyTypedValue*>(Data);
	return Value.GetType().GetValueHash(Value.GetDataPointer());
}

bool FRuntimeTypeDynamic::AreIdentical(const void* DataA, const void* DataB) const
{
	const FDynamicallyTypedValue& ValueA = *static_cast<const FDynamicallyTypedValue*>(DataA);
	const FDynamicallyTypedValue& ValueB = *static_cast<const FDynamicallyTypedValue*>(DataB);

	// Handle either of the values being uninitialized, which would mean having a type that isn't a FRuntimeType.
	const bool bValueAIsUninitialized = &ValueA.GetType() == &FDynamicallyTypedValue::NullType();
	const bool bValueBIsUninitialized = &ValueB.GetType() == &FDynamicallyTypedValue::NullType();
	if (bValueAIsUninitialized || bValueBIsUninitialized)
	{
		return bValueAIsUninitialized && bValueBIsUninitialized;
	}

	const FRuntimeType& TypeA = static_cast<const FRuntimeType&>(ValueA.GetType());
	const FRuntimeType& TypeB = static_cast<const FRuntimeType&>(ValueB.GetType());
	return TypeA.AreEquivalent(
		ValueA.GetDataPointer(),
		TypeB,
		ValueB.GetDataPointer());
}

bool FRuntimeTypeDynamic::AreEquivalent(const void* DataA, const FRuntimeType& TypeB, const void* DataB) const
{
	const FDynamicallyTypedValue& ValueA = *static_cast<const FDynamicallyTypedValue*>(DataA);
	const FRuntimeType& ValueTypeA = static_cast<const FRuntimeType&>(ValueA.GetType());
	if (TypeB.Kind == EKind::Dynamic)
	{
		return UE::Verse::AreEquivalent(ValueTypeA, ValueA.GetDataPointer(), *static_cast<const FDynamicallyTypedValue*>(DataB));
	}
	return ValueTypeA.AreEquivalent(ValueA.GetDataPointer(), TypeB, DataB);
}

void FRuntimeTypeDynamic::InstanceSubobjects(void* Data, void const* DefaultData, TNotNull<UObject*> Owner, FObjectInstancingGraph* InstanceGraph) const
{
	FDynamicallyTypedValue& Value = *static_cast<FDynamicallyTypedValue*>(Data);
	FDynamicallyTypedValue const* DefaultValue = static_cast<FDynamicallyTypedValue const*>(DefaultData);
	if (DefaultValue && &DefaultValue->GetType() != &Value.GetType())
	{
		DefaultValue = nullptr;
	}
	if (Value.GetType().GetContainsReferences() != EContainsReferences::DoesNot)
	{
		static_cast<FRuntimeType&>(Value.GetType()).InstanceSubobjects(Value.GetDataPointer(), DefaultValue ? DefaultValue->GetDataPointer() : nullptr, Owner, InstanceGraph);
	}
}

bool FRuntimeTypeDynamic::IsValid(const void* Data) const
{
	const FDynamicallyTypedValue& Value = *static_cast<const FDynamicallyTypedValue*>(Data);
	return &Value.GetType() != &FDynamicallyTypedValue::NullType()
		&& static_cast<UE::Verse::FRuntimeType&>(Value.GetType()).IsValid(Value.GetDataPointer());
}

} // namespace UE::Verse
#endif // WITH_VERSE_BPVM
