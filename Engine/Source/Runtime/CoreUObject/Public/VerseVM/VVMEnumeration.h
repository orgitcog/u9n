// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/StringFwd.h"
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMNamedType.h"

namespace Verse
{
enum class EValueStringFormat;
struct VEnumeration;
struct VEnumerator;
struct VUniqueString;

template <typename CppEnumType>
VEnumeration& StaticVEnumeration();

struct VEnumeration : VNamedType
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VNamedType);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	int32 NumEnumerators;

	// Find enumerator for a given int value
	VEnumerator& GetEnumeratorChecked(int32 IntValue) const;

	COREUOBJECT_API VEnumerator* GetEnumerator(const VUniqueString& Name) const;

	template <class SubTypeOfUStruct>
	SubTypeOfUStruct* GetOrCreateUEType(FAllocationContext Context);

	/**
	 * Creates a new enumeration.
	 *
	 * @param Scope         Containing package or null.
	 * @param Name          Name or null.
	 * @param UEMangledName Name to be used when creating the UE version of the class or the UE package.  Can be null.
	 * @param ImportStruct  The Unreal class/struct that is being reflected by this Verse VM type.
	 * @param bNative       `true` if this represents a native class (i.e. defined in C++).
	 * @param Enumerators   The members of this enumeration.
	 */
	static VEnumeration& New(FAllocationContext Context, VPackage* Package, VArray* RelativePath, VArray* EnumName, VArray* AttributeIndices, VArray* Attributes, UEnum* ImportEnum, bool bNative, const TArray<VEnumerator*>& Enumerators);

	static void SerializeLayout(FAllocationContext Context, VEnumeration*& This, FStructuredArchiveVisitor& Visitor);
	void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);

private:
	COREUOBJECT_API VEnumeration(FAllocationContext Context, VPackage* InPackage, VArray* InRelativePath, VArray* InEnumName, VArray* InAttributeIndices, VArray* InAttributes, UEnum* InImportEnum, bool bInNative, const TArray<VEnumerator*>& InEnumerators);
	COREUOBJECT_API VEnumeration(FAllocationContext Context, int32 InNumEnumerators);

	/// Creates an associated UEnum for this VEnumeration
	COREUOBJECT_API UEnum* CreateUEType(FAllocationContext Context);

	TWriteBarrier<VEnumerator> Enumerators[];
};

template <class SubTypeOfUEnum>
inline SubTypeOfUEnum* VEnumeration::GetOrCreateUEType(FAllocationContext Context)
{
	return HasUEType() ? GetUETypeChecked<SubTypeOfUEnum>() : CastChecked<SubTypeOfUEnum>(CreateUEType(Context));
}

} // namespace Verse
#endif // WITH_VERSE_VM
