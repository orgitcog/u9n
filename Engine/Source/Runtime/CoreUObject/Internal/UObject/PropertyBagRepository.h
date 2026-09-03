// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/TransactionallySafeCriticalSection.h"
#include "Containers/Map.h"
#include "Templates/FunctionFwd.h"
#include "UObject/ObjectPtr.h"

class UObject;
class FMemoryArchive;
struct FPropertyChangedChainEvent;

namespace UE
{

#if WITH_EDITORONLY_DATA

#if STATS
struct FPropertyBagRepositoryStats
{
	size_t IDOMemoryBytes;
	int32 NumIDOs;
	int32 NumIDOsWithLooseProperties;
	int32 NumPlaceholderTypes;
};
#endif

// Singleton class tracking property bag association with objects
class FPropertyBagRepository
{
	struct FPropertyBagAssociationData
	{
		void Destroy();

		TObjectPtr<UObject> InstanceDataObject = nullptr;
		bool bNeedsFixup = false;
	};
	// TODO: Make private throughout and extend access permissions here or in wrapper classes? Don't want engine code modifying bags outside of serializers and details panels.
	//friend UObjectBase;
	//friend UStruct;
	friend struct FScopedInstanceDataObjectLoad;

private:
	friend class FPropertyBagRepositoryLock;
	mutable FTransactionallySafeCriticalSection CriticalSection;

	// Lifetimes/ownership:
	// Managed within UObjectBase and synced with object lifetime. The repo tracks pointers to bags, not the bags themselves.

	/** Map of objects/subobjects to their top level property bag. */
	// TODO: Currently will only exist in editor world, but could tracking per world make some sense for teardown in future? We're relying on object destruction to occur properly to free these up. 
	TMap<const UObject*, FPropertyBagAssociationData> AssociatedData;
	
	TMap<const UObject*, const UObject*> InstanceDataObjectToOwner;

	FPropertyBagRepository() = default;

public:
	FPropertyBagRepository(const FPropertyBagRepository &) = delete;
	FPropertyBagRepository& operator=(const FPropertyBagRepository&) = delete;
	
	// Singleton accessor
	static COREUOBJECT_API FPropertyBagRepository& Get();

	// Reclaim space - TODO: Hook up to GC.
	void ShrinkMaps();

	// Future version for reworked InstanceDataObjects - track InstanceDataObject rather than bag (directly):

	/**
	 * Instantiate an InstanceDataObject object for the owner and serialize it from the archive.
	 *
	 * Creates an InstanceDataObject class containing the union of the fields in the owner and its associated
	 * unknown property tree.
	 *
	 * @param Owner			Object from which to construct an InstanceDataObject.
	 * @param Archive		Archive from which to serialize script properties for the object.
	 * @param StartOffset	Offset to seek Archive to before serializing script properties for the object.
	 * @param EndOffset		Offset to expect the Archive to be at after serializing script properties for the object.
	 * @param bIsArchetype	True if the IDO is to be treated as an archetype even when RF_ArchetypeObject is not set.
	 * @return				The InstanceDataObject object, UClass derived from associated unknown property tree.
	 */
	UObject* CreateInstanceDataObject(UObject* Owner, FArchive& Archive, int64 StartOffset, int64 EndOffset, bool bIsArchetype = false);
	UObject* DuplicateInstanceDataObject(UObject* SourceOwner, UObject* DestOwner);

	// called at the end of postload to copy data from Owner to its IDO
	COREUOBJECT_API void PostLoadInstanceDataObject(const UObject* Owner);

	// TODO: Restrict property bag  destruction to within UObject::BeginDestroy() & FPropertyBagProperty destructor.
	// Removes bag, InstanceDataObject, and all associated data for this object.
	void DestroyOuterBag(const UObject* Owner);

	/**
	 * ReassociateObjects
	 * @param ReplacedObjects - old/new owner object pairs. Reassigns InstanceDataObjects/bags to the new owner.
	 */
	COREUOBJECT_API void ReassociateObjects(const TMap<UObject*, UObject*>& ReplacedObjects);

	static void PostEditChangeChainProperty(const UObject* Object, FPropertyChangedChainEvent& PropertyChangedEvent);

	/**
	 * RequiresFixup - test if InstanceDataObject properties perfectly match object instance properties. This is necessary for the object to be published in UEFN.    
	 * @param Object		- Object to test.
	 * @param bIncludeOuter - Include the outer objects in the check.
	 * @return				- Does the object's InstanceDataObject, or optionally any of its outer objects, contain any loose properties requiring user fixup before the object may be published?
	 */
	COREUOBJECT_API bool RequiresFixup(const UObject* Object, bool bIncludeOuter = false) const;
	// set the bNeedsFixup flag for this object's IDO to false
	COREUOBJECT_API void MarkAsFixedUp(const UObject* Object = nullptr);

	// Accessors
	COREUOBJECT_API bool HasInstanceDataObject(const UObject* Owner) const;
	COREUOBJECT_API UObject* FindInstanceDataObject(const UObject* Owner);
	COREUOBJECT_API const UObject* FindInstanceDataObject(const UObject* Owner) const;
	COREUOBJECT_API void FindNestedInstanceDataObject(const UObject* Owner, bool bRequiresFixupOnly, TFunctionRef<void(UObject*)> Callback);
	UE_INTERNAL void AddReferencedInstanceDataObject(const UObject* Owner, FReferenceCollector& Collector);

	COREUOBJECT_API const UObject* FindInstanceForDataObject(const UObject* InstanceDataObject) const;

	#if STATS
	UE_INTERNAL void GatherStats(FPropertyBagRepositoryStats& Stats);
	#endif

	// Placeholder object feature flags; external callers can query a particular subfeature to see if it's enabled.
	// Note: These have CVar counterparts defined in the .cpp file. Add an entry there to also create a toggle switch.
	enum class EPlaceholderObjectFeature : uint8
	{
		/** Replace missing type imports with a placeholder type on load so that exports of the missing type can be serialized */
		ReplaceMissingTypeImportsOnLoad,
		/** Serialize references to placeholder exports on load so that they remain persistent and become visible to referencers */
		SerializeExportReferencesOnLoad,
		/** Replace missing types after reinstancing with a placeholder type so that data is not lost after rebuilding scripts */
		ReplaceMissingReinstancedTypes,
		/** Replace dead class types after a script compile with a placeholder type so that data is not lost after a script build failure */
		ReplaceDeadClassInstanceTypes,
	};

	// query for whether or not the given struct/class is a placeholder type
	static COREUOBJECT_API bool IsPropertyBagPlaceholderType(const UStruct* Type);
	// query for whether or not the given object was created as a placeholder type
	static COREUOBJECT_API bool IsPropertyBagPlaceholderObject(const UObject* Object);
	// query for whether or not creating property bag placeholder objects should be allowed
	static COREUOBJECT_API bool IsPropertyBagPlaceholderObjectSupportEnabled();
	// query for whether or not a specific property bag placeholder object feature is enabled
	static COREUOBJECT_API bool IsPropertyBagPlaceholderObjectFeatureEnabled(EPlaceholderObjectFeature Feature);

	/**
	 * Create a new placeholder type object to swap in for a missing class/struct. An object of
	 * this type will be associated with a property bag when serialized so it doesn't lose data.
	 * 
	 * @param Outer			Scope at which to create the placeholder type object (e.g. UPackage).
	 * @param Class			Type object class (or derivative type). For example, UClass::StaticClass().
	 * @param Name			Optional object name. If not specified, a unique object name will be created.
	 * @param Flags			Additional object flags. These will be appended to the default set of type object flags.
	 *						(Note: All placeholder types are transient by definition and internally default to 'RF_Transient'.)
	 * @param SuperStruct	Optional super type. By default, placeholder types are derivatives of UObject (NULL implies default).
	 * 
	 * @return A reference to a new placeholder type object.
	 */ 
	static COREUOBJECT_API UStruct* CreatePropertyBagPlaceholderType(UObject* Outer, UClass* Class, FName Name = NAME_None, EObjectFlags Flags = RF_NoFlags, UStruct* SuperStruct = nullptr);
	template<typename T = UObject>
	static UClass* CreatePropertyBagPlaceholderClass(UObject* Outer, UClass* Class, FName Name = NAME_None, EObjectFlags Flags = RF_NoFlags)
	{
		return Cast<UClass>(CreatePropertyBagPlaceholderType(Outer, Class, Name, Flags, T::StaticClass()));
	}

private:
	void Lock() const { CriticalSection.Lock(); }
	void Unlock() const { CriticalSection.Unlock(); }

	// Internal functions requiring the repository to be locked before being called

	// Delete owner reference and disassociate all data. Returns success.
	bool RemoveAssociationUnsafe(const UObject* Owner);
};

#endif // WITH_EDITORONLY_DATA

} // UE
