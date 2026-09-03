// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "Misc/AssertionMacros.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "VerseVM/Inline/VVMNativeStructInline.h"
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMVerseClass.h"
#include "VerseVM/VVMVerseStruct.h"

namespace Verse
{
inline bool FEmergentTypesCacheKeyFuncs::Matches(FEmergentTypesCacheKeyFuncs::KeyInitType A, FEmergentTypesCacheKeyFuncs::KeyInitType B)
{
	return A == B;
}

inline bool FEmergentTypesCacheKeyFuncs::Matches(FEmergentTypesCacheKeyFuncs::KeyInitType A, const VUniqueStringSet& B)
{
	return *(A.Get()) == B;
}

inline uint32 FEmergentTypesCacheKeyFuncs::GetKeyHash(FEmergentTypesCacheKeyFuncs::KeyInitType Key)
{
	return GetTypeHash(Key);
}

inline uint32 FEmergentTypesCacheKeyFuncs::GetKeyHash(const VUniqueStringSet& Key)
{
	return GetTypeHash(Key);
}

inline VArchetype& VArchetype::New(FAllocationContext Context, const TArray<VEntry>& InEntries)
{
	size_t NumBytes = offsetof(VArchetype, Entries) + InEntries.Num() * sizeof(Entries[0]);
	return *new (Context.AllocateFastCell(NumBytes)) VArchetype(Context, InEntries);
}

inline VArchetype::VArchetype(FAllocationContext Context, const TArray<VEntry>& InEntries)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	, NumEntries(InEntries.Num())
{
	for (uint32 Index = 0; Index < NumEntries; ++Index)
	{
		new (&Entries[Index]) VEntry(InEntries[Index]);
	}
}

inline VArchetype::VArchetype(FAllocationContext Context, const uint32 InNumEntries)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	, NumEntries(InNumEntries)
{
	for (uint32 Index = 0; Index < NumEntries; ++Index)
	{
		new (&Entries[Index]) VEntry{};
	}
}

inline VArchetype::VEntry VArchetype::VEntry::Constant(FAllocationContext Context, VUniqueString& InQualifiedField, bool bInNative, bool bIsInstanced, bool bUseCRCName, VValue InType, VValue InValue)
{
	checkSlow(InQualifiedField.AsStringView().Len() > 0);
	EArchetypeEntryFlags Flags = EArchetypeEntryFlags::HasDefaultValueExpression;
	if (bInNative)
	{
		Flags |= EArchetypeEntryFlags::Native;
	}
	if (bIsInstanced)
	{
		Flags |= EArchetypeEntryFlags::IsInstanced;
	}
	if (bUseCRCName)
	{
		Flags |= EArchetypeEntryFlags::UseCRCName;
	}
	return {
		.Name = {Context, InQualifiedField},
		.Type = {Context,           InType},
		.Value = {Context,          InValue},
		.Flags = Flags,
	};
}

inline VArchetype::VEntry VArchetype::VEntry::Field(FAllocationContext Context, VUniqueString& InQualifiedField, bool bInNative, bool bIsInstanced, bool bUseCRCName, VValue InType)
{
	checkSlow(InQualifiedField.AsStringView().Len() > 0);
	EArchetypeEntryFlags Flags = EArchetypeEntryFlags::None;
	if (bInNative)
	{
		Flags |= EArchetypeEntryFlags::Native;
	}
	if (bIsInstanced)
	{
		Flags |= EArchetypeEntryFlags::IsInstanced;
	}
	if (bUseCRCName)
	{
		Flags |= EArchetypeEntryFlags::UseCRCName;
	}
	return {
		.Name = {Context, InQualifiedField},
		.Type = {Context, InType},
		.Value = {},
		.Flags = Flags,
	};
}

inline VArchetype::VEntry VArchetype::VEntry::InitializedField(FAllocationContext Context, VUniqueString& InQualifiedField, bool bInNative, bool bIsInstanced, bool bUseCRCName, VValue InType)
{
	checkSlow(InQualifiedField.AsStringView().Len() > 0);
	EArchetypeEntryFlags Flags = EArchetypeEntryFlags::HasDefaultValueExpression;
	if (bInNative)
	{
		Flags |= EArchetypeEntryFlags::Native;
	}
	if (bIsInstanced)
	{
		Flags |= EArchetypeEntryFlags::IsInstanced;
	}
	if (bUseCRCName)
	{
		Flags |= EArchetypeEntryFlags::UseCRCName;
	}
	return {
		.Name = {Context, InQualifiedField},
		.Type = {Context, InType},
		.Value = {}, // Uninitialized `VValue` here since the field data is just set by `UnifyField` when the class body procedure runs.
		.Flags = Flags,
	};
}

inline bool VArchetype::VEntry::IsConstant() const
{
	return !Value.Get().IsUninitialized();
}

inline bool VArchetype::VEntry::IsNative() const
{
	return EnumHasAnyFlags(Flags, EArchetypeEntryFlags::Native);
}

inline bool VArchetype::VEntry::UseCRCName() const
{
	return EnumHasAnyFlags(Flags, EArchetypeEntryFlags::UseCRCName);
}

inline bool VArchetype::VEntry::HasDefaultValueExpression() const
{
	// It's not enough to check that the value is uninitialized, since there could be bytecode run later on in the body function
	// that initializes the field.
	return EnumHasAnyFlags(Flags, EArchetypeEntryFlags::HasDefaultValueExpression);
}

inline bool VArchetype::VEntry::IsInstanced() const
{
	return EnumHasAnyFlags(Flags, EArchetypeEntryFlags::IsInstanced);
}

template <class CppStructType>
inline VNativeStruct& VClass::NewNativeStruct(FAllocationContext Context, CppStructType&& Struct)
{
	VEmergentType& EmergentType = GetOrCreateEmergentTypeForNativeStruct(Context);
	return VNativeStruct::New(Context, EmergentType, Forward<CppStructType>(Struct));
}

inline VClass& VClass::New(
	FAllocationContext InContext,
	VPackage* InPackage,
	VArray* InRelativePath,
	VArray* InClassName,
	VArray* InAttributeIndices,
	VArray* InAttributes,
	UStruct* InImportStruct,
	bool bInNativeBound,
	EKind InKind,
	EFlags InFlags,
	const TArray<VClass*>& InInherited,
	VArchetype& InArchetype,
	VProcedure& InConstructor)
{
	const size_t NumBytes = offsetof(VClass, Inherited) + InInherited.Num() * sizeof(Inherited[0]);
	return *new (InContext.Allocate(FHeap::DestructorSpace, NumBytes)) VClass(
		InContext,
		InPackage,
		InRelativePath,
		InClassName,
		InAttributeIndices,
		InAttributes,
		InImportStruct,
		bInNativeBound,
		InKind,
		InFlags,
		InInherited,
		InArchetype,
		InConstructor);
}
} // namespace Verse
#endif // WITH_VERSE_VM
