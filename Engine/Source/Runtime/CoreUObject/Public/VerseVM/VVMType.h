// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMCell.h"
#include "VVMContext.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMWriteBarrier.h"
#include <new>

namespace Verse
{
struct VCppClassInfo;

template <typename T>
struct TGlobalHeapPtr;

// Represents Verse types, which may be independent of object shape, and independent of C++ type.
struct VType : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);

protected:
	COREUOBJECT_API explicit VType(FAllocationContext Context, VEmergentType* Type);
};

struct VTrivialType : VType
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);

	COREUOBJECT_API static TGlobalHeapPtr<VTrivialType> Singleton;

	static void Initialize(FAllocationContext Context);

private:
	VTrivialType(FAllocationContext Context);
};

#define DECLARE_PRIMITIVE_TYPE(Name)                                                                                       \
	struct V##Name##Type : VType                                                                                           \
	{                                                                                                                      \
		DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);                                                             \
		COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;               \
                                                                                                                           \
		COREUOBJECT_API static TGlobalHeapPtr<V##Name##Type> Singleton;                                                    \
                                                                                                                           \
		static constexpr bool SerializeIdentity = false;                                                                   \
		static void SerializeLayout(FAllocationContext Context, V##Name##Type*& This, FStructuredArchiveVisitor& Visitor); \
		void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);                                \
                                                                                                                           \
		static void Initialize(FAllocationContext Context);                                                                \
                                                                                                                           \
	private:                                                                                                               \
		V##Name##Type(FAllocationContext Context);                                                                         \
	}

#define DECLARE_STRUCTURAL_TYPE(Name, Fields)                                                                                         \
	struct V##Name##Type : VType                                                                                                      \
	{                                                                                                                                 \
		DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);                                                                        \
		COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;                          \
                                                                                                                                      \
		TWriteBarrier<VValue> Fields(DECLARE_STRUCTURAL_TYPE_FIELD);                                                                  \
                                                                                                                                      \
		static V##Name##Type& New(FAllocationContext Context, Fields(DECLARE_STRUCTURAL_TYPE_PARAM))                                  \
		{                                                                                                                             \
			return *new (Context.AllocateFastCell(sizeof(V##Name##Type))) V##Name##Type(Context, Fields(NAME_STRUCTURAL_TYPE_PARAM)); \
		}                                                                                                                             \
                                                                                                                                      \
		static constexpr bool SerializeIdentity = false;                                                                              \
		static void SerializeLayout(FAllocationContext Context, V##Name##Type*& This, FStructuredArchiveVisitor& Visitor);            \
		void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);                                           \
                                                                                                                                      \
	private:                                                                                                                          \
		V##Name##Type(FAllocationContext& Context, Fields(DECLARE_STRUCTURAL_TYPE_PARAM))                                             \
			: VType(Context, &GlobalTrivialEmergentType.Get(Context))                                                                 \
			, Fields(INIT_STRUCTURAL_TYPE_FIELD)                                                                                      \
		{                                                                                                                             \
		}                                                                                                                             \
	};
#define DECLARE_STRUCTURAL_TYPE_FIELD(Name) Name
#define DECLARE_STRUCTURAL_TYPE_PARAM(Name) VValue In##Name
#define NAME_STRUCTURAL_TYPE_PARAM(Name) In##Name
#define INIT_STRUCTURAL_TYPE_FIELD(Name) Name(Context, In##Name)

struct VVoidType : VType
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	COREUOBJECT_API static TGlobalHeapPtr<VVoidType> Singleton;

	void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);

	static constexpr bool SerializeIdentity = false;
	static void SerializeLayout(FAllocationContext Context, VVoidType*& This, FStructuredArchiveVisitor& Visitor);
	void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);

	static void Initialize(FAllocationContext Context);

private:
	VVoidType(FAllocationContext Context);
};

DECLARE_PRIMITIVE_TYPE(Any);
DECLARE_PRIMITIVE_TYPE(Comparable);
DECLARE_PRIMITIVE_TYPE(Logic);

DECLARE_PRIMITIVE_TYPE(Rational);

DECLARE_PRIMITIVE_TYPE(Char8);
DECLARE_PRIMITIVE_TYPE(Char32);
DECLARE_PRIMITIVE_TYPE(Range);

#define TYPE_FIELDS(Field) Field(PositiveType)
DECLARE_STRUCTURAL_TYPE(Type, TYPE_FIELDS)
#undef TYPE_FIELDS

#define TYPE_FIELDS(Field) Field(ElementType)
DECLARE_STRUCTURAL_TYPE(Array, TYPE_FIELDS)
#undef TYPE_FIELDS

#define TYPE_FIELDS(Field) Field(ElementType)
DECLARE_STRUCTURAL_TYPE(Generator, TYPE_FIELDS)
#undef TYPE_FIELDS

#define TYPE_FIELDS(Field) Field(KeyType), Field(ValueType)
DECLARE_STRUCTURAL_TYPE(Map, TYPE_FIELDS)
#undef TYPE_FIELDS

#define TYPE_FIELDS(Field) Field(ValueType)
DECLARE_STRUCTURAL_TYPE(Pointer, TYPE_FIELDS)
#undef TYPE_FIELDS

DECLARE_PRIMITIVE_TYPE(Reference);

#define TYPE_FIELDS(Field) Field(ValueType)
DECLARE_STRUCTURAL_TYPE(Option, TYPE_FIELDS)
#undef TYPE_FIELDS

DECLARE_PRIMITIVE_TYPE(Function);
DECLARE_PRIMITIVE_TYPE(Persistable);

#undef DECLARE_PRIMITIVE_TYPE
#undef DECLARE_STRUCTURAL_TYPE
#undef DECLARE_STRUCTURAL_TYPE_FIELD
#undef DECLARE_STRUCTURAL_TYPE_PARAM
#undef NAME_STRUCTURAL_TYPE_PARAM
#undef INIT_STRUCTURAL_TYPE_FIELD

} // namespace Verse
#endif // WITH_VERSE_VM
