// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/Casts.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMType.h"
#include "VerseVM/VVMWriteBarrier.h"

class UField;
enum class ECoreRedirectFlags : uint32;

namespace Verse
{
struct VArray;
struct VPackage;

// Abstract base class for VClass and VEnumeration
struct VNamedType : VType
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);

	VPackage& GetPackage() const { return *Package; }
	VArray& GetRelativePath() const { return *RelativePath; }
	VArray& GetBaseName() const { return *BaseName; }

	void AppendQualifiedName(FUtf8StringBuilderBase& Builder) const;
	void AppendScopeName(FUtf8StringBuilderBase& Builder) const;
	COREUOBJECT_API void AppendMangledName(FUtf8StringBuilderBase& Builder, UTF8CHAR Separator = UTF8CHAR('-')) const;

	bool IsNativeBound() const { return bNativeBound; }

	bool HasUEType() const { return !!AssociatedUEType; }
	template <typename UFieldType>
	UFieldType* GetUEType() const { return Cast<UFieldType>(AssociatedUEType.Get().ExtractUObject()); }
	template <typename UFieldType>
	UFieldType* GetUETypeChecked() const { return CastChecked<UFieldType>(AssociatedUEType.Get().AsUObject()); }
	void AddRedirect(ECoreRedirectFlags Kind);

	void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);

	void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);

protected:
	COREUOBJECT_API VNamedType(FAllocationContext Context, VEmergentType* EmergentType, VPackage* InPackage, VArray* InRelativePath, VArray* InBaseName, VArray* InAttributeIndices, VArray* InAttributes, UField* InImportType, bool bInNativeBound);

	COREUOBJECT_API VNamedType(FAllocationContext Context, VEmergentType* EmergentType);

	// The Verse path identifying this type, split into package, path, and name.

	TWriteBarrier<VPackage> Package;
	TWriteBarrier<VArray> RelativePath;
	TWriteBarrier<VArray> BaseName;

	// Attributes holds all the attributes for this type and its members, flattened into one array.
	// Each entry of AttributeIndices is the starting index of a group in Attributes.
	// AttributeIndices has one extra element at the end, to simplify lookup.
	TWriteBarrier<VArray> AttributeIndices;
	TWriteBarrier<VArray> Attributes;

	bool bNativeBound;

	/// An associated UClass/UScriptStruct/UEnum allows this VNamedType to create UObject/VNativeStruct/FEnumProperty instances
	TWriteBarrier<VValue> AssociatedUEType;

	friend class FInterpreter;
};
} // namespace Verse
#endif // WITH_VERSE_VM
