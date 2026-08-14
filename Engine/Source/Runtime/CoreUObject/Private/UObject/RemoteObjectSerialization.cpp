// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/RemoteObjectSerialization.h"
#include "UObject/RemoteObject.h"
#include "UObject/RemoteObjectTransfer.h"
#include "UObject/RemoteObjectPrivate.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Class.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectMigrationContext.h"
#include "UObject/UnrealType.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Templates/Casts.h"
#include "Async/Async.h"
#include "UObject/ObjectHandlePrivate.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/ArchiveSerializedPropertyChain.h"

int32 GRemoteObjectsMigrateFullHierarchy = 1;
static FAutoConsoleVariableRef CVarRemoteObjectsMigrateFullHierarchy(
	TEXT("ro.MigrateFullHierarchy"),
	GRemoteObjectsMigrateFullHierarchy,
	TEXT("Whether remote objects that are default subobjects should be always migrated with their parent objects"));

int32 GResetBorrowedObjects = 1;
static FAutoConsoleVariableRef CVarResetBorrowedObjects(
	TEXT("ro.ResetBorrowedObjects"),
	GRemoteObjectsMigrateFullHierarchy,
	TEXT("Whether remote objects that were borrowed should be reset upon returning to their owner server instead of being reconstructed"));

int32 GUseImmutableArchetypes = 1;
static FAutoConsoleVariableRef CVarUseImmutableArchetypes(
	TEXT("ro.UseImmutableArchetypes"),
	GUseImmutableArchetypes,
	TEXT("Whether to use immutable archetypes when serializing remote object data"));

const UObject* FindImmutableArchetype(const UObject* InObj);

using FNameIndexType = FPackedRemoteObjectPathName::FNameIndexType;

DEFINE_LOG_CATEGORY_STATIC(LogRemoteSerialization, Log, All);

FArchive& operator<<(FArchive& Ar, FRemoteObjectBytes& Chunk)
{
	Ar << Chunk.Bytes;
	return Ar;
}

namespace UE::RemoteObject::Serialization::Disk
{

void SerializeNameTables(FArchive& Ar, const FRemoteObjectData& InObjectData)
{
	FNameIndexType NumNames = IntCastChecked<FNameIndexType>(InObjectData.Tables.Names.Num());
	Ar << NumNames;
	for (const FName& Name : InObjectData.Tables.Names)
	{
		FString NameString = Name.ToString();
		Ar << NameString;
	}
	Ar << const_cast<TArray<FRemoteObjectId>&>(InObjectData.Tables.RemoteIds);
	Ar << const_cast<TArray<FPackedRemoteObjectPathName>&>(InObjectData.PathNames);
}

void DeserializeNameTables(FArchive& Ar, FRemoteObjectData& OutObjectData)
{
	OutObjectData.Tables.Names.Reset();
	FNameIndexType NumNames = 0;
	Ar << NumNames;
	for (FNameIndexType NameIndex = 0; NameIndex < NumNames; ++NameIndex)
	{
		FString NameString;
		Ar << NameString;
		OutObjectData.Tables.Names.Add(FName(NameString, FNAME_Add));
	}
	Ar << OutObjectData.Tables.RemoteIds;
	Ar << OutObjectData.PathNames;
}

FString GenerateRemoteObjectFilename(FRemoteObjectId ObjectId, FRemoteServerId OwnerServerId)
{
	return FPaths::Combine(*FPaths::ProjectSavedDir(), *(FString::Printf(TEXT("%s-%s_%s.remote"), *UE::RemoteObject::GetGlobalServerId().ToString(), *ObjectId.ToString(), *OwnerServerId.ToString())));
}

void LoadObjectFromDisk(const FUObjectMigrationContext& MigrationContext)
{
	const FString Filename(GenerateRemoteObjectFilename(MigrationContext.ObjectId, MigrationContext.OwnerServerId));
	TUniquePtr<FArchive> FileReader{ IFileManager::Get().CreateFileReader(*Filename) };
	checkf(FileReader.IsValid(), TEXT("Unable to create file reader for remote object %s"), *MigrationContext.ObjectId.ToString());

	FRemoteObjectData ObjectData;	
	DeserializeNameTables(*FileReader, ObjectData);
	*FileReader << ObjectData.Bytes;
	FileReader->Close();
	IFileManager::Get().Delete(*Filename, false, true, true);

	// We have transferred ownership from the Database to the local server
	FRemoteServerId LocalServerId = UE::RemoteObject::GetGlobalServerId();
	const FRemoteServerId& DatabaseId = UE::RemoteObject::Transfer::DatabaseId;
	UE::RemoteObject::Transfer::OnObjectDataReceived(LocalServerId, LocalServerId, MigrationContext.ObjectId, DatabaseId, ObjectData);
}

void SaveObjectToDisk(const UE::RemoteObject::Transfer::FMigrateSendParams& Params)
{
	TUniquePtr<FArchive> FileWriter{ IFileManager::Get().CreateFileWriter(*GenerateRemoteObjectFilename(Params.MigrationContext.ObjectId, Params.MigrationContext.OwnerServerId)) };
	checkf(FileWriter.IsValid(), TEXT("Unable to create file writer for remote object %s"), *Params.MigrationContext.ObjectId.ToString());
	
	SerializeNameTables(*FileWriter, Params.ObjectData);
	*FileWriter << const_cast<TArray<FRemoteObjectBytes>&>(Params.ObjectData.Bytes);
	FileWriter->Close();
}

} // namespace UE::RemoteObject::Serialization::Disk

namespace UE::RemoteObject::Serialization
{

UObject* FindArchetype(const UObject* InObj)
{
	using namespace UE::CoreUObject::Private;

	if (GUseImmutableArchetypes)
	{
		bool bNativeObject = true;
		// No need to get the immutable CDO for BP class instance or their subobjects as BP classes are assets themselves and although their CDOs can still technically be modified at runtime
		// they never are because they can be GC'd and reset to their original state when they're reloaded so they're not a persistent storage like the native CDOs
		for (const UObject* OuterIt = InObj; OuterIt && bNativeObject; OuterIt = FObjectHandleUtils::GetNonAccessTrackedOuterNoResolve(OuterIt))
		{
			UClass* ObjectClass = OuterIt->GetClass();
			bNativeObject = !ObjectClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		}
		if (bNativeObject)
		{
			return const_cast<UObject*>(FindImmutableArchetype(InObj));
		}
	}
	return InObj->GetArchetype();
}

const FRemoteObjectConstructionParams* FRemoteObjectConstructionOverrides::Find(FName InName, UObject* InOuter) const
{
	const FRemoteObjectId OuterId(InOuter);
	// At the moment the number of serialized objects is usually pretty low (< 10) so no need for hash table lookups
	for (const FRemoteObjectConstructionParams& RemoteObjectParams : Overrides)
	{
		if (RemoteObjectParams.Name == InName && RemoteObjectParams.OuterId == OuterId)
		{
			return &RemoteObjectParams;
		}
	}
	return nullptr;
}

enum class ERemoteReferenceType : uint8
{
	None = 0,
	IdOnly = 1,
	PathName = 2
};

FArchive& operator<<(FArchive& Ar, ERemoteReferenceType& RefType)
{
	uint8 SerialziedType = (uint8)RefType;
	Ar << SerialziedType;
	RefType = (ERemoteReferenceType)SerialziedType;
	return Ar;
}

/**
* Structure that holds information about a reference to an object
* Helpful in avoiding calculating the same reference properties multiple times
*/
struct FRemoteObjectReferenceInfo
{
	UObject* Object = nullptr;
	FRemoteObjectId Id;
	ERemoteReferenceType Type = ERemoteReferenceType::None;
	bool bIsSubobject = false;
};

/**
* Base archive for serializing object data for migration
* Implements serialization debugging functionality (see UE_WITH_REMOTEOBJECTARCHIVE_DEBUGGING)
*/
template <typename T>
class TArchiveRemoteObjectBase : public T
{
protected:

	UObject* RootObject = nullptr;
	FRemoteObjectData& ObjectData;
	const FUObjectMigrationContext* MigrationContext = nullptr;
	TArray<uint8> SerializedBytes;
	FString ArchiveName;

#if UE_WITH_REMOTEOBJECTARCHIVE_DEBUGGING
	struct FMigratedPropertyStats
	{
		int64 Size = 0;
		int64 Count = 0;
	};

	FString SerializationScope;
	TMap<FString, TMap<FProperty*, FMigratedPropertyStats>> ObjectPropertyStats;

	void DumpStatsToLog()
	{
		int64 NameTableSize = 0;
		{
			TArray<uint8> NameTableData;
			FMemoryWriter NameTableWriter(NameTableData);
			UE::RemoteObject::Serialization::Disk::SerializeNameTables(NameTableWriter, ObjectData);
			NameTableSize = NameTableData.Num();
		}

		const int64 TotalSize = NameTableSize + ObjectData.GetNumBytes();

		UE_LOG(LogRemoteSerialization, Log, TEXT("%s Object Data stats for %s %s (Object Data toal: %d, total: %lld):"), *GetArchiveName(), *FRemoteObjectId(RootObject).ToString(), *GetFullNameSafe(RootObject), ObjectData.GetNumBytes(), TotalSize);
		UE_LOG(LogRemoteSerialization, Log, TEXT("  Name Table total size: %lld (Names: %d, RemoteIds: %d, Paths: %d)"), NameTableSize, ObjectData.Tables.Names.Num(), ObjectData.Tables.RemoteIds.Num(), ObjectData.PathNames.Num());

		TArray<FString> SortedPathNames;
		for (const FPackedRemoteObjectPathName& PathName : ObjectData.PathNames)
		{
			SortedPathNames.Add(PathName.ToString(ObjectData.Tables));
		}
		SortedPathNames.Sort();
		for (const FString& PathName : SortedPathNames)
		{
			UE_LOG(LogRemoteSerialization, Log, TEXT("    %s"), *PathName);
		}

		for (const TPair<FString, TMap<FProperty*, FMigratedPropertyStats>>& ObjectStatsPair : ObjectPropertyStats)
		{
			const FString& Obj = ObjectStatsPair.Key;
			const TMap<FProperty*, FMigratedPropertyStats>& PropStats = ObjectStatsPair.Value;

			int64 Total = 0;
			for (const TPair<FProperty*, FMigratedPropertyStats>& PropertyPair : PropStats)
			{
				const FMigratedPropertyStats& Stats = PropertyPair.Value;
				Total += Stats.Size;
			}

			UE_LOG(LogRemoteSerialization, Log, TEXT("  Data serialized for %s (Total: %lld):"), Obj.Len() ? *Obj : TEXT("Native Serialize"), Total);

			for (const TPair<FProperty*, FMigratedPropertyStats>& PropertyPair : PropStats)
			{
				FProperty* Prop = PropertyPair.Key;
				const FMigratedPropertyStats& Stats = PropertyPair.Value;
				UE_LOG(LogRemoteSerialization, Log, TEXT("    %s: size: %lld, count: %lld"), Prop ? *GetFullNameSafe(Prop) : TEXT("Native Serialize"), Stats.Size, Stats.Count);
			}
		}
	}
#endif // #if UE_WITH_REMOTEOBJECTARCHIVE_DEBUGGING

public:

	TArchiveRemoteObjectBase(FRemoteObjectData& InObjectData, const FUObjectMigrationContext* InContext, const TCHAR* InArchiveName)
		: T(SerializedBytes)
		, ObjectData(InObjectData)
		, MigrationContext(InContext)
		, ArchiveName(InArchiveName)
	{
		T::SetIsPersistent(false);
		T::SetUseUnversionedPropertySerialization(true);
		T::SetPortFlags(PPF_AvoidRemoteObjectMigration);

		if (T::IsLoading())
		{
			int32 NumSerializedBytes = InObjectData.GetNumBytes();
			SerializedBytes.Reserve(NumSerializedBytes);
			for (const FRemoteObjectBytes& Chunk : ObjectData.Bytes)
			{
				SerializedBytes.Append(Chunk.Bytes);
			}			
		}
	}

	virtual ~TArchiveRemoteObjectBase()
	{
		if (SerializedBytes.Num() > ObjectData.GetNumBytes())
		{
			ObjectData.Bytes.Empty();

			constexpr int32 MaxChunkSize = int32(TNumericLimits<uint16>::Max()) - 1;
			const uint8* RawBytes = SerializedBytes.GetData();
			int32 NumBytes = SerializedBytes.Num();
			int32 NumChunks = (NumBytes + MaxChunkSize - 1) / MaxChunkSize;
			ObjectData.Bytes.AddDefaulted(NumChunks);

			for (FRemoteObjectBytes& Chunk : ObjectData.Bytes)
			{
				int32 ChunkSize = FMath::Min(NumBytes, MaxChunkSize);
				if (ChunkSize > 0)
				{
					Chunk.Bytes.AddZeroed(ChunkSize);
					FMemory::Memcpy(Chunk.Bytes.GetData(), RawBytes, ChunkSize);
					RawBytes += ChunkSize;
					NumBytes -= ChunkSize;
				}
			}
		}
#if UE_WITH_REMOTEOBJECTARCHIVE_DEBUGGING
		DumpStatsToLog();
#endif
	}

	virtual FString GetArchiveName() const override
	{
		return ArchiveName;
	}

	virtual const FUObjectMigrationContext* GetMigrationContext() const override
	{
		return MigrationContext;
	}

	void SetRootObject(UObject* InRoot)
	{
		RootObject = InRoot;
	}

	UObject* GetRootObject() const
	{
		return RootObject;
	}

#if UE_WITH_REMOTEOBJECTARCHIVE_DEBUGGING
	void SetSerializationScope(const TCHAR* InScope)
	{
		SerializationScope = InScope;
	}

	virtual void Serialize(void* Data, int64 Num) override
	{
		FProperty* CurrentProperty = nullptr;
		if (const FArchiveSerializedPropertyChain* PropChain = T::GetSerializedPropertyChain())
		{
			if (PropChain->GetNumProperties())
			{
				CurrentProperty = PropChain->GetPropertyFromRoot(0);
			}
		}

		TMap<FProperty*, FMigratedPropertyStats>& ObjectStats = ObjectPropertyStats.FindOrAdd(SerializationScope);
		FMigratedPropertyStats& PropertyStats = ObjectStats.FindOrAdd(CurrentProperty);

		const int64 StartPos = T::Tell();

		T::Serialize(Data, Num);

		PropertyStats.Size += T::Tell() - StartPos;
		PropertyStats.Count++;
	}
#endif // UE_WITH_REMOTEOBJECTARCHIVE_DEBUGGING
};

class FArchiveRemoteObjectWriter : public TArchiveRemoteObjectBase<FMemoryWriter>
{
protected:

	TArray<UObject*> ObjectsToSerialize;
	TSet<UObject*> SerializedObjects;
	TMap<FName, FNameIndexType> NameMap;
	TMap<UObject*, FNameIndexType> PathNameMap;
	TMap<FRemoteObjectId, FNameIndexType> RemoteIdMap;
	TSet<UObject*>* ReferencedObjectsSet = nullptr;

public:

	static FRemoteObjectReferenceInfo GetReferenceInfo(const FObjectPtr& ObjPtr, UObject* InRootObject)
	{
		using namespace UE::RemoteObject;

		FRemoteObjectReferenceInfo Info;
		Info.Id = ObjPtr.GetRemoteId();
		if (ObjPtr)
		{
			if (!ObjPtr.IsRemote())
			{
				Info.Object = ObjPtr.Get();
				Info.bIsSubobject = Info.Object->IsIn(InRootObject);

				if (Info.Object == InRootObject || Info.bIsSubobject || (Info.Id.GetServerId().IsValid() && Info.Id.GetServerId() != GetGlobalServerId() && !Info.Id.IsAsset()))
				{
					Info.Type = ERemoteReferenceType::IdOnly;
				}
				else
				{

					Info.Type = ERemoteReferenceType::PathName;
				}
			}
			else
			{
				Info.Type = ERemoteReferenceType::IdOnly;
			}
		}
		return Info;
	}

	virtual bool PopulateObjectHeader(UObject* Object, FRemoteObjectHeader& OutHeader) const
	{
		using namespace UE::RemoteObject;
		using namespace UE::RemoteObject::Private;
		using namespace UE::CoreUObject::Private;

		OutHeader.Name = Object->GetFName();
		OutHeader.RemoteId = FObjectHandleUtils::GetRemoteId(Object);
		OutHeader.Class = Object->GetClass();
		UObject* Outer = FObjectHandleUtils::GetNonAccessTrackedOuterNoResolve(Object);
		OutHeader.Outer = Outer;
		OutHeader.Archetype = Object->GetArchetype();
		OutHeader.InternalFlags = (int32)(Object->GetInternalFlags() & EInternalObjectFlags::Garbage);

		return true;
	}

protected:

	virtual FRemoteObjectReferenceInfo GetReferenceInfo(const FObjectPtr& ObjPtr) const
	{
		return GetReferenceInfo(ObjPtr, RootObject);
	}

	virtual void WriteObjectReference(const FRemoteObjectReferenceInfo& RefInfo)
	{
		using namespace UE::CoreUObject::Private;

		*this << RefInfo.Type;
		// Always serialize unique id as objects may not exist on the other server and then we may need to pull them from this server
		FNameIndexType IdIndex = 0;
		if (RefInfo.Type != ERemoteReferenceType::None)
		{
			IdIndex = AddRemoteIdToIdMap(RefInfo.Id);
			*this << IdIndex;

			if (RefInfo.Type == ERemoteReferenceType::PathName)
			{
				FNameIndexType PathNameIndex = AddPathNameToNameMap(RefInfo.Object);
				*this << PathNameIndex;
			}
		}		
	}

	void WriteObjectPtr(const FObjectPtr& ObjPtr)
	{
		using namespace UE::CoreUObject::Private;

		FRemoteObjectReferenceInfo Info = GetReferenceInfo(ObjPtr);
		WriteObjectReference(Info);

		if (ObjPtr && !ObjPtr.IsRemote())
		{
			UObject* Object = ObjPtr.Get();

			// Anything can be marked as a remote reference, even assets in which case we rely on this flag to be set so that GC calls
			// StoreObjectToDatabase for any remotely referenced asset (and only for remotely referenced assets) that's about to be GC'd
			if (!Object->HasAnyInternalFlags(EInternalObjectFlags::RemoteReference))
			{
				if (ReferencedObjectsSet)
				{
					UE_AUTORTFM_OPEN
					{
						ReferencedObjectsSet->Add(Object);
					};
				}
			}

			if (Info.Type == ERemoteReferenceType::IdOnly && Info.bIsSubobject)
			{
				// Add subobjects of the root object to the list of objects to serialize
				ensureMsgf(Object->IsIn(RootObject), TEXT("We're about to serialize a subobject %s which is not a subobject of a root object %s"), *Object->GetPathName(), *RootObject->GetPathName());
				ObjectsToSerialize.Add(Object);
			}
		}
	}

public:

	FArchiveRemoteObjectWriter(UObject* InRootObject, FRemoteObjectData& OutObjectData, const FUObjectMigrationContext* InMigrationContext, const TCHAR* ArchiveName = nullptr, TSet<UObject*>* OutReferencedObjectsSet = nullptr)
		: TArchiveRemoteObjectBase<FMemoryWriter>(OutObjectData, InMigrationContext, ArchiveName ? ArchiveName : TEXT("RemoteObjectWriter"))
	{
		static_assert(sizeof(FNameIndexType) == sizeof(FPackedRemoteObjectPathName::FNameIndexType), "Name Index type must match the type used for storing remote object path name indices");

		SetRootObject(InRootObject);
		ObjectsToSerialize.Add(InRootObject);

		ReferencedObjectsSet = OutReferencedObjectsSet;
	}

	virtual UObject* GetArchetypeFromLoader(const UObject* Obj) override
	{
		return FindArchetype(Obj);
	}

	FNameIndexType AddNameToNameMap(const FName& Name)
	{
		FNameIndexType NameIndex = TNumericLimits<FNameIndexType>::Max();
		if (FNameIndexType* ExistingNameIndex = NameMap.Find(Name))
		{
			NameIndex = *ExistingNameIndex;
		}
		else
		{
			int32 NewIndex = ObjectData.Tables.Names.Add(Name);
			NameIndex = IntCastChecked<FNameIndexType>(NewIndex);
			NameMap.Add(Name, NameIndex);
		}
		return NameIndex;
	}

	FNameIndexType AddRemoteIdToIdMap(FRemoteObjectId RemoteId)
	{
		FNameIndexType IdIndex = TNumericLimits<FNameIndexType>::Max();
		if (FNameIndexType* ExistingIdIndex = RemoteIdMap.Find(RemoteId))
		{
			IdIndex = *ExistingIdIndex;
		}
		else
		{
			int32 NewIndex = ObjectData.Tables.RemoteIds.Add(RemoteId);
			IdIndex = IntCastChecked<FNameIndexType>(NewIndex);
			RemoteIdMap.Add(RemoteId, IdIndex);
		}
		return IdIndex;
	}

	FNameIndexType AddPathNameToNameMap(UObject* Object)
	{
		FNameIndexType PathNameIndex = TNumericLimits<FNameIndexType>::Max();
		if (FNameIndexType* ExistingPathNameIndex = PathNameMap.Find(Object))
		{
			PathNameIndex = *ExistingPathNameIndex;
		}
		else
		{
			int32 NewPathNameIndex = ObjectData.PathNames.AddDefaulted();
			PathNameIndex = IntCastChecked<FNameIndexType>(NewPathNameIndex);

			FPackedRemoteObjectPathName& NewPathName = ObjectData.PathNames[PathNameIndex];

			// Store individual indices of FNames of every object in this object's outer chain
			for (UObject* OuterChain = Object; OuterChain; OuterChain = UE::CoreUObject::Private::FObjectHandleUtils::GetNonAccessTrackedOuterNoResolve(OuterChain))
			{
				NewPathName.RemoteIds.Add(AddRemoteIdToIdMap(FRemoteObjectId(OuterChain)));
				NewPathName.Names.Add(AddNameToNameMap(OuterChain->GetFName()));
			}
			PathNameMap.Add(Object, PathNameIndex);
		}
		return PathNameIndex;
	}

	virtual FArchive& operator<<(UObject*& Obj) override
	{
		WriteObjectPtr(FObjectPtr(Obj));
		return *this;
	}

	virtual FArchive& operator<<(FName& Name) override
	{
		FNameIndexType NameIndex = AddNameToNameMap(Name);
		*this << NameIndex;
		return *this;
	}

	virtual FArchive& operator<<(FLazyObjectPtr& Value) override { return FArchiveUObject::SerializeLazyObjectPtr(*this, Value); }
	virtual FArchive& operator<<(FObjectPtr& Value) override 
	{ 
		WriteObjectPtr(Value);
		return *this;
	}
	virtual FArchive& operator<<(FSoftObjectPtr& Value) override { return FArchiveUObject::SerializeSoftObjectPtr(*this, Value); }
	virtual FArchive& operator<<(FSoftObjectPath& Value) override { return FArchiveUObject::SerializeSoftObjectPath(*this, Value); }
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override
	{
		FObjectPtr Ptr;
#if UE_WITH_REMOTE_OBJECT_HANDLE
		const FRemoteObjectId WeakPtrId = Value.GetRemoteId();
		if (UE::RemoteObject::Handle::IsRemote(WeakPtrId))
		{
			Ptr = FObjectPtr(WeakPtrId);
		}
		else
#endif
		{
			Ptr = Value.Get(/** bEvenIfGarbage */ true);
		}
		WriteObjectPtr(Ptr);

		return *this;
	}

	TArray<UObject*>& GetObjectsToSerialize()
	{
		return ObjectsToSerialize;
	}

	FORCENOINLINE void SerializeRemoteObject(UObject* Object, const FRemoteObjectHeader& Header)
	{
		AddNameToNameMap(Header.Name);
		Object->Serialize(*this);
	}
};

template <typename T>
struct TRemoteObjectArchiveScope
{
	TArchiveRemoteObjectBase<T>& Ar;

	TRemoteObjectArchiveScope(TArchiveRemoteObjectBase<T>& InAr, const TCHAR* InScope)
		: Ar(InAr)
	{
#if UE_WITH_REMOTEOBJECTARCHIVE_DEBUGGING
		Ar.SetSerializationScope(InScope);
#endif
	}
	TRemoteObjectArchiveScope(TArchiveRemoteObjectBase<T>& InAr, UObject* InObjectScope)
		: Ar(InAr)
	{
#if UE_WITH_REMOTEOBJECTARCHIVE_DEBUGGING
		UObject* Root = Ar.GetRootObject();
		UObject* RootOuter = UE::CoreUObject::Private::FObjectHandleUtils::GetNonAccessTrackedOuterNoResolve(Root);
		Ar.SetSerializationScope(*InObjectScope->GetFullName(RootOuter));
#endif
	}
	~TRemoteObjectArchiveScope()
	{
#if UE_WITH_REMOTEOBJECTARCHIVE_DEBUGGING
		Ar.SetSerializationScope(nullptr);
#endif
	}
};

using FRemoteObjectWriterScope = TRemoteObjectArchiveScope<FMemoryWriter>;
using FRemoteObjectReaderScope = TRemoteObjectArchiveScope<FMemoryReader>;

/**
* Helper archive that serializes the difference between archetypes and their instances
* This is achieved using delta serialization but the data we serialize against is coming from instances of the archetypes
* Effectively this is the opposite how delta serialization normally works which serializes instances of archetypes agains the archetypes
* In other words this archive is used to serialize archetypes, not their instances.
*/
class FArchetypeDeltaWriter : public FArchiveRemoteObjectWriter
{
	TMap<const UObject*, UObject*>& ArchetypeToInstanceMap;

public:

	FArchetypeDeltaWriter(UObject* InRootObject, FRemoteObjectData& OutObjectData, TMap<const UObject*, UObject*>& InArchetypeToInstanceMap)
		: FArchiveRemoteObjectWriter(InRootObject, OutObjectData, /*MigrationContext*/ nullptr, TEXT("RemoteArchetypeDeltaWriter"))
		, ArchetypeToInstanceMap(InArchetypeToInstanceMap)
	{
	}

	virtual UObject* GetArchetypeFromLoader(const UObject* Obj) override
	{
		// Since this archive serializes the archetype we want the archetype of that archetype to be its instance (effectively reversing the object -> archetype relationship)
		// this way we will delta serialize the difference between the archetype and its instance
		if (UObject** FoundArchetypeInstance = ArchetypeToInstanceMap.Find(Obj))
		{
			return *FoundArchetypeInstance;
		}
		UE_LOG(LogRemoteSerialization, Warning, TEXT("FArchetypeDeltaWriter::ArchetypeToInstanceMap does not contain an archetype mapping for %s"), *GetPathNameSafe(Obj));
		return FArchiveRemoteObjectWriter::GetArchetypeFromLoader(Obj);
	}

protected:

	virtual void WriteObjectReference(const FRemoteObjectReferenceInfo& RefInfo) override
	{
		using namespace UE::CoreUObject::Private;

		FRemoteObjectReferenceInfo ReplacementInfo(RefInfo);

		// If we're serializing a reference to an archetype try to replace it with a reference to its instance
		// This way the produced delta between the archetype and its instance will be correctly pointing to the instances of default subobjects
		// we can then deserialize over the archetype instance.
		if (UObject** Instance = ArchetypeToInstanceMap.Find(RefInfo.Object))
		{
			ReplacementInfo.Object = *Instance;
			ReplacementInfo.Id = FRemoteObjectId(*Instance);
		}

		FArchiveRemoteObjectWriter::WriteObjectReference(ReplacementInfo);
	}

public:

	virtual bool PopulateObjectHeader(UObject* Object, FRemoteObjectHeader& OutHeader) const override
	{
		// As per comment in WriteObjectReference - when serializing archetype data pretend we're actually serializing references to instances of the archetype 
		// so substitute the archetype object with its instance when serializing the object header
		if (UObject** Instance = ArchetypeToInstanceMap.Find(Object))
		{
			Object = *Instance;
		}

		// If for some reason we failed to substitute the archetype with its instance or for some other reason Object is an asset, skip it
		bool bCanSaveObject = !FRemoteObjectId(Object).IsAsset();
		if (bCanSaveObject)
		{
			bCanSaveObject = FArchiveRemoteObjectWriter::PopulateObjectHeader(Object, OutHeader);
		}
		
		return bCanSaveObject;
	}
};


FArchive& operator<<(FArchive& Ar, FRemoteObjectHeader& Header)
{
	Ar << Header.Name;
	Ar << Header.RemoteId;
	Ar << Header.Class;
	Ar << Header.Outer;
	Ar << Header.Archetype;
	Ar << Header.InternalFlags;

	return Ar;
}

class FArchiveRemoteObjectReader : public TArchiveRemoteObjectBase<FMemoryReader>
{
	const TArray<FName>& Names;
	const TArray<FRemoteObjectId>& RemoteIds;
	const TArray<UObject*>& ResolvedPathNameObjects;
	const ERemoteObjectSerializationFlags DeserializeFlags = ERemoteObjectSerializationFlags::None;

	ERemoteReferenceType ReadObjectReference(FObjectPtr& Value)
	{
		ERemoteReferenceType Type;		
		*this << Type;

		if (Type != ERemoteReferenceType::None)
		{
			FRemoteObjectId ObjId;
			{
				FNameIndexType IdIndex;
				*this << IdIndex;
				ObjId = RemoteIds[IdIndex];
			}

			bool bNeedsResolvingWithId = true;
			if (Type == ERemoteReferenceType::PathName)
			{
				FNameIndexType PathNameIndex = TNumericLimits<FNameIndexType>::Max();
				*this << PathNameIndex;

				// In some situations (like resetting an object to its archetype state) we might want to preserve
				// references to remote objects because we might end up migrating them mid-deserialization 
				// Overwriting them could also potentially discard any changes made to them on another server
				if (EnumHasAnyFlags(DeserializeFlags, ERemoteObjectSerializationFlags::PreserveRemoteReferences) && Value.IsRemote())
				{
					bNeedsResolvingWithId = false;
				}
				else
				{
					// Try to resolve path name immediately as we expect the object to exist in memory.
					UObject* Obj = ResolvedPathNameObjects[PathNameIndex];
					if (Obj)
					{
						Obj->SetInternalFlags(EInternalObjectFlags::RemoteReference);
						Value = FObjectPtr(Obj);
						bNeedsResolvingWithId = false;
					}
				}
			}
			if (bNeedsResolvingWithId)
			{
				// If the serialized reference was not found in memory or if the reference was serialized as id-only 
				// keep it as an unresolved ObjectPtr and store a pointer to it so that we can try to resolve it after all objects have been deserialize
#if UE_WITH_REMOTE_OBJECT_HANDLE
				if (!EnumHasAnyFlags(DeserializeFlags, ERemoteObjectSerializationFlags::PreserveRemoteReferences) || !Value.IsRemote())
				{
					FObjectHandle Handle = FObjectHandle::FromIdNoResolve(ObjId);
					Value = FObjectPtr(Handle);
				}
#endif // UE_WITH_REMOTE_OBJECT_HANDLE
			}
		}
		else
		{
			Value = FObjectPtr();
		}
		return Type;
	}

public:

	/**
	 * @param InObjectData:  The serialized object data we are should deserialize
	 * @param InResolvedPathNames:  The existing Resolved Objects that corresponds to InObjectData.PathNames
	 * @param bInAssignedOwnership:  We must take ownership of the objects we deserialize. We could already have ownership of those objects.
	 * @param InDeserializeFlags:  The flags for how we should treat references during deserialization.
	 */
	FArchiveRemoteObjectReader(FRemoteObjectData& InObjectData, const TArray<UObject*>& InResolvedPathNames, const FUObjectMigrationContext* InMigrationContext, ERemoteObjectSerializationFlags InDeserializeFlags)
		: TArchiveRemoteObjectBase<FMemoryReader>(InObjectData, InMigrationContext, TEXT("RemoteObjectReader"))
		, Names(InObjectData.Tables.Names)
		, RemoteIds(InObjectData.Tables.RemoteIds)
		, ResolvedPathNameObjects(InResolvedPathNames)
		, DeserializeFlags(InDeserializeFlags)
	{
	}

	virtual FArchive& operator<<(UObject*& Obj) override
	{
		FObjectPtr Value;
		ReadObjectReference(Value);
		check(Value.IsResolved());
		Obj = Value.Get();
		return *this;
	}

	virtual FArchive& operator<<(FName& Name) override
	{
		FNameIndexType NameIndex = 0;
		*this << NameIndex;
		Name = Names[NameIndex];
		return *this;
	}

	virtual FArchive& operator<<(FLazyObjectPtr& Value) override { return FArchiveUObject::SerializeLazyObjectPtr(*this, Value); }
	virtual FArchive& operator<<(FObjectPtr& Value) override 
	{
		ReadObjectReference(Value);
		return *this;
	}

	virtual FArchive& operator<<(FSoftObjectPtr& Value) override { return FArchiveUObject::SerializeSoftObjectPtr(*this, Value); }
	virtual FArchive& operator<<(FSoftObjectPath& Value) override { return FArchiveUObject::SerializeSoftObjectPath(*this, Value); }
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override 
	{
		FObjectPtr Ptr;
		ReadObjectReference(Ptr);

		if (Ptr.IsResolved())
		{
			Value = Ptr.Get();
		}
#if UE_WITH_REMOTE_OBJECT_HANDLE
		else
		{
			Value = FWeakObjectPtr(Ptr.GetRemoteId());
		}
#endif // UE_WITH_REMOTE_OBJECT_HANDLE

		return *this;
	}
};

/**
* Archive that replaces unresolved FObjectPtrs (TObjectPtrs<>) with actual pointers to deserialized objects
*/
class FArchiveRemoteReferencePatcher : public FArchiveUObject
{
	TMap<FRemoteObjectId, UObject*> IdToObjectMap;

	void InitObjectMap(const TArray<UObject*>& DeserializedObjects)
	{
		for (UObject* Obj : DeserializedObjects)
		{
			IdToObjectMap.Add(FRemoteObjectId(Obj), Obj);
		}
	}

	void PatchObjectReference(FObjectPtr& Value)
	{
#if UE_WITH_REMOTE_OBJECT_HANDLE
		using namespace UE::RemoteObject::Handle;
		const FRemoteObjectStub* Stub = Value.GetHandleRef().ToStub();
		if (UObject** ResolvedObject = IdToObjectMap.Find(Stub->Id))
		{
			Value = FObjectPtr(*ResolvedObject);
		}
#endif
	}

public:

	FArchiveRemoteReferencePatcher(const TArray<UObject*>& DeserializedObjects)
	{
		ArIsObjectReferenceCollector = true;
		SetIsPersistent(false);
		InitObjectMap(DeserializedObjects);
		SetPortFlags(PPF_AvoidRemoteObjectMigration);
	}

	virtual FArchive& operator<<(FObjectPtr& Value) override
	{
		if (!Value.IsResolved())
		{
			PatchObjectReference(Value);
		}
		return *this;
	}
};


class FArchiveSubObjectGatherer : public FArchiveUObject
{
	UObject* RootObject = nullptr;
	TMap<const UObject*, UObject*>& ArchetypeToObjectMap;
	TArray<UObject*>& ObjectsToSerialize;

public:

	FArchiveSubObjectGatherer(UObject* InRootObject, TMap<const UObject*, UObject*>& InArchetypeToObjectMap, TArray<UObject*>& InObjectsToSerialize)
		: RootObject(InRootObject)
		, ArchetypeToObjectMap(InArchetypeToObjectMap)
		, ObjectsToSerialize(InObjectsToSerialize)
	{
		ArchetypeToObjectMap.Add(FindArchetype(RootObject), RootObject);
		ArIsObjectReferenceCollector = true;
		SetIsPersistent(false);
		SetPortFlags(PPF_AvoidRemoteObjectMigration);
	}

	virtual FArchive& operator<<(FObjectPtr& Value) override
	{
		FRemoteObjectReferenceInfo Info = FArchiveRemoteObjectWriter::GetReferenceInfo(Value, RootObject);
		if (Info.bIsSubobject)
		{
			ArchetypeToObjectMap.Add(FindArchetype(Info.Object), Info.Object);
			ObjectsToSerialize.Add(Info.Object);
		}
		return *this;
	}
};

FRemoteObjectConstructionOverrides::FRemoteObjectConstructionOverrides(const TArray<FRemoteObjectHeader>& InObjectHeaders)
{
	using namespace UE::RemoteObject::Handle;
	using namespace UE::RemoteObject::Private;

	Overrides.Reserve(InObjectHeaders.Num());
	for (const FRemoteObjectHeader& Header : InObjectHeaders)
	{
		FRemoteObjectConstructionParams& Params = Overrides.Emplace_GetRef();
		Params.Name = Header.Name;
#if UE_WITH_REMOTE_OBJECT_HANDLE
		Params.OuterId = GetRemoteObjectId(Header.Outer.GetHandle());
#endif
		Params.RemoteId = Header.RemoteId;
		Params.SerialNumber = Header.SerialNumber;
	}
}

UObject* ConstructRemoteObject(const FRemoteObjectHeader& Header, FRemoteObjectConstructionOverrides* ConstructionOverrides, ERemoteObjectSerializationFlags DeserializeFlags)
{
	using namespace UE::RemoteObject::Handle;
	using namespace UE::RemoteObject::Private;

	UClass* Class = CastChecked<UClass>(Header.Class.Get());
	UObject* Outer = nullptr;
#if UE_WITH_REMOTE_OBJECT_HANDLE
	if (Header.Outer.IsResolved())
	{
		Outer = Header.Outer.Get();
	}
	else
	{
		Outer = StaticFindObjectFastInternal(Header.Outer.GetRemoteId());
		UE_CLOG(!Outer, LogRemoteSerialization, Fatal, TEXT("Failed to resolve an Outer when constructing remote object"))
	}
#endif
	FName Name = Header.Name;

	// The object may already exist in memory (it could be a default subobject of an object we've just created)
	UObject* Object = StaticFindObjectFast(Class, Outer, Name);
	if (Object)
	{
		if (FRemoteObjectId(Object) != Header.RemoteId)
		{
			UE_LOG(LogRemoteSerialization, Warning, TEXT("Received remote object %s with identical pathname (%s) as a local object %s. Remote object will be renamed."), *Header.RemoteId.ToString(), *Object->GetPathName(), *FRemoteObjectId(Object).ToString());
			Name = FName();
			Object = nullptr;
		}
	}
	else
	{
		Object = StaticFindObjectFastInternal(Header.RemoteId);
		if (Object)
		{
			// The object already exists on this server but has been renamed
			Name = Object->GetFName();
		}
	}

	// If not or the object is marked as remote (which means we brought it back before it was GC'd) (re)construct it
	// Unless we explcitly want to re-use existing (valid / not marked as garbage) objects and skip re-construction to avoid side-effects
	// Note that even then an object may not exist on this server (it could've been constructed on a different server when its owner was migrated or was simply GC'd)
	const bool bSkipConstruction = IsValid(Object) && (!IsRemote(Object) || EnumHasAnyFlags(DeserializeFlags, ERemoteObjectSerializationFlags::UseExistingObjects));
	if (!bSkipConstruction)
	{
		FStaticConstructObjectParameters Params(Class);
		Params.Outer = Outer;
		Params.Name = Name;
		Params.SerialNumber = Header.SerialNumber;
		Params.Template = Header.Archetype.Get();
#if UE_WITH_REMOTE_OBJECT_HANDLE
		Params.RemoteId = Header.RemoteId;
		Params.RemoteSubObjectOverrides = ConstructionOverrides;
#endif

		{
			// In case we're allocating on top of existing object that's marked as remote don't try to resolve any of its references since they all are going to be destroyed anyway
			FUnsafeToMigrateScope UnsafeToMigrateScope;

			// Using StaticConstructObject_Internal to pass the extra parameters (RemoteId and SubobjectOverrides) which are not exposed to normal APIs
			Object = StaticConstructObject_Internal(Params);
		}
	}

	if (!Object)
	{
		return nullptr;
	}

	checkf(FRemoteObjectId(Object) == Header.RemoteId, TEXT("Created an object with a different ID:%s than requested:%s"), *FRemoteObjectId(Object).ToString(), *Header.RemoteId.ToString());

	MarkAsLocal(Object);

	// Update internal flags on the migrated object. It's possible the object being migrated already existed in memory on this server and had the EInternalObjectFlags::Garbage flag set.
	// Unless the migrated version also had this flag set we need to clear it.
	// It's also possible that the local object didn't have this flag set but the migrated one has so we need to set it on this server too (it's not impossible to migrate objects marked as garbage)
	EInternalObjectFlags InternalFlags = (EInternalObjectFlags)Header.InternalFlags;
	// Clearing and setting the garbage flag needs to happen through dedicated functions
	if (!(InternalFlags & EInternalObjectFlags::Garbage))
	{
		Object->ClearGarbage();
	}
	else
	{
		Object->MarkAsGarbage();
		InternalFlags &= ~EInternalObjectFlags::Garbage;
	}
	// Any other internal flags can be set with SetInternalFlags 
	if (InternalFlags != EInternalObjectFlags::None)
	{
		Object->SetInternalFlags(InternalFlags);
	}

	return Object;
}

void ResolvePathNames(const FRemoteObjectData& InObjectData, TArray<UObject*>& OutResolvedObjects)
{	
	for (const FPackedRemoteObjectPathName& PathName : InObjectData.PathNames)
	{
		UObject* Object = PathName.Resolve(InObjectData.Tables);
		OutResolvedObjects.Add(Object);
	}
}

void SerializeObjectDataInternal(FArchiveRemoteObjectWriter& Ar, UObject* RequestedObject, FRemoteObjectId RequestedObjectId, TSet<UObject*>& OutObjects)
{
	using namespace UE::RemoteObject::Handle;

	TArray<FRemoteObjectHeader> ObjectHeaders;
	int32 Version = 0;
	int64 HeaderOffset = 0;

	int64 OffsetOfHeaderOffset = 0;
	{
		FRemoteObjectWriterScope Scope(Ar, TEXT("Header"));
		Ar << Version;
		Ar << RequestedObjectId;
		OffsetOfHeaderOffset = Ar.Tell();
		Ar << HeaderOffset;
	}

	int32 SerializedObjectIndex = 0;
	bool bSerializedRequestedObject = false;

	TSet<UObject*> ProcessedObjects;
	do
	{
		for (; SerializedObjectIndex < Ar.GetObjectsToSerialize().Num(); ++SerializedObjectIndex)
		{
			UObject* ObjectToSerialize = Ar.GetObjectsToSerialize()[SerializedObjectIndex];
			if (!ProcessedObjects.Contains(ObjectToSerialize))
			{
				ProcessedObjects.Add(ObjectToSerialize);

				FRemoteObjectWriterScope Scope(Ar, ObjectToSerialize);				
				FRemoteObjectHeader Header;
				if (Ar.PopulateObjectHeader(ObjectToSerialize, Header))
				{					
					ObjectHeaders.Add(Header);
					OutObjects.Add(ObjectToSerialize);

					Ar.SerializeRemoteObject(ObjectToSerialize, ObjectHeaders.Last());

					ObjectHeaders.Last().NextOffset = Ar.Tell();
				}
				else
				{
					UE_LOG(LogRemoteSerialization, Warning, TEXT("Unable to serialize object (asset: %s) %s"), 
						FRemoteObjectId(ObjectToSerialize).IsAsset() ? TEXT("yes") : TEXT("no"),
						*ObjectToSerialize->GetPathName());
				}
			}
		}

		bSerializedRequestedObject = OutObjects.Contains(RequestedObject);
		if (!bSerializedRequestedObject)
		{
			checkf(!ProcessedObjects.Contains(RequestedObject), TEXT("%s couldn't be serialized"), *RequestedObject->GetPathName());

			// InObject was a default subobject (see GRemoteObjectsMigrateFullHierarchy) but when we serialized its parent 
			// it turned out that the parent had no direct reference to InObject in which case we need to manually add InObject to ObjectsToSerialize list
			Ar.GetObjectsToSerialize().Add(RequestedObject);
		}
	} while (!bSerializedRequestedObject);

	HeaderOffset = Ar.Tell();
	Ar.Seek(OffsetOfHeaderOffset);
	Ar << HeaderOffset;
	Ar.Seek(HeaderOffset);

	{
		FRemoteObjectWriterScope Scope(Ar, TEXT("ObjectHeaders"));
		Ar << ObjectHeaders;
	}
}

FRemoteObjectData SerializeObjectData(UObject* InObject, TSet<UObject*>& OutObjects, TSet<UObject*>& OutReferencedObjects, const FUObjectMigrationContext* MigrationContext)
{
	UObject* Object = InObject;
	if (GRemoteObjectsMigrateFullHierarchy)
	{
		Object = FindCanonicalRootObjectForSerialization(Object);
	}

	FRemoteObjectData ObjectData;
	FRemoteObjectId RequestedObjectId(InObject);

	{
		FArchiveRemoteObjectWriter Ar(Object, ObjectData, MigrationContext, nullptr, &OutReferencedObjects);
		Ar.SetMigratingRemoteObjects(true);
		SerializeObjectDataInternal(Ar, InObject, RequestedObjectId, OutObjects);
	}

	return MoveTemp(ObjectData);
}

void ResetRemoteObject(UObject* InObject)
{
	UObject* Object = InObject;
	if (GRemoteObjectsMigrateFullHierarchy)
	{
		Object = FindCanonicalRootObjectForSerialization(Object);
	}

	TMap<const UObject*, UObject*> ReverseArchetypeToObjectMap;

	// Serialize Object to gather its subobjects and map the subobject archetypes to their respective instances
	// This map will also be used to replace archetype object pathnames in the serialized archetype data to their instances' pathnames
	{
		TArray<UObject*> ObjectsToSerialize;
		TSet<UObject*> SerializedObjects;
		FArchiveSubObjectGatherer SubobjectGather(Object, ReverseArchetypeToObjectMap, ObjectsToSerialize);
		ObjectsToSerialize.Add(Object);

		for (int32 Index = 0; Index < ObjectsToSerialize.Num(); ++Index)
		{
			UObject* Obj = ObjectsToSerialize[Index];
			if (!SerializedObjects.Contains(Obj))
			{
				Obj->Serialize(SubobjectGather);
				SerializedObjects.Add(Obj);
			}
		}
	}

	FRemoteObjectData* ArchetypeDelta = new FRemoteObjectData();
	{
		// Serialize the Object Archetype against the Object (and its subobjects)
		// This will produce a delta between the archetypes and their instances which will then be used to restore the instances' state to the archetypes'
		FRemoteObjectId RequestedObjectId(Object);
		UObject* Archetype = const_cast<UObject*>(FindArchetype(Object));
		FArchetypeDeltaWriter Ar(Archetype, *ArchetypeDelta, ReverseArchetypeToObjectMap);
		TSet<UObject*> ArchetypeSerializedObjects;
		SerializeObjectDataInternal(Ar, Archetype, RequestedObjectId, ArchetypeSerializedObjects);
	}

	{
		// Deserialize archetype data on top of the object (and its subobjects) to restore their state to the archetype values
		TArray<UObject*> DeserializedObjects;
		TArray<FRemoteObjectId> DeserializedIds;
		// When deserializing archetype delta we want to:
		// Preserve any references to remote objects that have not been migrated yet (this is because we can't generate archetype delta for them because they don't exist on this server and we don't have their data)
		// Use any existing objects and deserialize archetype delta on top of them avoiding re-construction which may lead to undiserable side-effects
		// Additionally we don't want to recursively re-enter this function so let the deserialization process know we're already resetting migrated object(s)
		ERemoteObjectSerializationFlags DeserializationFlags = ERemoteObjectSerializationFlags::PreserveRemoteReferences | ERemoteObjectSerializationFlags::UseExistingObjects | ERemoteObjectSerializationFlags::Resetting;
		DeserializeObjectData(*ArchetypeDelta, /*MigrationContext*/ nullptr, DeserializedIds, DeserializedObjects, DeserializationFlags);
	}
	delete ArchetypeDelta;
}

int32 DeserializeObjectData(FRemoteObjectData& ObjectData,
							const FUObjectMigrationContext* MigrationContext,
							TArray<FRemoteObjectId>& OutObjectRemoteIds,
							TArray<UObject*>& OutObjects,
							ERemoteObjectSerializationFlags DeserializeFlags)
{
	using namespace UE::RemoteObject::Handle;
	using namespace UE::RemoteObject::Private;

	int32 Version = 0;
	FRemoteObjectId RequestedObjectId;
	int64 HeaderOffset = 0;
	TArray<FRemoteObjectHeader> ObjectHeaders;
	TArray<UObject*> ResolvedPathNameObjects;
	int32 RequestedObjectIndex = -1;
	const bool bResetting = EnumHasAnyFlags(DeserializeFlags, ERemoteObjectSerializationFlags::Resetting);

	ResolvePathNames(ObjectData, ResolvedPathNameObjects);

	// If we are being assigned ownership, we *must* take ownership of the objects.  Note: We also may already have ownership of those objects. 
	FArchiveRemoteObjectReader Ar(ObjectData, ResolvedPathNameObjects, MigrationContext, DeserializeFlags);
	// If we're calling this function to reset an object to its archetype state then we don't want FArchiveRemoteObjectReader to be marked as migrating remote objects (Which it is by default)
	Ar.SetMigratingRemoteObjects(!bResetting);

	{
		FRemoteObjectReaderScope Scope(Ar, TEXT("Header"));
		Ar << Version;
		Ar << RequestedObjectId;
		Ar << HeaderOffset;
	}

	const int64 ObjectDataOffset = Ar.Tell();

	Ar.Seek(HeaderOffset);
	{
		FRemoteObjectReaderScope Scope(Ar, TEXT("ObjectHeaders"));
		Ar << ObjectHeaders;
	}
	Ar.Seek(ObjectDataOffset);	

	if (ObjectHeaders.Num())
	{
		const FRemoteServerId LocalServerId = UE::RemoteObject::GetGlobalServerId();

		// Try to find any existing WeakObject serial numbers for the objects that are about to be constructed
		for (FRemoteObjectHeader& ObjectHeader : ObjectHeaders)
		{
			if (FRemoteObjectStub* Stub = FindRemoteObjectStub(ObjectHeader.RemoteId))
			{
				ObjectHeader.SerialNumber = Stub->SerialNumber;
				if (!Stub->Name.IsNone())
				{
					// Remote object could've been renamed when it was migrated so always make sure that it has the same name locally
					ObjectHeader.Name = Stub->Name;
				}
			}
		}

		const bool bReturningBorrowedObject = GResetBorrowedObjects && IsOwned(ObjectHeaders[0].RemoteId);
		if (bReturningBorrowedObject)
		{
			// Root object is owned by this server so we're receiving an object that was borrowed by another server.
			// In this case we don't need to reconstruct anything and we can re-use the objects that are already in memory.
			DeserializeFlags |= ERemoteObjectSerializationFlags::UseExistingObjects;
		}

		// Construct all objects first
		FRemoteObjectConstructionOverrides ConstructionOverrides(ObjectHeaders);
		OutObjectRemoteIds.Reserve(ObjectHeaders.Num());
		OutObjects.Reserve(ObjectHeaders.Num());
		for (int32 ObjectIndex = 0; ObjectIndex < ObjectHeaders.Num(); ++ObjectIndex)
		{
			OutObjectRemoteIds.Add(ObjectHeaders[ObjectIndex].RemoteId);
			OutObjects.Add(ConstructRemoteObject(ObjectHeaders[ObjectIndex], &ConstructionOverrides, DeserializeFlags));
		}

		UObject* RootObject = OutObjects.Num() ? OutObjects[0] : nullptr;
		if (!ensureMsgf(RootObject, TEXT("%hs had objects to construct but could not reconstruct them"), __func__))
		{
			return RequestedObjectIndex;
		}

		// If we're already resetting a borrowed object we don't want to change the ownership until the object is deserialized using remote server data
		// and we also don't want to recursively reset the object
		if (!bResetting)
		{
			// ensure remote object stubs are created (ownership will be assigned after PostMigrate)
			for (int32 ObjectIndex = 0; ObjectIndex < OutObjects.Num(); ++ObjectIndex)
			{
				if (UObject* Object = OutObjects[ObjectIndex])
				{
					UE::RemoteObject::Private::RegisterRemoteObjectId(FRemoteObjectId(Object), LocalServerId);
				}
			}

			if (bReturningBorrowedObject)
			{
				ResetRemoteObject(OutObjects[0]);
			}
		}

		// Deserialize all objects
		Ar.SetRootObject(OutObjects.Num() ? OutObjects[0] : nullptr);
		for (int32 ObjectIndex = 0; ObjectIndex < OutObjects.Num(); ++ObjectIndex)
		{
			if (UObject* Object = OutObjects[ObjectIndex])
			{
				FRemoteObjectReaderScope Scope(Ar, Object);
				Object->Serialize(Ar);
				if (RequestedObjectIndex == -1 && RequestedObjectId == FRemoteObjectId(Object))
				{
					RequestedObjectIndex = ObjectIndex;
				}
			}
			else
			{
				Ar.Seek(ObjectHeaders[ObjectIndex].NextOffset);
			}
		}

		// Patch any unresolved remote references (skip if we're resetting an object to its CDO state)
		if (!bResetting)
		{
			FArchiveRemoteReferencePatcher PatchAr(OutObjects);
			for (UObject* Object : OutObjects)
			{
				if (Object)
				{
					Object->Serialize(PatchAr);
				}
			}
		}
	}
	checkf(RequestedObjectIndex >= 0, TEXT("Received remote object data but the requested object (%s) was not deserialized"), *RequestedObjectId.ToString());
	return RequestedObjectIndex;
}

UObject* FindCanonicalRootObjectForSerialization(UObject* Object)
{
	// find the outermost migration root
	UObject* Cursor = Object;

	// if we walk the outer chain and don't happen to find 
	// any migration roots, default to using the object itself
	UObject* Result = Object;

#if UE_WITH_REMOTE_OBJECT_HANDLE
	// walk the cursor up the entire Outer chain and update
	// Result with the outermost Outer that is a migration root
	// (this covers the case where we find a migration root
	// nested in another, we pick the outermost one)
	while (Cursor)
	{
		if (Cursor->IsMigrationRoot())
		{
			Result = Cursor;
		}

		Cursor = Cursor->GetOuter();
	}
#endif

	return Result;
}

} // namespace UE::RemoteObject::Serialization