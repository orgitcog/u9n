// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyBagRepository.h"

#include "UObject/PropertyPathFunctions.h"

#if WITH_EDITORONLY_DATA

#include "Containers/Queue.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ArchiveProxy.h"
#include "Serialization/ArchiveCountMem.h"
#include "UObject/GarbageCollection.h"
#include "UObject/InstanceDataObjectUtils.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/PropertyOptional.h"
#include "UObject/PropertyStateTracking.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectSerializeContext.h"
#include "UObject/UObjectThreadContext.h"
#include "Templates/UnrealTemplate.h"

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPropertyBagRepository, Log, All);

namespace UE
{

/** Defined in InstanceDataObjectUtils.cpp */
void CopyTaggedProperties(const UObject* Source, UObject* Dest);

/** Internal registry that tracks the current set of types for property bag container objects instanced as placeholders for package exports that have invalid or missing class imports on load. */
class FPropertyBagPlaceholderTypeRegistry
{
public:
	static FPropertyBagPlaceholderTypeRegistry& Get()
	{
		static FPropertyBagPlaceholderTypeRegistry Instance;
		return Instance;
	}

	void Add(const UStruct* Type)
	{
		PendingPlaceholderTypes.Enqueue(Type);
	}

	void Remove(const UStruct* Type)
	{
		PlaceholderTypes.Remove(Type);
	}

	bool Contains(const UStruct* Type)
	{
		ConsumePendingPlaceholderTypes();
		return PlaceholderTypes.Contains(Type);
	}

	int32 Num() const
	{
		return PlaceholderTypes.Num();
	}

protected:
	FPropertyBagPlaceholderTypeRegistry() = default;
	FPropertyBagPlaceholderTypeRegistry(const FPropertyBagPlaceholderTypeRegistry&) = delete;
	FPropertyBagPlaceholderTypeRegistry& operator=(const FPropertyBagPlaceholderTypeRegistry&) = delete;

	void ConsumePendingPlaceholderTypes()
	{
		if (!PendingPlaceholderTypes.IsEmpty())
		{
			FScopeLock ScopeLock(&CriticalSection);

			TObjectPtr<const UStruct> PendingType;
			while(PendingPlaceholderTypes.Dequeue(PendingType))
			{
				PlaceholderTypes.Add(PendingType);
			}
		}
	}

private:
	FCriticalSection CriticalSection;

	// List of types that have been registered.
	TSet<TObjectPtr<const UStruct>> PlaceholderTypes;

	// Types that have been added but not yet registered. Utilizes a thread-safe queue so we can avoid race conditions during an async load.
	TQueue<TObjectPtr<const UStruct>> PendingPlaceholderTypes;
};

class FPropertyBagRepositoryLock
{
#if THREADSAFE_UOBJECTS
	const FPropertyBagRepository* Repo;	// Technically a singleton, but just in case...
#endif
public:
	FORCEINLINE FPropertyBagRepositoryLock(const FPropertyBagRepository* InRepo)
	{
#if THREADSAFE_UOBJECTS
		if (!(IsGarbageCollectingAndLockingUObjectHashTables() && IsInGameThread()))	// Mirror object hash tables behaviour exactly for now
		{
			Repo = InRepo;
			InRepo->Lock();
		}
		else
		{
			Repo = nullptr;
		}
#else
		check(IsInGameThread());
#endif
	}
	FORCEINLINE ~FPropertyBagRepositoryLock()
	{
#if THREADSAFE_UOBJECTS
		if (Repo)
		{
			Repo->Unlock();
		}
#endif
	}
};

class FArchetypeMatchingArchive : public FArchiveProxy
{
public:

	FArchetypeMatchingArchive(FArchive& InInnerArchive, UObject* InObject, UObject* InArchetype)
		: FArchiveProxy(InInnerArchive)
		, Object(InObject)
		, Archetype(InArchetype)
	{		
	}

	UObject* GetArchetypeFromLoader(const UObject* Obj) override
	{
		if (Archetype && (Obj == Object))
		{
			return Archetype;
		}
		else
		{
			return InnerArchive.GetArchetypeFromLoader(Obj);
		}
	}

private:

	UObject* Object = nullptr;
	UObject* Archetype = nullptr;
};

void FPropertyBagRepository::FPropertyBagAssociationData::Destroy()
{
	if (InstanceDataObject && InstanceDataObject->IsValidLowLevel())
	{
		InstanceDataObject = nullptr;
	}
}

FPropertyBagRepository& FPropertyBagRepository::Get()
{
	static FPropertyBagRepository Repo;
	return Repo;
}

void FPropertyBagRepository::ReassociateObjects(const TMap<UObject*, UObject*>& ReplacedObjects)
{
	if (!IsInstanceDataObjectSupportEnabled())
	{
		return;
	}

	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBagAssociationData OldBagData;
	for (const TPair<UObject*, UObject*>& Pair : ReplacedObjects)
	{
		if (AssociatedData.RemoveAndCopyValue(Pair.Key, OldBagData))
		{
			InstanceDataObjectToOwner.Remove(OldBagData.InstanceDataObject);
			if (Pair.Value != nullptr) // Pair.Value can be nullptr when an object was destroyed like for example a UClass when it's deleted
			{
				if (const FPropertyBagAssociationData* NewBagData = AssociatedData.Find(Pair.Value))
				{
					InstanceDataObjectToOwner.Add(NewBagData->InstanceDataObject, Pair.Value);

					CopyPropertyValueSerializedData(
						OldBagData.InstanceDataObject->GetClass(), OldBagData.InstanceDataObject,
						NewBagData->InstanceDataObject->GetClass(), NewBagData->InstanceDataObject);
				}
				else
				{
					UE_LOG(LogPropertyBagRepository, Log, TEXT("No associated data entry found for replaced object: %s"), *Pair.Value->GetPathName());
				}
			}
			OldBagData.Destroy();
		}
		else if (UStruct* TypeObject = Cast<UStruct>(Pair.Key))
		{
			if (IsPropertyBagPlaceholderType(TypeObject))
			{
				FPropertyBagPlaceholderTypeRegistry::Get().Remove(TypeObject);
			}
		}
	}
}

static FProperty* FindPropertyByNameAndType(const UStruct* Struct, const FProperty* SourceProperty)
{
	// use property impersonation for SaveTypeName so that keys of IDOs and non-IDOs match
	FUObjectSerializeContext* SerializeContext = FUObjectThreadContext::Get().GetSerializeContext();
	TGuardValue<bool> ScopedImpersonateProperties(SerializeContext->bImpersonateProperties, true);

	return FindPropertyByNameAndTypeName(Struct, SourceProperty->GetFName(), FPropertyTypeName(SourceProperty));
}

	
static void CopyProperty(const FProperty* SourceProperty, const void* SourceValue, const FProperty* DestProperty, void* DestValue)
{
	check(SourceProperty->GetID() == DestProperty->GetID());
	if (SourceProperty->SameType(DestProperty))
	{
		SourceProperty->CopySingleValue(DestValue, SourceValue);
	}
	else if (const FStructProperty* SourcePropertyAsStruct = CastField<FStructProperty>(SourceProperty))
	{
		const UStruct* SourceStruct = SourcePropertyAsStruct->Struct;
		UStruct* DestStruct = CastFieldChecked<FStructProperty>(DestProperty)->Struct;
		for (FProperty* SourceChild : TFieldRange<FProperty>(SourceStruct))
		{
			if (FProperty* DestChild = FindPropertyByNameAndType(DestStruct, SourceChild))
			{
				CopyProperty(SourceChild, SourceChild->ContainerPtrToValuePtr<void>(SourceValue),
					DestChild, DestChild->ContainerPtrToValuePtr<void>(DestValue));
			}
		}
	}
	else if (const FOptionalProperty* SourcePropertyAsOptional = CastField<FOptionalProperty>(SourceProperty))
	{
		const FOptionalProperty* DestPropertyAsOptional = CastFieldChecked<FOptionalProperty>(DestProperty);
		FOptionalPropertyLayout SourceOptionalLayout(SourcePropertyAsOptional->GetValueProperty());
		FOptionalPropertyLayout DestOptionalLayout(DestPropertyAsOptional->GetValueProperty());
		if (!SourceOptionalLayout.IsSet(SourceValue))
		{
			DestOptionalLayout.MarkUnset(DestValue);
		}
		else
		{
			const void* SourceChildValue = SourceOptionalLayout.GetValuePointerForRead(SourceValue);
			void* DestChildValue = DestOptionalLayout.MarkSetAndGetInitializedValuePointerToReplace(DestValue);
			
			CopyProperty(SourceOptionalLayout.GetValueProperty(), SourceChildValue,
				DestOptionalLayout.GetValueProperty(), DestChildValue);
		}
	}
	else if (const FArrayProperty* SourcePropertyAsArray = CastField<FArrayProperty>(SourceProperty))
	{
		const FArrayProperty* DestPropertyAsArray = CastFieldChecked<FArrayProperty>(DestProperty);
		FScriptArrayHelper SourceArray(SourcePropertyAsArray, SourceValue);
		FScriptArrayHelper DestArray(DestPropertyAsArray, DestValue);
		DestArray.Resize(SourceArray.Num());
		for (int32 I = 0; I < SourceArray.Num(); ++I)
		{
			CopyProperty(SourcePropertyAsArray->Inner, SourceArray.GetElementPtr(I),
					DestPropertyAsArray->Inner, DestArray.GetElementPtr(I));
		}
	}
	else if (const FSetProperty* SourcePropertyAsSet = CastField<FSetProperty>(SourceProperty))
	{
		const FSetProperty* DestPropertyAsSet = CastFieldChecked<FSetProperty>(DestProperty);
		FScriptSetHelper SourceSet(SourcePropertyAsSet, SourceValue);
		FScriptSetHelper DestSet(DestPropertyAsSet, DestValue);
		DestSet.Set->Empty(0, DestSet.SetLayout);
		for (FScriptSetHelper::FIterator Itr = SourceSet.CreateIterator(); Itr; ++Itr)
		{
			void* DestChild = DestSet.GetElementPtr(DestSet.AddUninitializedValue());
			DestSet.ElementProp->InitializeValue(DestChild);
			
			CopyProperty(SourceSet.ElementProp, SourceSet.GetElementPtr(Itr.GetInternalIndex()),
					DestSet.ElementProp, DestChild);
		}
		DestSet.Rehash();
	}
	else if (const FMapProperty* SourcePropertyAsMap = CastField<FMapProperty>(SourceProperty))
	{
		const FMapProperty* DestPropertyAsMap = CastFieldChecked<FMapProperty>(DestProperty);
		FScriptMapHelper SourceMap(SourcePropertyAsMap, SourceValue);
		FScriptMapHelper DestMap(DestPropertyAsMap, DestValue);
		DestMap.EmptyValues();
		for (FScriptMapHelper::FIterator Itr = SourceMap.CreateIterator(); Itr; ++Itr)
		{
			void* DestChildKey = DestMap.GetKeyPtr(DestMap.AddUninitializedValue());
			DestMap.KeyProp->InitializeValue(DestChildKey);
			
			CopyProperty(SourceMap.KeyProp, SourceMap.GetKeyPtr(Itr.GetInternalIndex()),
					DestMap.KeyProp, DestChildKey);
			
			void* DestChildValue = DestMap.GetValuePtr(DestMap.AddUninitializedValue());
			DestMap.ValueProp->InitializeValue(DestChildValue);
			
			CopyProperty(SourceMap.ValueProp, SourceMap.GetValuePtr(Itr.GetInternalIndex()),
					DestMap.ValueProp, DestChildValue);
		}
		DestMap.Rehash();
	}
}


// This is a utility struct that's used to take an existing FPropertyChangedChainEvent, and remap its property chain to instead have the properties
// of a different class. In the process of remapping, it also resolves the data pointer of both the source property, and the resolved property.
struct FRemappedChainEvent : public FPropertyChangedChainEvent
{
public:
	FRemappedChainEvent(FPropertyChangedChainEvent& InEvent, const UObject* InSourceObject, const UObject* InObjectToResolve)
		: FPropertyChangedChainEvent(this->FRemappedChainEvent::PropertyChain, InEvent)
		, SourceEvent(InEvent)
		, SourceChainNode(nullptr)
		, ResolvedChainNode(nullptr)
		, SourceField(InSourceObject->GetClass())
		, ResolvedField(InObjectToResolve->GetClass())
		, SourceMemoryPtr(const_cast<UObject*>(InSourceObject))
		, ResolvedMemoryPtr(const_cast<UObject*>(InObjectToResolve))
		, SourceObject(InSourceObject)
		, ObjectToResolve(InObjectToResolve)
	{
		ResolvedArrayIndices.AddDefaulted(InEvent.ObjectIteratorIndex + 1);
		SetArrayIndexPerObject(ResolvedArrayIndices);

		// Iterate property chain, Remap fields, and resolve data pointers
		while(!ConstructionComplete())
		{
			if (UStruct* ResolvedStruct = ResolvedField.Get<UStruct>())
			{
				SourceChainNode = (SourceChainNode) ? SourceChainNode->GetNextNode() : SourceEvent.PropertyChain.GetActiveMemberNode();
				check(SourceChainNode);
				
				SourceField = SourceChainNode->GetValue();
				SourceMemoryPtr = SourceMemoryPtr ? SourceField.Get<FProperty>()->ContainerPtrToValuePtr<void>(SourceMemoryPtr) : nullptr;
				
				ResolvedChainNode = ResolveCurChainNode(ResolvedStruct, SourceField.Get<FProperty>());
				ResolvedField = ResolvedChainNode->GetValue();
				ResolvedMemoryPtr = ResolvedMemoryPtr ? ResolvedField.Get<FProperty>()->ContainerPtrToValuePtr<void>(ResolvedMemoryPtr) : nullptr;
			}
			else if (UObject* ResolvedObject = ResolvedField.Get<UObject>())
			{
				check(false);
			}
			else if (FArrayProperty* ResolvedArrayProperty = ResolvedField.Get<FArrayProperty>())
			{
				FArrayProperty* SourceArrayProperty = SourceField.Get<FArrayProperty>();
				int32 ArrayIndex = SourceEvent.GetArrayIndex(SourceArrayProperty->GetName());
				if (ArrayIndex == INDEX_NONE)
				{
					checkf(SourceChainNode->GetNextNode() == nullptr, TEXT("Expected this to be the last property because there's no index"));
					SourceField = nullptr;
					ResolvedChainNode = nullptr;
					ResolvedField = nullptr;
				}
				else
				{
					FScriptArrayHelper SourceArrayHelper(SourceArrayProperty, SourceMemoryPtr);
	                FScriptArrayHelper ResolvedArrayHelper(ResolvedArrayProperty, ResolvedMemoryPtr);
	                if(ResolvedMemoryPtr && !ResolvedArrayHelper.IsValidIndex(ArrayIndex))
	                {
						if (ChangeType & EPropertyChangeType::ArrayAdd)
						{
							check(ResolvedArrayHelper.Num() == ArrayIndex);
							ResolvedArrayHelper.Resize(ArrayIndex + 1);
						}
						else
						{
							ResolvedMemoryPtr = nullptr;
						}
	                }
	                ResolvedArrayIndices[SourceEvent.ObjectIteratorIndex].Add(ResolvedArrayProperty->GetName(), ArrayIndex);
	                SourceMemoryPtr = SourceMemoryPtr ? SourceArrayHelper.GetRawPtr(ArrayIndex) : nullptr;
	                ResolvedMemoryPtr = ResolvedMemoryPtr ? ResolvedArrayHelper.GetRawPtr(ArrayIndex) : nullptr;
	                SourceField = SourceArrayProperty->Inner;
	                ResolvedField = ResolvedArrayProperty->Inner;
				}
			}
			else if (FSetProperty* ResolvedSetProperty = ResolvedField.Get<FSetProperty>())
			{
				FSetProperty* SourceSetProperty = SourceField.Get<FSetProperty>();
                int32 SetIndex = SourceEvent.GetArrayIndex(SourceSetProperty->GetName());
                if (SetIndex == INDEX_NONE)
                {
                	checkf(SourceChainNode->GetNextNode() == nullptr, TEXT("Expected this to be the last property because there's no index"));
                	SourceField = nullptr;
                	ResolvedChainNode = nullptr;
                	ResolvedField = nullptr;
                }
                else
                {
                	FScriptSetHelper SourceSetHelper(SourceSetProperty, SourceMemoryPtr);
                	FScriptSetHelper ResolvedSetHelper(ResolvedSetProperty, ResolvedMemoryPtr);
                	if(ResolvedMemoryPtr && !ResolvedSetHelper.IsValidIndex(SetIndex))
                	{
						if (ChangeType & EPropertyChangeType::ArrayAdd)
						{
							check(ResolvedSetHelper.Num() == SetIndex);
							int32 AddedIndex = ResolvedSetHelper.AddUninitializedValue();
							check(AddedIndex == SetIndex);
						}
						else
						{
							ResolvedMemoryPtr = nullptr;
						}
                	}
                    ResolvedArrayIndices[SourceEvent.ObjectIteratorIndex].Add(ResolvedSetProperty->GetName(), SetIndex);
                	SourceMemoryPtr = SourceMemoryPtr ? SourceSetHelper.GetElementPtr(SetIndex) : nullptr;
                	ResolvedMemoryPtr = ResolvedMemoryPtr ? ResolvedSetHelper.GetElementPtr(SetIndex) : nullptr;
                	SourceField = SourceSetProperty->ElementProp;
                	ResolvedField = ResolvedSetProperty->ElementProp;
                }
			}
			else if (FMapProperty* ResolvedMapProperty = ResolvedField.Get<FMapProperty>())
			{
				FMapProperty* SourceMapProperty = SourceField.Get<FMapProperty>();
				int32 MapIndex = SourceEvent.GetArrayIndex(SourceMapProperty->GetName());
				if (MapIndex == INDEX_NONE)
				{
					checkf(SourceChainNode->GetNextNode() == nullptr, TEXT("Expected this to be the last property because there's no index"));
					SourceField = nullptr;
					ResolvedChainNode = nullptr;
					ResolvedField = nullptr;
				}
				else
				{
					FScriptMapHelper SourceMapHelper(SourceMapProperty, SourceMemoryPtr);
					FScriptMapHelper ResolvedMapHelper(ResolvedMapProperty, ResolvedMemoryPtr);
					if(ResolvedMemoryPtr && !ResolvedMapHelper.IsValidIndex(MapIndex))
					{
						if (ChangeType & EPropertyChangeType::ArrayAdd)
						{
							check(ResolvedMapHelper.Num() == MapIndex);
							int32 AddedIndex = ResolvedMapHelper.AddUninitializedValue();
							check(AddedIndex == MapIndex);
						}
						else
						{
							ResolvedMemoryPtr = nullptr;
						}
					}
	                ResolvedArrayIndices[SourceEvent.ObjectIteratorIndex].Add(ResolvedMapProperty->GetName(), MapIndex);
					SourceMemoryPtr = SourceMemoryPtr ? SourceMapHelper.GetValuePtr(MapIndex) : nullptr;
					ResolvedMemoryPtr = ResolvedMemoryPtr ? ResolvedMapHelper.GetValuePtr(MapIndex) : nullptr;
					SourceField = SourceMapProperty->ValueProp;
					ResolvedField = ResolvedMapProperty->ValueProp;
				}
			}
			else if (FStructProperty* ResolvedStructProperty = ResolvedField.Get<FStructProperty>())
			{
				FStructProperty* SourceStructProperty = SourceField.Get<FStructProperty>();
				SourceField = SourceStructProperty->Struct;
				ResolvedField = ResolvedStructProperty->Struct;
			}
			else if (FObjectProperty* ResolvedObjectProperty = ResolvedField.Get<FObjectProperty>())
			{
				UObject* ResolvedSubObject = GetObjectRefFromProperty(ResolvedObjectProperty, ResolvedMemoryPtr, ObjectToResolve);
				ResolvedField = ResolvedSubObject ? ResolvedSubObject->GetClass() : ResolvedObjectProperty->PropertyClass.Get();
				ResolvedMemoryPtr = ResolvedSubObject;
				
				FObjectProperty* SourceObjectProperty = SourceField.Get<FObjectProperty>();
				UObject* SourceSubObject = GetObjectRefFromProperty(SourceObjectProperty, SourceMemoryPtr, SourceObject);
				SourceField = SourceSubObject ? SourceSubObject->GetClass() : SourceObjectProperty->PropertyClass.Get();
				SourceMemoryPtr = SourceSubObject;
			}
			else if (FOptionalProperty* ResolvedOptionalProperty = ResolvedField.Get<FOptionalProperty>())
			{
				FOptionalProperty* SourceOptionalProperty = SourceField.Get<FOptionalProperty>();
				
				SourceField = SourceOptionalProperty->GetValueProperty();
            	ResolvedField = ResolvedOptionalProperty->GetValueProperty();
				if (SourceMemoryPtr)
				{
					if (SourceOptionalProperty->IsSet(SourceMemoryPtr))
					{
						SourceMemoryPtr = SourceOptionalProperty->GetValuePointerForReadOrReplace(SourceMemoryPtr);
					}
				
					if (ResolvedMemoryPtr)
					{
						if (!ResolvedOptionalProperty->IsSet(ResolvedMemoryPtr))
						{
							ResolvedMemoryPtr = ResolvedOptionalProperty->MarkSetAndGetInitializedValuePointerToReplace(ResolvedMemoryPtr);
						}
						else
						{
							ResolvedMemoryPtr = ResolvedOptionalProperty->GetValuePointerForReadOrReplace(ResolvedMemoryPtr);
						}
					}
				}
			}
			else
			{
				checkf(SourceChainNode->GetNextNode() == nullptr, TEXT("Expected this to be the last property in the chain because it's not a type known to contain properties"));
				SourceChainNode = nullptr;
				SourceField = nullptr;
				
				ResolvedChainNode = nullptr;
				ResolvedField = nullptr;
			}
			
			if (SourceEvent.Property == SourceField.Get<FProperty>())
			{
				Property = ResolvedField.Get<FProperty>();
			}
			if (SourceEvent.MemberProperty == SourceField.Get<FProperty>())
			{
				MemberProperty = ResolvedField.Get<FProperty>();
			}
			if (SourceEvent.PropertyChain.GetActiveNode() == SourceChainNode)
			{
				PropertyChain.SetActivePropertyNode(ResolvedChainNode->GetValue());
			}
			if (SourceEvent.PropertyChain.GetActiveMemberNode() == SourceChainNode)
			{
				PropertyChain.SetActiveMemberPropertyNode(ResolvedChainNode->GetValue());
			}
		}

		// For add changes, SourceField probably has one more array index that needs to be added. Add that here
		if (int32 AddChangeIndex = SourceEvent.GetArrayIndex(SourceField.GetName()); AddChangeIndex != INDEX_NONE)
		{
			ResolvedArrayIndices[SourceEvent.ObjectIteratorIndex].Add(ResolvedField.GetName(), AddChangeIndex);
		}
	}

	bool ConstructionComplete() const
	{
		if (FProperty* SourceProperty = SourceField.Get<FProperty>())
		{
			if (SourceEvent.GetArrayIndex(SourceProperty->GetName()) != INDEX_NONE && SourceProperty->Owner.Get<FProperty>() == SourceEvent.Property)
			{
				return false;
			}
			return SourceProperty == SourceEvent.Property;
		}
		check(SourceField.IsValid());
		check(ResolvedField.IsValid());
		return SourceEvent.PropertyChain.GetActiveMemberNode() == nullptr;
	}

	FEditPropertyChain::TDoubleLinkedListNode* ResolveCurChainNode(const UStruct* ResolvedStruct, const FProperty* SourceProperty)
	{
		FProperty* FoundProperty = FindPropertyByNameAndType(ResolvedStruct, SourceProperty);
		check(FoundProperty);
		
		PropertyChain.AddTail(FoundProperty);
		return PropertyChain.GetTail();
	}

	static UObject* GetObjectRefFromProperty(const FObjectProperty* Property, const void* Memory, const UObject* OwningObject)
	{
		if (!Memory)
		{
			return nullptr;
		}
		UObject* SubObject = Property->GetObjectPropertyValue(Memory);
		if (SubObject && UE::IsClassOfInstanceDataObjectClass(OwningObject->GetClass()))
		{
			if (UObject* FoundIdo = FPropertyBagRepository::Get().FindInstanceDataObject(SubObject))
			{
				SubObject = FoundIdo;
			}
		}
		return SubObject;
	}

	FEditPropertyChain PropertyChain;
	const FPropertyChangedChainEvent& SourceEvent;
	FEditPropertyChain::TDoubleLinkedListNode* SourceChainNode;
	
	FEditPropertyChain::TDoubleLinkedListNode* ResolvedChainNode;
	TArray<TMap<FString, int32>> ResolvedArrayIndices;
	
	FFieldVariant SourceField;
	FFieldVariant ResolvedField;
	
	void* SourceMemoryPtr;
	void* ResolvedMemoryPtr;
	const UObject* SourceObject;
	const UObject* ObjectToResolve;
};

void FPropertyBagRepository::PostEditChangeChainProperty(const UObject* Object, FPropertyChangedChainEvent& PropertyChangedEvent)
{
#if WITH_EDITOR
	static TSet<TSoftObjectPtr<UObject>> ChangeCallbacksToSkip;
	if (ChangeCallbacksToSkip.Remove(Object))
	{
		// avoids infinite recursion
		return;
	}
	
	auto CopyChanges = [&PropertyChangedEvent](const UObject* Source, UObject* Dest)
	{
		FRemappedChainEvent RemappedChainEvent(PropertyChangedEvent, Source, Dest);
		
		Dest->PreEditChange(RemappedChainEvent.PropertyChain);
		
		FProperty* SourceProperty = RemappedChainEvent.SourceField.Get<FProperty>();
		FProperty* DestProperty = RemappedChainEvent.ResolvedField.Get<FProperty>();
		if (SourceProperty && DestProperty && RemappedChainEvent.SourceMemoryPtr && RemappedChainEvent.ResolvedMemoryPtr)
		{
			CopyProperty(
				SourceProperty,
				RemappedChainEvent.SourceMemoryPtr,
				DestProperty,
				RemappedChainEvent.ResolvedMemoryPtr);
		}

            
		Dest->PostEditChangeChainProperty(RemappedChainEvent);
	};
	
	if (UObject* Ido = Get().FindInstanceDataObject(Object))
	{
		// if this object is an instance, modify it's IDO as well
		ChangeCallbacksToSkip.Add(Ido); // avoid infinite recursion
		CopyChanges(Object, Ido);
	}
	else if (UObject* Instance = const_cast<UObject*>(Get().FindInstanceForDataObject(Object)))
	{
		// if this object is an InstanceDataObject, modify it's owner as well
		ChangeCallbacksToSkip.Add(Instance); // avoid infinite recursion
		CopyChanges(Object, Instance);
	}
#endif
}

UObject* FPropertyBagRepository::DuplicateInstanceDataObject(UObject* SourceOwner, UObject* DestOwner)
{
	FPropertyBagRepositoryLock LockRepo(this);
	if (FPropertyBagAssociationData* SourceData = AssociatedData.Find(SourceOwner))
	{
		check(SourceData->InstanceDataObject);
		FPropertyBagAssociationData& DestData = AssociatedData.FindOrAdd(DestOwner);
		ensure(!DestData.InstanceDataObject);

		UObject* Outer = GetTransientPackage();

		// construct InstanceDataObject
		FStaticConstructObjectParameters Params(SourceData->InstanceDataObject->GetClass());
		Params.SetFlags |= EObjectFlags::RF_Transactional;
		Params.Name = MakeUniqueObjectName(Outer, Params.Class, DestOwner->GetFName(), EUniqueObjectNameOptions::UniversallyUnique);
		Params.Outer = Outer;
		DestData.InstanceDataObject = StaticConstructObject_Internal(Params);
		InstanceDataObjectToOwner.Add(DestData.InstanceDataObject, DestOwner);

		CopyTaggedProperties(SourceData->InstanceDataObject, DestData.InstanceDataObject);

		DestData.bNeedsFixup = SourceData->bNeedsFixup;
		return DestData.InstanceDataObject;
	}
	return nullptr;
}

void FPropertyBagRepository::PostLoadInstanceDataObject(const UObject* Owner)
{
	// fixups may have been applied to the instance during PostLoad and they need to be copied to its IDO
	FPropertyBagRepositoryLock LockRepo(this);
	if (FPropertyBagAssociationData* BagData = AssociatedData.Find(Owner))
	{
		if (BagData->InstanceDataObject)
		{
			// copy data from owner to IDO
			CopyTaggedProperties(Owner, BagData->InstanceDataObject);

			// the owner's PostLoad() may have mutated its instanced subobjects as well (e.g. pointer fixup). to handle
			// that case, we look for any instanced subobjects that have already had their PostLoad() called, as those will
			// not have a chance to get their IDO data fixed up to match changes potentially made by its owner's PostLoad().
			TArray<UObject*> InstancedSubObjects;
			constexpr bool bIncludeNestedObjects = false;
			GetObjectsWithOuter(Owner, InstancedSubObjects, bIncludeNestedObjects);
			for (UObject* InstancedSubObject : InstancedSubObjects)
			{
				if (!InstancedSubObject->HasAnyFlags(RF_NeedPostLoad))
				{
					PostLoadInstanceDataObject(InstancedSubObject);
				}
			}
		}
	}
}

// TODO: Remove this? Bag destruction to be handled entirely via UObject::BeginDestroy() (+ FPropertyBagProperty destructor)?
void FPropertyBagRepository::DestroyOuterBag(const UObject* Owner)
{
	FPropertyBagRepositoryLock LockRepo(this);
	RemoveAssociationUnsafe(Owner);
}

bool FPropertyBagRepository::RequiresFixup(const UObject* Object, bool bIncludeOuter) const
{
	FPropertyBagRepositoryLock LockRepo(this);

	const FPropertyBagAssociationData* BagData = AssociatedData.Find(Object);
	bool bResult = BagData ? BagData->bNeedsFixup : false;
	if (!bResult && bIncludeOuter)
	{
		ForEachObjectWithOuterBreakable(Object,
			[&bResult, this](UObject* Object) 
			{
				if (const FPropertyBagAssociationData* BagData = AssociatedData.Find(Object); 
					BagData && BagData->bNeedsFixup)
				{
					bResult = true;
					return false;
				}
				return true;
			}, true);
	}
	return bResult;
}


void FPropertyBagRepository::MarkAsFixedUp(const UObject* Object)
{
	FPropertyBagRepositoryLock LockRepo(this);
	if (FPropertyBagAssociationData* BagData = AssociatedData.Find(Object))
	{
		BagData->bNeedsFixup = false;
	}
}

bool FPropertyBagRepository::RemoveAssociationUnsafe(const UObject* Owner)
{
	const UStruct* OwnerAsTypeObject = Cast<const UStruct>(Owner);
	if (OwnerAsTypeObject && IsPropertyBagPlaceholderType(OwnerAsTypeObject))
	{
		FPropertyBagPlaceholderTypeRegistry::Get().Remove(OwnerAsTypeObject);
		return true;
	}

	FPropertyBagAssociationData OldData;
	if (AssociatedData.RemoveAndCopyValue(Owner, OldData))
	{
		InstanceDataObjectToOwner.Remove(OldData.InstanceDataObject);
		OldData.Destroy();
		return true;
	}
	return false;
}

bool FPropertyBagRepository::HasInstanceDataObject(const UObject* Object) const
{
	FPropertyBagRepositoryLock LockRepo(this);
	// May be lazily instantiated, but implied from existence of object data.
	return AssociatedData.Contains(Object);
}

UObject* FPropertyBagRepository::FindInstanceDataObject(const UObject* Object)
{
	FPropertyBagRepositoryLock LockRepo(this);
	const FPropertyBagAssociationData* BagData = AssociatedData.Find(Object);
	return BagData ? BagData->InstanceDataObject : nullptr;
}

const UObject* FPropertyBagRepository::FindInstanceDataObject(const UObject* Object) const
{
	return const_cast<FPropertyBagRepository*>(this)->FindInstanceDataObject(Object);
}

void FPropertyBagRepository::FindNestedInstanceDataObject(const UObject* Owner, bool bRequiresFixupOnly, TFunctionRef<void(UObject*)> Callback)
{
	FPropertyBagRepositoryLock LockRepo(this);

	if (const FPropertyBagAssociationData* BagData = AssociatedData.Find(Owner); 
		BagData && BagData->InstanceDataObject && (!bRequiresFixupOnly || BagData->bNeedsFixup))
	{
		Callback(BagData->InstanceDataObject);
	}

	ForEachObjectWithOuter(Owner,
		[this, bRequiresFixupOnly, Callback](UObject* Object)
		{
			if (const FPropertyBagAssociationData* BagData = AssociatedData.Find(Object);
				BagData && BagData->InstanceDataObject && (!bRequiresFixupOnly || BagData->bNeedsFixup))
			{
				Callback(BagData->InstanceDataObject);
			}
		}, true);
}

void FPropertyBagRepository::AddReferencedInstanceDataObject(const UObject* Object, FReferenceCollector& Collector)
{
	TObjectPtr<UObject> InstanceDataObject;
	{
		FPropertyBagRepositoryLock LockRepo(this);
		FPropertyBagAssociationData* BagData = AssociatedData.Find(Object);
		if (!BagData || !BagData->InstanceDataObject)
		{
			return;
		}
		InstanceDataObject = BagData->InstanceDataObject;
	}
	Collector.AddReferencedObject(InstanceDataObject, Object);
}

const UObject* FPropertyBagRepository::FindInstanceForDataObject(const UObject* InstanceDataObject) const
{
	FPropertyBagRepositoryLock LockRepo(this);
	const UObject* const* Owner = InstanceDataObjectToOwner.Find(InstanceDataObject);
	return Owner ? *Owner : nullptr;
}

UObject* FPropertyBagRepository::CreateInstanceDataObject(UObject* Owner, FArchive& Archive, int64 StartOffset, int64 EndOffset, bool bIsArchetype)
{
	// Limit the scope of the lock during to the find because this calls itself recursively to handle archetypes.
	if (FPropertyBagRepositoryLock LockRepo(this); FPropertyBagAssociationData* BagData = AssociatedData.Find(Owner))
	{
		check(BagData->InstanceDataObject);
		return BagData->InstanceDataObject;
	}

	// TODO: Should the IDO or its class be in another package?
	UObject* Outer = GetTransientPackage();

	// Construct the class for the IDO.
	TSharedPtr<FPropertyPathNameTree> PropertyTree = FUnknownPropertyTree(Owner).Find();
	const FUnknownEnumNames EnumNames(Owner);
	const FUnknownEnumNames* EnumNamesPtr = !EnumNames.IsEmpty() ? &EnumNames : nullptr;
	const UClass* InstanceDataObjectClass = CreateInstanceDataObjectClass(PropertyTree.Get(), EnumNamesPtr, Owner->GetClass(), Outer);

	// Find the template for the IDO.
	UObject* Template = nullptr;
	if (IsInstanceDataObjectArchetypeChainEnabled())
	{
		if (UObject* OwnerArchetype = Owner->GetArchetype())
		{
			if (OwnerArchetype == Owner->GetClass()->GetDefaultObject(false))
			{
				// if the archetype is a CDO, we can simply use the IDO's CDO because it's data should match.
				Template = InstanceDataObjectClass->GetDefaultObject(false);
			}
			else
			{
				// attempt to find/create an IDO for the archetype. Unfortunately if there's unkown properties involved, the types may not match.
				UObject* ArchetypeIdo = UE::CreateInstanceDataObject(OwnerArchetype);
				if (ArchetypeIdo && ArchetypeIdo->GetClass() && ArchetypeIdo->IsA(InstanceDataObjectClass))
				{
					Template = ArchetypeIdo;
				}
				else if (OwnerArchetype->GetArchetype())
				{
					// we need a archetype that is the same type as the IDO but has the same data as the owner's archetype
					FStaticConstructObjectParameters TemplateParams(InstanceDataObjectClass);
					TemplateParams.SetFlags |= EObjectFlags::RF_ArchetypeObject;
					TemplateParams.Outer = GetTransientPackage();
					Template = StaticConstructObject_Internal(TemplateParams);
					CopyTaggedProperties(OwnerArchetype, Template);
				}
			}
		}
	}

	// Construct the IDO.
	FStaticConstructObjectParameters Params(InstanceDataObjectClass);
	Params.SetFlags |= EObjectFlags::RF_Transactional;
	// Set the RF_ArchetypeObject flag on all IDO archetypes so that they can be identified as such.
	if (bIsArchetype || Owner->HasAllFlags(EObjectFlags::RF_ArchetypeObject))
	{
		Params.SetFlags |= EObjectFlags::RF_ArchetypeObject;
	}

	Params.Name = MakeUniqueObjectName(Outer, Params.Class, Owner->GetFName(), EUniqueObjectNameOptions::UniversallyUnique);
	Params.Outer = Outer;
	Params.Template = Template;

	UObject* InstanceDataObject = StaticConstructObject_Internal(Params);

	FPropertyBagRepositoryLock LockRepo(this);
	InstanceDataObjectToOwner.Add(InstanceDataObject, Owner);

	FPropertyBagAssociationData& BagData = AssociatedData.Add(Owner);
	BagData.InstanceDataObject = InstanceDataObject;
	BagData.bNeedsFixup = StructContainsLooseProperties(InstanceDataObjectClass);

	// Load the IDO.
	if (StartOffset != EndOffset)
	{
		// We want to force the template we used as the archetype during SerializeScriptProperties. For this reason 
		// we wrap the archive in a proxy that will return the correct archetype.
		FArchetypeMatchingArchive ArchiveWrapper(Archive, InstanceDataObject, Template);

		const int64 OffsetToRestore = ArchiveWrapper.Tell();
		FScopedObjectSerializeContext ObjectSerializeContext(InstanceDataObject, ArchiveWrapper);
		ArchiveWrapper.Seek(StartOffset);
		{
			FGuardValue_Bitfield(ArchiveWrapper.ArMergeOverrides, true);
			InstanceDataObject->SerializeScriptProperties(ArchiveWrapper);
		}
		ensureMsgf(ArchiveWrapper.Tell() == EndOffset,
			TEXT("Serializing %s into its IDO consumed %" INT64_FMT " bytes when %" INT64_FMT " bytes were expected."),
			*Owner->GetPathName(), ArchiveWrapper.Tell() - StartOffset, EndOffset - StartOffset);
		ArchiveWrapper.Seek(OffsetToRestore);
	}
	return BagData.InstanceDataObject;
}

// Not sure this is necessary.
void FPropertyBagRepository::ShrinkMaps()
{
	FPropertyBagRepositoryLock LockRepo(this);
	AssociatedData.Compact();
	InstanceDataObjectToOwner.Compact();
}

bool FPropertyBagRepository::IsPropertyBagPlaceholderType(const UStruct* Type)
{
	if (!Type)
	{
		return false;
	}

	return FPropertyBagPlaceholderTypeRegistry::Get().Contains(Type);
}

bool FPropertyBagRepository::IsPropertyBagPlaceholderObject(const UObject* Object)
{
	if (!Object)
	{
		return false;
	}

	return IsPropertyBagPlaceholderType(Object->GetClass());
}

namespace Private
{
#if WITH_EDITOR
	static bool bEnablePropertyBagPlaceholderObjectSupport = false;
	static FAutoConsoleVariableRef CVarEnablePropertyBagPlaceholderObjectSupport(
		TEXT("IDO.Placeholder.Enable"),
		bEnablePropertyBagPlaceholderObjectSupport,
		TEXT("If true, allows placeholder types to be created in place of missing types in order to redirect serialization into a property bag."),
		ECVF_Default
	);

	using EPlaceholderObjectFeature = UE::FPropertyBagRepository::EPlaceholderObjectFeature;
	constexpr uint32 PropertyBagPlaceholderObjectFeatureFlag(EPlaceholderObjectFeature Feature)
	{
		return 1 << static_cast<std::underlying_type_t<EPlaceholderObjectFeature>>(Feature);
	}

	static uint32 PropertyBagPlaceholderObjectEnabledFeaturesMask =
		PropertyBagPlaceholderObjectFeatureFlag(EPlaceholderObjectFeature::ReplaceMissingTypeImportsOnLoad);

	#define DEFINE_IDO_PLACEHOLDER_FEATURE_FLAG_CVAR(FEATURE) \
		static bool bEnablePropertyBagPlaceholderObjectFeature_##FEATURE = !!(PropertyBagPlaceholderObjectEnabledFeaturesMask & PropertyBagPlaceholderObjectFeatureFlag(EPlaceholderObjectFeature::FEATURE)); \
		static FAutoConsoleVariableRef CVarEnablePropertyBagPlaceholderObjectFeature_##FEATURE( \
			TEXT("IDO.Placeholder.Feature."#FEATURE), \
			bEnablePropertyBagPlaceholderObjectFeature_##FEATURE, \
			TEXT("Enable/disable IDO placeholder feature: "#FEATURE), \
			FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*) \
			{ \
				if (bEnablePropertyBagPlaceholderObjectFeature_##FEATURE) \
				{ \
					PropertyBagPlaceholderObjectEnabledFeaturesMask |= PropertyBagPlaceholderObjectFeatureFlag(EPlaceholderObjectFeature::FEATURE); \
				} \
				else \
				{ \
					PropertyBagPlaceholderObjectEnabledFeaturesMask &= ~PropertyBagPlaceholderObjectFeatureFlag(EPlaceholderObjectFeature::FEATURE); \
				} \
			}), \
			ECVF_Default \
		)

	DEFINE_IDO_PLACEHOLDER_FEATURE_FLAG_CVAR(ReplaceMissingTypeImportsOnLoad);
	DEFINE_IDO_PLACEHOLDER_FEATURE_FLAG_CVAR(SerializeExportReferencesOnLoad);
	DEFINE_IDO_PLACEHOLDER_FEATURE_FLAG_CVAR(ReplaceMissingReinstancedTypes);
	DEFINE_IDO_PLACEHOLDER_FEATURE_FLAG_CVAR(ReplaceDeadClassInstanceTypes);
#endif
}

bool FPropertyBagRepository::IsPropertyBagPlaceholderObjectSupportEnabled()
{
#if WITH_EDITOR && UE_WITH_OBJECT_HANDLE_TYPE_SAFETY
	return Private::bEnablePropertyBagPlaceholderObjectSupport && IsInstanceDataObjectSupportEnabled();
#else
	return false;
#endif
}

bool FPropertyBagRepository::IsPropertyBagPlaceholderObjectFeatureEnabled(EPlaceholderObjectFeature Feature)
{
#if WITH_EDITOR
	if (!IsPropertyBagPlaceholderObjectSupportEnabled())
	{
		return false;
	}

	return !!(Private::PropertyBagPlaceholderObjectEnabledFeaturesMask & Private::PropertyBagPlaceholderObjectFeatureFlag(Feature));
#else
	return false;
#endif
}

UStruct* FPropertyBagRepository::CreatePropertyBagPlaceholderType(UObject* Outer, UClass* Class, FName Name, EObjectFlags Flags, UStruct* SuperStruct)
{
	// Generate and link a new type object using the given SuperStruct as its base.
	UStruct* PlaceholderType = NewObject<UStruct>(Outer, Class, Name, Flags);
	PlaceholderType->SetSuperStruct(SuperStruct);
	PlaceholderType->Bind();
	PlaceholderType->StaticLink(/*bRelinkExistingProperties =*/ true);

	// Extra configuration needed for class types.
	if (UClass* PlaceholderTypeAsClass = Cast<UClass>(PlaceholderType))
	{
		// Create and configure its CDO as if it were loaded - for non-native class types, this is required.
		UObject* PlaceholderClassDefaults = PlaceholderTypeAsClass->GetDefaultObject();
		PlaceholderTypeAsClass->PostLoadDefaultObject(PlaceholderClassDefaults);

		// This class is for internal use and should not be exposed for selection or instancing in the editor.
		PlaceholderTypeAsClass->ClassFlags |= CLASS_Hidden | CLASS_HideDropDown;

		// Required by garbage collection for class types.
		PlaceholderTypeAsClass->AssembleReferenceTokenStream();
	}

	// Use the property bag repository for now to register property bag placeholder types for query purposes.
	// Note: Object lifetimes of this type and its instances depend on existing references that are serialized.
	FPropertyBagPlaceholderTypeRegistry::Get().Add(PlaceholderType);

	return PlaceholderType;
}

#if STATS
void FPropertyBagRepository::GatherStats(FPropertyBagRepositoryStats& Stats)
{
	FMemory::Memset(Stats, 0);

	Stats.NumPlaceholderTypes = FPropertyBagPlaceholderTypeRegistry::Get().Num();

	FPropertyBagRepositoryLock LockRepo(this);

	for (const TPair<const UObject*, FPropertyBagAssociationData>& Pair : AssociatedData)
	{ 
		const FPropertyBagAssociationData& BagData = Pair.Value;

		if (BagData.InstanceDataObject != nullptr)
		{
			++Stats.NumIDOs;

			FArchiveCountMem MemoryCount(BagData.InstanceDataObject);
			Stats.IDOMemoryBytes += MemoryCount.GetMax();

			if (BagData.bNeedsFixup)
			{
				++Stats.NumIDOsWithLooseProperties;
			}
		}
	}
}
#endif // STATS

} // UE

#endif // WITH_EDITORONLY_DATA
