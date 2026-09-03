// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMCell.h"
#include "VerseVM/VVMHeap.h"
#include "VerseVM/VVMPropertyInlineCache.h"
#include "VerseVM/VVMShape.h"

namespace Verse
{
enum class EValueStringFormat;
struct VUniqueString;

/// Base class for Verse objects that may store fields and associated values for those fields on it.
/// An object points to an emergent type, which in turn points to a "shape".
/// A "shape" is a dynamic memory layout of fields and their offsets.
struct VObject : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);

	FOpResult LoadField(FAllocationContext Context, const VUniqueString& Name, FCacheCase* OutCacheCase = nullptr);
	FOpResult SetField(FAllocationContext Context, const VUniqueString& Name, VValue Value);

	bool IsStruct() { return IsDeeplyMutable(); }
	void SetIsStruct() { SetIsDeeplyMutable(); }

	void VisitMembersImpl(FAllocationContext Context, FDebuggerVisitor& Visitor);

	void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);

	// `TBinaryFunction` should be invocable with `(declval<FUtf8StringView>(), declval<FOpResult>())`.
	template <typename TBinaryFunction>
	bool AllFields(FAllocationContext Context, TBinaryFunction);

protected:
	friend class FInterpreter;
	friend struct VClass;
	friend struct VNativeConstructorWrapper;

	VObject(FAllocationContext Context, VEmergentType& InEmergentType);

	static constexpr const size_t DataAlignment = VERSE_HEAP_MIN_ALIGN;

	COREUOBJECT_API FOpResult LoadField(FAllocationContext Context, VEmergentType& EmergentType, const VShape::VEntry* Field, FCacheCase* OutCacheCase = nullptr);
	static FOpResult SetField(FAllocationContext Context, const VShape& Shape, const VUniqueString& Name, void* Data, VValue Value);
	static FOpResult SetField(FAllocationContext Context, const VShape::VEntry& Field, void* Data, VValue Value);

	static size_t DataOffset(const VCppClassInfo& CppClassInfo);
	/*
	 * Mutable variables store their data as a `VRestValue`.
	 * It's not an array of `VValue`s because you can potentially load a class member before actually defining it. i.e.
	 *
	 * ```
	 * c := class {x:int}
	 * C := c{}
	 * Foo(C.X) # allocates a placeholder
	 * C.X := 1  # This is the first time `c.X` actually gets defined.
	 * ```
	 *
	 * This stores the actual data for individual fields. Some constants and procedures are stored in the shape, not the
	 * object (since then there's no need to do an unnecessary index lookup).
	 *
	 * The mapping of offsets to each field are stored in the emergent type's "shape".  The reason why the object
	 * doesn't just store the mapping of fields to data itself is that it will eventually help when we implement inline
	 * caches for retrieving fields on objects. It also helps reduce memory usage because multiple objects can share
	 * the same hash table that describes their layouts.
	 */
	inline void* GetData(const VCppClassInfo& CppClassInfo) const;
	inline VRestValue* GetFieldData(const VCppClassInfo& CppClassInfo) const;
};

inline VObject::VObject(FAllocationContext Context, VEmergentType& InEmergentType)
	: VHeapValue(Context, &InEmergentType)
{
	// Leave initialization of the data to the subclasses
}

inline size_t VObject::DataOffset(const VCppClassInfo& CppClassInfo)
{
	return Align(CppClassInfo.SizeWithoutFields, DataAlignment);
}

inline void* VObject::GetData(const VCppClassInfo& CppClassInfo) const
{
	return BitCast<uint8*>(this) + DataOffset(CppClassInfo);
}

inline VRestValue* VObject::GetFieldData(const VCppClassInfo& CppClassInfo) const
{
	return BitCast<VRestValue*>(GetData(CppClassInfo));
}
} // namespace Verse
#endif // WITH_VERSE_VM
