// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Misc/Optional.h"
#include "VVMCell.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMNameValueMap.h"
#include "VerseVM/VVMNames.h"

class UPackage;
struct FCoreRedirect;

namespace Verse
{
struct VNamedType;
struct VProgram;
struct VTupleType;
struct VMutableArray;
struct VWeakCellMap;

enum class EDigestVariant : uint8
{
	PublicAndEpicInternal = 0,
	PublicOnly = 1,
};

struct FVersionedDigest
{
	TWriteBarrier<VArray> Code;
	uint32 EffectiveVerseVersion;
	TArray<FName> DependencyPackageNames;
};

struct VPackage : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	TOptional<FVersionedDigest> DigestVariants[2]; // One for each variant

	VUniqueString& GetName() const { return *Name; }
	VUniqueString& GetRootPath() const { return *RootPath; }

	uint32 NumDefinitions() const { return Definitions.Num(); }
	VUniqueString& GetDefinitionName(uint32 Index) const { return Definitions.GetName(Index); }
	VValue GetDefinition(uint32 Index) const { return Definitions.GetValue(Index).Follow(); }
	void AddDefinition(FAllocationContext Context, VUniqueString& Path, VValue Definition) { Definitions.AddValue(Context, Path, Definition); }
	VValue LookupDefinition(FUtf8StringView Path) const { return Definitions.Lookup(Path); }
	template <typename CellType>
	CellType* LookupDefinition(FUtf8StringView Path) const { return Definitions.LookupCell<CellType>(Path); }

	COREUOBJECT_API void NotifyUsedTupleType(FAllocationContext Context, VTupleType* TupleType);
	template <typename FunctorType> // FunctorType is (VTupleType*) -> void
	void ForEachUsedTupleType(FunctorType&& Functor);

	COREUOBJECT_API void NotifyUsedImport(FAllocationContext Context, VNamedType* TypeWithImport);
	template <typename FunctorType> // FunctorType is (VNamedType*) -> void
	void ForEachUsedImport(FunctorType&& Functor);

	COREUOBJECT_API UPackage* GetUPackage() const;
	COREUOBJECT_API UPackage* GetOrCreateUPackage(FAllocationContext Context);
	void AddRedirect(FCoreRedirect&& Redirect);
	COREUOBJECT_API void ApplyRedirects();
	COREUOBJECT_API void ResetRedirects();

	static VPackage& New(FAllocationContext Context, VUniqueString& Name, VUniqueString& RootPath, uint32 Capacity)
	{
		return *new (Context.Allocate(FHeap::DestructorAndCensusSpace, sizeof(VPackage))) VPackage(Context, &Name, &RootPath, Capacity);
	}

	COREUOBJECT_API void RecordCells(FAllocationContext Context);

	static void SerializeLayout(FAllocationContext Context, VPackage*& This, FStructuredArchiveVisitor& Visitor);
	void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);

private:
	friend struct VIntrinsics;

	COREUOBJECT_API VPackage(FAllocationContext Context, VUniqueString* InName, VUniqueString* InRootPath, uint32 Capacity);
	~VPackage();

	TWriteBarrier<VUniqueString> Name;
	TWriteBarrier<VUniqueString> RootPath;

	VNameValueMap Definitions;

	// Keep track of tuple types and imports here in addition to the corresponding global maps in VProgram
	// so that during incremental compilation we can populate the global maps with these
	TWriteBarrier<VWeakCellMap> UsedTupleTypes;
	TWriteBarrier<VMutableArray> UsedImports;

	TWriteBarrier<VValue> AssociatedUPackage;
	TArray<FCoreRedirect> Redirects;
};
} // namespace Verse
#endif // WITH_VERSE_VM
