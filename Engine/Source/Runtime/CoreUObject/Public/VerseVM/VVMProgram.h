// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMCell.h"
#include "VVMPackage.h"
#include "VVMTupleType.h"
#include "VerseVM/VVMNameValueMap.h"

namespace Verse
{
struct VIntrinsics;
struct VMapBase;

struct VProgram : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	uint32 NumPackages() const { return PackageMap.Num(); }
	const VUniqueString& GetPackageName(uint32 Index) const { return PackageMap.GetName(Index); }
	VPackage& GetPackage(uint32 Index) const { return PackageMap.GetCell<VPackage>(Index); }
	COREUOBJECT_API void AddPackage(FAllocationContext Context, VUniqueString& Name, VPackage& Package, bool bRegisterUsedTypes);
	COREUOBJECT_API void RemovePackage(FUtf8StringView VersePackageName);
	VPackage* LookupPackage(FUtf8StringView VersePackageName) const { return PackageMap.LookupCell<VPackage>(VersePackageName); }

	// Will overwrite existing entry if exists
	COREUOBJECT_API void AddTupleType(FAllocationContext Context, VUniqueString& MangledName, VTupleType& TupleType);
	COREUOBJECT_API VTupleType* LookupTupleType(FAccessContext Context, VUniqueString& MangledName) const;

	// Will overwrite existing entry if exists
	COREUOBJECT_API void AddImport(FAllocationContext Context, VNamedType& TypeWithImport, UField* ImportedType);
	COREUOBJECT_API VNamedType* LookupImport(FAllocationContext Context, UField* ImportedType) const;
	VPackage* LookupPackage(FAllocationContext Context, UPackage* Package);

	VIntrinsics& GetIntrinsics() const { return *Intrinsics.Get(); }
	void AddIntrinsics(FAllocationContext Context, VIntrinsics& InIntrinsics) { Intrinsics.Set(Context, InIntrinsics); }

	COREUOBJECT_API void Reset(FAllocationContext Context);

	static VProgram& New(FAllocationContext Context, uint32 Capacity)
	{
		return *new (Context.AllocateFastCell(sizeof(VProgram))) VProgram(Context, Capacity);
	}

private:
	VProgram(FAllocationContext Context, uint32 Capacity)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, PackageMap(Context, Capacity)
	{
	}

	VNameValueMap PackageMap;
	TWriteBarrier<VWeakCellMap> TupleTypeMap;
	TWriteBarrier<VMapBase> ImportMap; // For reverse lookup UField -> VNamedType
	TWriteBarrier<VIntrinsics> Intrinsics;
};
} // namespace Verse
#endif // WITH_VERSE_VM
