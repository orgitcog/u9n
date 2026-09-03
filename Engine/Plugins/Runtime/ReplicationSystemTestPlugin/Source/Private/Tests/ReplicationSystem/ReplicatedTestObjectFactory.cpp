// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicatedTestObjectFactory.h"
#include "ReplicatedTestObject.h"

#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/NetBitStreamUtil.h"


namespace UE::Net
{

//-------------------------------------------------------------------------
// FReplicationTestCreationHeader
//------------------------------------------------------------------------

FString FReplicationTestCreationHeader::ToString() const
{
	return FString::Printf(TEXT("FReplicationTestCreationHeader (ProtocolId:0x%x)\n\t"
								"ArchetypeName=%s"
								"NumComponentsToSpawn=%u"
								"NumIrisComponentsToSpawn=%u"
								"NumDynamicComponentsToSpawn=%u"
								"NumConnectionFilteredComponentsToSpawn=%u"
								"NumObjectReferenceComponentsToSpawn=%u"
								"bForceFailCreationRemoteInstance=%u"),
						   		GetProtocolId(),
						   		*ArchetypeName,
						   		NumComponentsToSpawn,
						   		NumIrisComponentsToSpawn,
						   		NumDynamicComponentsToSpawn,
						   		NumConnectionFilteredComponentsToSpawn,
						   		NumObjectReferenceComponentsToSpawn,
						   		bForceFailCreateRemoteInstance);
}

bool FReplicationTestCreationHeader::Serialize(const FCreationHeaderContext& Context) const
{
	FNetBitStreamWriter& Writer = *Context.Serialization.GetBitStreamWriter();

	WriteString(&Writer, ArchetypeName);
	Writer.WriteBits(NumComponentsToSpawn, 16);
	Writer.WriteBits(NumIrisComponentsToSpawn, 16);
	Writer.WriteBits(NumDynamicComponentsToSpawn, 16);
	Writer.WriteBits(NumConnectionFilteredComponentsToSpawn, 16);
	Writer.WriteBits(NumObjectReferenceComponentsToSpawn, 16);
	Writer.WriteBool(bForceFailCreateRemoteInstance);

	return !Writer.IsOverflown();
}

bool FReplicationTestCreationHeader::Deserialize(const FCreationHeaderContext& Context)
{
	FNetBitStreamReader& Reader = *Context.Serialization.GetBitStreamReader();

	ReadString(&Reader, this->ArchetypeName);

	this->NumComponentsToSpawn = Reader.ReadBits(16);
	this->NumIrisComponentsToSpawn = Reader.ReadBits(16);
	this->NumDynamicComponentsToSpawn = Reader.ReadBits(16);
	this->NumConnectionFilteredComponentsToSpawn = Reader.ReadBits(16);
	this->NumObjectReferenceComponentsToSpawn = Reader.ReadBits(16);
	this->bForceFailCreateRemoteInstance = Reader.ReadBool();

	return !Reader.IsOverflown();
}

} // end namespace UE::Net

//------------------------------------------------------------------------
// UReplicatedTestObjectFactory
//------------------------------------------------------------------------

FName UReplicatedTestObjectFactory::GetFactoryName()
{
	return TEXT("TestObjectFactory");
}

TUniquePtr<UE::Net::FNetObjectCreationHeader> UReplicatedTestObjectFactory::CreateAndFillHeader(UE::Net::FNetRefHandle Handle)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	const UObject* Object = Bridge->GetReplicatedObject(Handle);
	if (!ensure(Object))
	{
		return nullptr;
	}

	UObject* Archetype = Object->GetArchetype();
	if (!Archetype)
	{
		check(Archetype);
		return nullptr;
	}

	TUniquePtr<FReplicationTestCreationHeader> HeaderRef(new FReplicationTestCreationHeader);
	FReplicationTestCreationHeader* Header = HeaderRef.Get();

	if (const UReplicatedTestObject* ReplicatedTestObject = Cast<UReplicatedTestObject>(Object))
	{
		Header->bForceFailCreateRemoteInstance = ReplicatedTestObject->bForceFailToInstantiateOnRemote;
	}

	if (const UTestReplicatedIrisObject* TestReplicatedIrisObject = Cast<UTestReplicatedIrisObject>(Object))
	{
		Header->NumComponentsToSpawn = IntCastChecked<uint16>(TestReplicatedIrisObject->Components.Num());
		Header->NumIrisComponentsToSpawn = IntCastChecked<uint16>(TestReplicatedIrisObject->IrisComponents.Num());
		Header->NumDynamicComponentsToSpawn = IntCastChecked<uint16>(TestReplicatedIrisObject->DynamicStateComponents.Num());
		Header->NumConnectionFilteredComponentsToSpawn = IntCastChecked<uint16>(TestReplicatedIrisObject->ConnectionFilteredComponents.Num());
		Header->NumObjectReferenceComponentsToSpawn = IntCastChecked<uint16>(TestReplicatedIrisObject->ObjectReferenceComponents.Num());
	}

	Header->ArchetypeName = Archetype->GetPathName();

	return HeaderRef;
}

bool UReplicatedTestObjectFactory::SerializeHeader(const UE::Net::FCreationHeaderContext& Context, const UE::Net::FNetObjectCreationHeader* Header)
{
	const UE::Net::FReplicationTestCreationHeader* TestHeader = static_cast<const UE::Net::FReplicationTestCreationHeader*>(Header);
	return TestHeader->Serialize(Context);
}

TUniquePtr<UE::Net::FNetObjectCreationHeader> UReplicatedTestObjectFactory::CreateAndDeserializeHeader(const UE::Net::FCreationHeaderContext& Context)
{
	TUniquePtr<UE::Net::FReplicationTestCreationHeader> Header(new UE::Net::FReplicationTestCreationHeader);
	Header->Deserialize(Context);
	return Header;
}

UNetObjectFactory::FInstantiateResult UReplicatedTestObjectFactory::InstantiateReplicatedObjectFromHeader(const FInstantiateContext& Context, const UE::Net::FNetObjectCreationHeader* InHeader)
{
	using namespace UE::Net;

	const FReplicationTestCreationHeader* Header = static_cast<const FReplicationTestCreationHeader*>(InHeader);

	// Force fail to create this remote instance
	if (Header->bForceFailCreateRemoteInstance)
	{
		return FInstantiateResult();
	}

	UObject* ArcheType = StaticFindObject(UObject::StaticClass(), nullptr, *Header->ArchetypeName, false);
	check(ArcheType);

	FStaticConstructObjectParameters ConstructObjectParameters(ArcheType->GetClass());
	UObject* CreatedObject = StaticConstructObject_Internal(ConstructObjectParameters);

	const bool bIsSubObject = Context.RootObjectOfSubObject.IsValid();

	if (UReplicatedTestObject* BaseTestObject = Cast<UReplicatedTestObject>(CreatedObject))
	{
		BaseTestObject->bIsSubObject = bIsSubObject;
	}
	
	if (UTestReplicatedIrisObject* CreatedTestObject = Cast<UTestReplicatedIrisObject>(CreatedObject))
	{
		UTestReplicatedIrisObject::FComponents Components;
		Components.PropertyComponentCount = Header->NumComponentsToSpawn;
		Components.IrisComponentCount = Header->NumIrisComponentsToSpawn;
		Components.DynamicStateComponentCount = Header->NumDynamicComponentsToSpawn;
		Components.ConnectionFilteredComponentCount = Header->NumConnectionFilteredComponentsToSpawn;
		Components.ObjectReferenceComponentCount = Header->NumObjectReferenceComponentsToSpawn;

		CreatedTestObject->AddComponents(Components);
	}

	UReplicatedTestObjectBridge* TestBridge = Cast<UReplicatedTestObjectBridge>(Bridge);

	// Store the object so that we can find detached/torn off instances from tests
	if (CreatedObjectsOnNode)
	{
		CreatedObjectsOnNode->Add(TStrongObjectPtr<UObject>(CreatedObject));
	}
	
	FInstantiateResult InstantiateResult;
	InstantiateResult.Instance = CreatedObject;
	InstantiateResult.Flags |= EReplicationBridgeCreateNetRefHandleResultFlags::AllowDestroyInstanceFromRemote;

	if (bIsSubObject)
	{
		InstantiateResult.Flags |= EReplicationBridgeCreateNetRefHandleResultFlags::ShouldCallSubObjectCreatedFromReplication;
	}

	return InstantiateResult;
}

void UReplicatedTestObjectFactory::PostInit(const FPostInitContext& Context)
{
	UReplicatedTestObject* Instance = CastChecked<UReplicatedTestObject>(Context.Instance);

	Instance->NetRefHandle = Context.Handle;
}

void UReplicatedTestObjectFactory::DestroyReplicatedObject(const FDestroyedContext& Context)
{
	if (!Context.DestroyedInstance)
	{
		return;
	}

	if (Context.DestroyReason == EReplicationBridgeDestroyInstanceReason::Destroy)
	{
		if (EnumHasAnyFlags(Context.DestroyFlags, EReplicationBridgeDestroyInstanceFlags::AllowDestroyInstanceFromRemote))
		{
			// Remove the object from the created objects on the node
			if (CreatedObjectsOnNode)
			{
				CreatedObjectsOnNode->Remove(TStrongObjectPtr<UObject>(Context.DestroyedInstance));
			}

			Context.DestroyedInstance->PreDestroyFromReplication();
			Context.DestroyedInstance->MarkAsGarbage();
		}
	}
}

void UReplicatedTestObjectFactory::GetWorldInfo(const FWorldInfoContext& Context, FWorldInfoData& OutData)
{
	if (GetInstanceWorldObjectInfoFunction)
	{
		GetInstanceWorldObjectInfoFunction(Context.Handle, Context.Instance, OutData.WorldLocation, OutData.CullDistance);
	}
}

void UReplicatedTestObjectFactory::SubObjectCreatedFromReplication(UE::Net::FNetRefHandle RootObject, UE::Net::FNetRefHandle SubObjectCreated)
{
	UReplicatedTestObject* RootInstance = Cast<UReplicatedTestObject>(Bridge->GetReplicatedObject(RootObject));
	if (ensure(RootInstance))
	{
		UObject* SubObject = Bridge->GetReplicatedObject(SubObjectCreated);
		RootInstance->OnSubObjectCreated(SubObject);
	}
}

void UReplicatedTestObjectFactory::SubObjectDestroyedFromReplication(const FDestroyedContext& Context)
{
	UReplicatedTestObject* RootInstance = Cast<UReplicatedTestObject>(Context.RootObject);
	if (ensure(RootInstance))
	{
		RootInstance->OnSubObjectDestroyed(Context.DestroyedInstance);
	}
}