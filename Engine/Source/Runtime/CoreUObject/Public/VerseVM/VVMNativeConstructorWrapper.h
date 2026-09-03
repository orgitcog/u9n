// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMCell.h"
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMMap.h"

namespace Verse
{
struct VUniqueString;
struct VNativeStruct;

/*
  This is a wrapper around a native object for us to know what fields have been initialized or not in the object.
  We only need this for native objects. For pure-Verse objects, we rely on checking if the `VValue` itself in the
  object is/isn't initialized. But native objects don't always have a 1:1 `VObject` representation, and the data
  lives in the `UObject` itself, so that approach doesn't work.

  To handle this, we wrap native objects in a `VNativeConstructorWrapper` object that contains a bitmap of the fields
  initialized instead, and keep track of the fields that way instead. When we are done with archetype construction, we
  unwrap the native object using a special opcode and return it as part of `NewObject`, and let the wrapper object get
  GC'ed during the next collection.

  By convention, non-native Verse objects are not wrapped; the unwrap opcode just no-ops and returns the
  object itself when it encounters it (this is so we can avoid allocating an extra wrapper object in the common
  non-native case).

  Therefore, we always favour emitting the unwrap instruction where a native object needs to be unwrapped, since we
  don't know during codegen whether the object in question is native or not (since we do the wrapping in `NewObject` at
  runtime). Non-native objects can also be turned into native objects at any time (we test this as well using
  `--uobject-probability` in our tests.)
 */
struct VNativeConstructorWrapper : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;
	COREUOBJECT_API void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);

	COREUOBJECT_API static VNativeConstructorWrapper& New(FAllocationContext Context, VNativeStruct& ObjectToWrap);
	COREUOBJECT_API static VNativeConstructorWrapper& New(FAllocationContext Context, UObject& ObjectToWrap);

	void MarkFieldAsInitialized(FAllocationContext Context, VUniqueString& FieldName);

	bool CreateField(FAllocationContext Context, VUniqueString& FieldName);

	VValue WrappedObject() const;

private:
	VNativeConstructorWrapper(FAllocationContext Context, VNativeStruct& NativeStruct);
	VNativeConstructorWrapper(FAllocationContext Context, UObject& UEObject);

	static uint32 GetNumProperties(VShape& Shape);

	/// This should either be a `VNativeStruct`/`UObject` wrapped in a `VValue`.
	TWriteBarrier<VValue> NativeObject;

	/// If the entry does not yet exist in the map, the field is considered uninitialized, and vice versa, regardless of what
	/// value is associated with the entry.
	// TODO: For now, this is a set of the fields that are already initialized. Eventually this should just be a bitmap that is
	// key-ed off the fields' offset indices in the object.
	TWriteBarrier<VMapBase> FieldsInitialized;

	friend class FInterpreter;
};

} // namespace Verse
#endif // WITH_VERSE_VM
