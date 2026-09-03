// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if WITH_EDITORONLY_DATA

#include "UObject/PropertyStateTracking.h" // Temporary...

#define UE_API COREUOBJECT_API

class FFieldVariant;
class FProperty;
class FStructuredArchiveRecord;
class UClass;
class UObject;
class UStruct;

namespace UE { class FPropertyPathNameTree; }
namespace UE { class FUnknownEnumNames; }

namespace UE
{

/** Query if InstanceDataObject support is available generally. */
UE_API bool IsInstanceDataObjectSupportEnabled();
/** Query if InstanceDataObject support is enabled for a specific object. */
UE_API bool IsInstanceDataObjectSupportEnabled(const UObject* Object);
/** Query if InstanceDataObject support is enabled for garbage collection for a specific class. */
UE_API bool IsInstanceDataObjectSupportEnabledForGC(const UClass* Class);
/** Query if uninitialized alert UI is enabled **/
UE_API bool IsUninitializedAlertUIEnabled();
/** Query if InstanceDataObjects should be saved instead of instances. */
UE_API bool IsInstanceDataObjectImpersonationEnabledOnSave();
/** Query if InstanceDataObjects should be constructed using an archetype chain. */
UE_API bool IsInstanceDataObjectArchetypeChainEnabled();

UE_API bool StructContainsLooseProperties(const UStruct* Struct);
UE_API bool StructIsInstanceDataObjectStruct(const UStruct* Struct);

UE_API bool IsPropertyLoose(const FProperty* Property);

/** Query if placeholder support is enabled for a specific import class type. */
bool CanCreatePropertyBagPlaceholderTypeForImportClass(const UClass* ImportType);

/** Helper to check if a class is an IDO class. */
UE_API bool IsClassOfInstanceDataObjectClass(const UStruct* Class);

/** Generate a UClass that contains the union of the properties of PropertyTree and OwnerClass. */
UClass* CreateInstanceDataObjectClass(const FPropertyPathNameTree* PropertyTree, const FUnknownEnumNames* EnumNames, UClass* OwnerClass, UObject* Outer);

/** Copy whether each property was set by serialization from one IDO to another. */
void CopyPropertyValueSerializedData(const FFieldVariant& OldField, void* OldDataPtr, const FFieldVariant& NewField, void* NewDataPtr);

/** Returns whether the object is an IDO. */
UE_API bool IsInstanceDataObject(const UObject* Object);

/**
 * Creates an IDO for Owner and serializes it from Archive.
 *
 * USE THE OVERLOAD TAKING AN ARCHIVE IF UNKNOWN PROPERTY VALUES ARE REQUIRED!
 *
 * Creates an IDO class containing the union of the fields in Owner and any associated unknown property tree.
 *
 * @param Owner   Object from which to construct an InstanceDataObject.
 * @return The InstanceDataObject for Owner.
 */
UE_API UObject* CreateInstanceDataObject(UObject* Owner);

/**
 * Creates an IDO for Owner and serializes it from Archive.
 *
 * Creates an IDO class containing the union of the fields in Owner and any associated unknown property tree.
 *
 * @param Owner   Object from which to construct an InstanceDataObject.
 * @param Archive   Archive from which to serialize script properties for the object.
 * @param StartOffset   Offset to seek Archive to before serializing script properties for the object.
 * @param EndOffset   Offset to expect Archive to be at after serializing script properties for the object.
 * @return The InstanceDataObject for Owner.
 */
UE_API UObject* CreateInstanceDataObject(UObject* Owner, FArchive& Ar, int64 StartOffset, int64 EndOffset);

/** Returns the IDO for the object if it has one, otherwise returns the object. */
UE_API UObject* ResolveInstanceDataObject(UObject* Object);

} // UE

#undef UE_API

#endif // WITH_EDITORONLY_DATA
