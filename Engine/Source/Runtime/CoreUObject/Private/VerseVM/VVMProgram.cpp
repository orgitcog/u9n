// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMProgram.h"
#include "VerseVM/Inline/VVMMapInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMPackageInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMIntrinsics.h"
#include "VerseVM/VVMMap.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VProgram);
TGlobalTrivialEmergentTypePtr<&VProgram::StaticCppClassInfo> VProgram::GlobalTrivialEmergentType;

void VProgram::AddPackage(FAllocationContext Context, VUniqueString& Name, VPackage& Package, bool bRegisterUsedTypes)
{
	PackageMap.AddValue(Context, Name, VValue(Package));

	if (bRegisterUsedTypes)
	{
		// Register tuple types used by the added package
		Package.ForEachUsedTupleType([Context, this](VTupleType* UsedTupleType) {
			AddTupleType(Context, UsedTupleType->GetUEMangledName(), *UsedTupleType);
		});

		// Register imports used by the added package
		Package.ForEachUsedImport([Context, this](VNamedType* TypeWithImport) {
			AddImport(Context, *TypeWithImport, TypeWithImport->GetUETypeChecked<UField>()); // Will overwrite existing entry if exists
		});
	}
}

void VProgram::RemovePackage(FUtf8StringView VersePackageName)
{
	TOptional<VValue> RemovedValue = PackageMap.RemoveValue(VersePackageName);
	if (RemovedValue)
	{
		RemovedValue->StaticCast<VPackage>().ResetRedirects();
	}

	// Note: The TupleTypeMap will weed out now unused tuple types during the next GC census
	// We leave the ImportMap unchanged as it would be expensive to determine which can be removed
	// There are usually very few imports so leaking them won't do much harm
	// They will get reused if a new package gets added that needs them
}

void VProgram::AddTupleType(FAllocationContext Context, VUniqueString& MangledName, VTupleType& TupleType)
{
	if (!TupleTypeMap)
	{
		TupleTypeMap.Set(Context, VWeakCellMap::New(Context));
	}
	TupleTypeMap->Add(Context, &MangledName, &TupleType);
}

VTupleType* VProgram::LookupTupleType(FAccessContext Context, VUniqueString& MangledName) const
{
	if (!TupleTypeMap)
	{
		return nullptr;
	}

	return static_cast<VTupleType*>(TupleTypeMap->Find(Context, &MangledName));
}

template <typename TVisitor>
void VProgram::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(PackageMap, TEXT("PackageMap"));
	Visitor.Visit(TupleTypeMap, TEXT("TupleTypeMap"));
	Visitor.Visit(ImportMap, TEXT("ImportMap"));
	Visitor.Visit(Intrinsics, TEXT("Intrinsics"));
}

void VProgram::Reset(FAllocationContext Context)
{
	PackageMap.Reset(Context);
	TupleTypeMap.Reset();
	ImportMap.Reset();
}

void VProgram::AddImport(FAllocationContext Context, VNamedType& TypeWithImport, UField* ImportedType)
{
	if (!ImportMap)
	{
		ImportMap.Set(Context, &VMapBase::New<VMutableMap>(Context, 32));
	}

	ImportMap->Add(Context, VValue(ImportedType), VValue(TypeWithImport));
}

VNamedType* VProgram::LookupImport(FAllocationContext Context, UField* ImportedType) const
{
	if (!ImportMap)
	{
		return nullptr;
	}

	VValue FoundImport = ImportMap->Find(Context, VValue(ImportedType));
	if (!FoundImport)
	{
		return nullptr;
	}

	return &FoundImport.StaticCast<VNamedType>();
}

VPackage* VProgram::LookupPackage(FAllocationContext Context, UPackage* Package)
{
	for (int32 Index = 0; Index < PackageMap.Num(); ++Index)
	{
		VPackage& VersePackage = GetPackage(Index);
		if (VersePackage.GetUPackage() == Package)
		{
			return &VersePackage;
		}
	}
	return nullptr;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
