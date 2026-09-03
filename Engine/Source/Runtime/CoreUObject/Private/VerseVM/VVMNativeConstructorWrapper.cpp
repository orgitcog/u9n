// Copyright Epic Games, Inc. All Rights Reserved.

#if !WITH_VERSE_BPVM || defined(__INTELLISENSE__)

#include "VerseVM/VVMNativeConstructorWrapper.h"
#include "VerseVM/Inline/VVMMapInline.h"
#include "VerseVM/Inline/VVMNativeConstructorWrapperInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/VVMNativeStruct.h"
#include "VerseVM/VVMValuePrinting.h"
#include "VerseVM/VVMVerseClass.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VNativeConstructorWrapper);
TGlobalTrivialEmergentTypePtr<&VNativeConstructorWrapper::StaticCppClassInfo> VNativeConstructorWrapper::GlobalTrivialEmergentType;

template <typename TVisitor>
void VNativeConstructorWrapper::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(NativeObject, TEXT("NativeObject"));
	Visitor.Visit(FieldsInitialized, TEXT("FieldsInitialized"));
}

void VNativeConstructorWrapper::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	if (IsCellFormat(Format))
	{
		Builder << UTF8TEXT("Wrapped object(Value: %s");
		WrappedObject().AppendToString(Builder, Context, Format, RecursionDepth + 1);
		Builder << UTF8TEXT(")");
	}
}

VNativeConstructorWrapper& VNativeConstructorWrapper::New(FAllocationContext Context, VNativeStruct& ObjectToWrap)
{
	return *new (Context.AllocateFastCell(sizeof(VNativeConstructorWrapper))) VNativeConstructorWrapper(Context, ObjectToWrap);
}

VNativeConstructorWrapper& VNativeConstructorWrapper::New(FAllocationContext Context, UObject& ObjectToWrap)
{
	return *new (Context.AllocateFastCell(sizeof(VNativeConstructorWrapper))) VNativeConstructorWrapper(Context, ObjectToWrap);
}

uint32 VNativeConstructorWrapper::GetNumProperties(VShape& Shape)
{
	// This is assuming that the fields are already de-duplicated in the shape.
	uint32 NumProperties = 0;
	for (auto Iterator = Shape.CreateFieldsIterator(); Iterator; ++Iterator)
	{
		const VShape::VEntry& Entry = Iterator->Value;
		if (Entry.IsProperty())
		{
			++NumProperties;
		}
	}
	return NumProperties;
}

VNativeConstructorWrapper::VNativeConstructorWrapper(FAllocationContext Context, VNativeStruct& NativeStruct)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	, NativeObject(Context, NativeStruct)
{
	if (VShape* Shape = NativeStruct.GetEmergentType()->Shape.Get())
	{
		FieldsInitialized.Set(Context, &VMapBase::New<VMutableMap>(Context, GetNumProperties(*Shape)));
	}
}

VNativeConstructorWrapper::VNativeConstructorWrapper(FAllocationContext Context, UObject& UEObject)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	, NativeObject(Context, &UEObject)
{
	if (UVerseClass* VerseClass = Cast<UVerseClass>(UEObject.GetClass()))
	{
		FieldsInitialized.Set(Context, &VMapBase::New<VMutableMap>(Context, GetNumProperties(*VerseClass->Shape)));
	}
}
} // namespace Verse
#endif
