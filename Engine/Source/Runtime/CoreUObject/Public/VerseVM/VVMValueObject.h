// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMObject.h"

namespace Verse
{
enum class ECompares : uint8;

/// Specialization of VObject that stores only VValues
struct VValueObject : VObject
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VObject);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	/// Allocate a new object with the given shape, populated with placeholders
	static VValueObject& NewUninitialized(FAllocationContext Context, VEmergentType& InEmergentType);

	bool CreateField(const VUniqueString& Name);

protected:
	friend class FInterpreter;

	static std::byte* AllocateCell(FAllocationContext Context, VCppClassInfo& CppClassInfo, uint64 NumIndexedFields);

	VValueObject(FAllocationContext Context, VEmergentType& InEmergentType);

	ECompares EqualImpl(FAllocationContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);
	uint32 GetTypeHashImpl();
	VValue MeltImpl(FAllocationContext Context);
	FOpResult FreezeImpl(FAllocationContext Context);
	static void SerializeLayout(FAllocationContext Context, VValueObject*& This, FStructuredArchiveVisitor& Visitor);
	void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);
};
} // namespace Verse
#endif // WITH_VERSE_VM
