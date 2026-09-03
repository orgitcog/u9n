// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMType.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/VVMStructuredArchiveVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VType);
DEFINE_TRIVIAL_VISIT_REFERENCES(VType);

VType::VType(FAllocationContext Context, VEmergentType* Type)
	: VHeapValue(Context, Type)
{
}

DEFINE_DERIVED_VCPPCLASSINFO(VTrivialType)
DEFINE_TRIVIAL_VISIT_REFERENCES(VTrivialType);

TGlobalHeapPtr<VTrivialType> VTrivialType::Singleton;

void VTrivialType::Initialize(FAllocationContext Context)
{
	V_DIE_UNLESS(VEmergentTypeCreator::EmergentTypeForTrivialType);
	Singleton.Set(Context, new (Context.AllocateFastCell(sizeof(VTrivialType))) VTrivialType(Context));
}

VTrivialType::VTrivialType(FAllocationContext Context)
	: VType(Context, VEmergentTypeCreator::EmergentTypeForTrivialType.Get())
{
}

#define DEFINE_PRIMITIVE_TYPE(Name)                                                                                           \
	DEFINE_DERIVED_VCPPCLASSINFO(V##Name##Type)                                                                               \
	DEFINE_TRIVIAL_VISIT_REFERENCES(V##Name##Type)                                                                            \
	TGlobalTrivialEmergentTypePtr<&V##Name##Type::StaticCppClassInfo> V##Name##Type::GlobalTrivialEmergentType;               \
	TGlobalHeapPtr<V##Name##Type> V##Name##Type::Singleton;                                                                   \
	void V##Name##Type::SerializeLayout(FAllocationContext Context, V##Name##Type*& This, FStructuredArchiveVisitor& Visitor) \
	{                                                                                                                         \
		if (Visitor.IsLoading())                                                                                              \
		{                                                                                                                     \
			This = Singleton.Get();                                                                                           \
		}                                                                                                                     \
	}                                                                                                                         \
	void V##Name##Type::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)                         \
	{                                                                                                                         \
	}                                                                                                                         \
	void V##Name##Type::Initialize(FAllocationContext Context)                                                                \
	{                                                                                                                         \
		Singleton.Set(Context, new (Context.AllocateFastCell(sizeof(V##Name##Type))) V##Name##Type(Context));                 \
	}                                                                                                                         \
	V##Name##Type::V##Name##Type(FAllocationContext Context)                                                                  \
		: VType(Context, &GlobalTrivialEmergentType.Get(Context))                                                             \
	{                                                                                                                         \
	}

#define DEFINE_STRUCTURAL_TYPE(Name, Fields)                                                                                  \
	DEFINE_DERIVED_VCPPCLASSINFO(V##Name##Type)                                                                               \
	TGlobalTrivialEmergentTypePtr<&V##Name##Type::StaticCppClassInfo> V##Name##Type::GlobalTrivialEmergentType;               \
	template <typename TVisitor>                                                                                              \
	void V##Name##Type::VisitReferencesImpl(TVisitor& Visitor)                                                                \
	{                                                                                                                         \
		Fields(VISIT_STRUCTURAL_TYPE_FIELD);                                                                                  \
	}                                                                                                                         \
	void V##Name##Type::SerializeLayout(FAllocationContext Context, V##Name##Type*& This, FStructuredArchiveVisitor& Visitor) \
	{                                                                                                                         \
		if (Visitor.IsLoading())                                                                                              \
		{                                                                                                                     \
			This = &V##Name##Type::New(Context, Fields(INIT_STRUCTURAL_TYPE_ARG));                                            \
		}                                                                                                                     \
	}                                                                                                                         \
	void V##Name##Type::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)                         \
	{                                                                                                                         \
		Fields(VISIT_STRUCTURAL_TYPE_FIELD);                                                                                  \
	}
#define VISIT_STRUCTURAL_TYPE_FIELD(Name) Visitor.Visit(Name, TEXT(#Name))
#define INIT_STRUCTURAL_TYPE_ARG(Name) VValue()

DEFINE_DERIVED_VCPPCLASSINFO(VVoidType)
DEFINE_TRIVIAL_VISIT_REFERENCES(VVoidType)
TGlobalTrivialEmergentTypePtr<&VVoidType::StaticCppClassInfo> VVoidType::GlobalTrivialEmergentType;
TGlobalHeapPtr<VVoidType> VVoidType::Singleton;
void VVoidType::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	Builder.Append(UTF8TEXT("void"));
}
void VVoidType::SerializeLayout(FAllocationContext Context, VVoidType*& This, FStructuredArchiveVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		This = Singleton.Get();
	}
}
void VVoidType::SerializeImpl(FAllocationContext Cotnext, FStructuredArchiveVisitor& Visitor)
{
}
void VVoidType::Initialize(FAllocationContext Context)
{
	Singleton.Set(Context, new (Context.AllocateFastCell(sizeof(VVoidType))) VVoidType(Context));
}
VVoidType::VVoidType(FAllocationContext Context)
	: VType(Context, &GlobalTrivialEmergentType.Get(Context))
{
}

DEFINE_PRIMITIVE_TYPE(Any)
DEFINE_PRIMITIVE_TYPE(Comparable)
DEFINE_PRIMITIVE_TYPE(Logic)

DEFINE_PRIMITIVE_TYPE(Rational)

DEFINE_PRIMITIVE_TYPE(Char8)
DEFINE_PRIMITIVE_TYPE(Char32)
DEFINE_PRIMITIVE_TYPE(Range)

#define DEFINE_STRUCTURAL_TYPE(Name, Fields)                                                                                  \
	DEFINE_DERIVED_VCPPCLASSINFO(V##Name##Type)                                                                               \
	TGlobalTrivialEmergentTypePtr<&V##Name##Type::StaticCppClassInfo> V##Name##Type::GlobalTrivialEmergentType;               \
	template <typename TVisitor>                                                                                              \
	void V##Name##Type::VisitReferencesImpl(TVisitor& Visitor)                                                                \
	{                                                                                                                         \
		Fields(VISIT_STRUCTURAL_TYPE_FIELD);                                                                                  \
	}                                                                                                                         \
	void V##Name##Type::SerializeLayout(FAllocationContext Context, V##Name##Type*& This, FStructuredArchiveVisitor& Visitor) \
	{                                                                                                                         \
		if (Visitor.IsLoading())                                                                                              \
		{                                                                                                                     \
			This = &V##Name##Type::New(Context, Fields(INIT_STRUCTURAL_TYPE_ARG));                                            \
		}                                                                                                                     \
	}                                                                                                                         \
	void V##Name##Type::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)                         \
	{                                                                                                                         \
		Fields(VISIT_STRUCTURAL_TYPE_FIELD);                                                                                  \
	}
#define VISIT_STRUCTURAL_TYPE_FIELD(Name) Visitor.Visit(Name, TEXT(#Name))
#define INIT_STRUCTURAL_TYPE_ARG(Name) VValue()

#define TYPE_FIELDS(Field) Field(PositiveType)
DEFINE_STRUCTURAL_TYPE(Type, TYPE_FIELDS)
#undef TYPE_FIELDS

#define TYPE_FIELDS(Field) Field(ElementType)
DEFINE_STRUCTURAL_TYPE(Array, TYPE_FIELDS)
#undef TYPE_FIELDS

#define TYPE_FIELDS(Field) Field(ElementType)
DEFINE_STRUCTURAL_TYPE(Generator, TYPE_FIELDS)
#undef TYPE_FIELDS

#define TYPE_FIELDS(Field) Field(KeyType), Field(ValueType)
DEFINE_STRUCTURAL_TYPE(Map, TYPE_FIELDS)
#undef TYPE_FIELDS

DEFINE_PRIMITIVE_TYPE(Reference)

#define TYPE_FIELDS(Field) Field(ValueType)
DEFINE_STRUCTURAL_TYPE(Pointer, TYPE_FIELDS)
#undef TYPE_FIELDS

#define TYPE_FIELDS(Field) Field(ValueType)
DEFINE_STRUCTURAL_TYPE(Option, TYPE_FIELDS)
#undef TYPE_FIELDS

DEFINE_PRIMITIVE_TYPE(Function)
DEFINE_PRIMITIVE_TYPE(Persistable)

#undef DEFINE_PRIMITIVE_TYPE
#undef DEFINE_STRUCTURAL_TYPE
#undef VISIT_STRUCTURAL_TYPE_FIELD
#undef INIT_STRUCTURAL_TYPE_ARG

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)