// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "VerseVM/VVMVerseClass.h"

#if WITH_VERSE_BPVM
#include "Serialization/StructuredArchive.h"
#endif

class FString;
class UPackage;
class UStruct;
class UEnum;
struct FTopLevelAssetPath;

#if WITH_VERSE_BPVM
class FOutputDevice;
namespace UE::Verse
{
struct FRuntimeType;
}
#endif

namespace uLang
{
class CTypeBase;
class CScope;
struct SBuildParams;
} // namespace uLang

namespace Verse
{
struct FAllocationContext;
struct VClass;
struct VPackage;
struct VPropertyType;
struct VTupleType;
class CSymbolToResult;

// This interface must be implemented if Verse needs to create UObject instances.
class IEngineEnvironment
{
public:
	// Add persistent vars
	virtual void AddPersistentVars(UObject* Object, const TArray<FVersePersistentVar>& Vars) = 0;

	// Add session vars
	virtual void AddSessionVars(UObject* Object, const TArray<FVerseSessionVar>& Vars) = 0;

#if WITH_VERSE_BPVM
	virtual void ArchiveType(FStructuredArchive::FSlot Slot, UE::Verse::FRuntimeType*& Type) = 0;
	virtual UE::Verse::FRuntimeType* ImportRuntimeTypeFromText(const TCHAR*& InputCursor, FOutputDevice* ErrorText) = 0;
	virtual void ExportRuntimeTypeToText(FString& OutputString, UE::Verse::FRuntimeType& Type) = 0;
#endif

#if WITH_VERSE_BPVM
	virtual void TryBindVniType(UField* Type) = 0;
#endif
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	// Create a property
	virtual FProperty* CreateProperty(FAllocationContext Context, VPackage* Scope, UStruct* Struct, FUtf8StringView PropertyName, FUtf8StringView CRCPropertyName, VType* Type, bool bIsNative, bool bIsInstanced) = 0;

	// Validate property information during code generation
	virtual bool ValidateProperty(FAllocationContext Context, const FName& Name, VType* Type, const FProperty* ExistingProperty, bool bIsInstanced) = 0;

	// Bind a native structure
	virtual void TryBindVniType(VPackage* Scope, UStruct* Struct) = 0;

	// Bind a native module, class, or struct.
	virtual void TryBindVniAsset(FAllocationContext Context, VPackage* Scope, const FTopLevelAssetPath& Path) = 0;

	// Create a new UPackage with the given package name
	virtual UPackage* CreateUPackage(FAllocationContext Context, const TCHAR* PackageName) = 0;
#endif // WITH_VERSE_VM
};
} // namespace Verse
