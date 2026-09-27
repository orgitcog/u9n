// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebugger/AnimNextTrace.h"

#include "ObjectTrace.h"
#include "Chaos/ParticleDirtyFlags.h"
#include "DataInterface/AnimNextDataInterfaceInstance.h"
#include "Misc/HashBuilder.h"
#include "Module/AnimNextModuleInstance.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/ObjectWriter.h"

#if ANIMNEXT_TRACE_ENABLED


UE_TRACE_EVENT_BEGIN(AnimNext, Instance)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint64, HostInstanceId)
	UE_TRACE_EVENT_FIELD(uint64, OuterObjectId)
	UE_TRACE_EVENT_FIELD(uint64, AssetId)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(AnimNext, InstanceVariables)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(double, RecordingTime)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, VariableDescriptionHash)
	UE_TRACE_EVENT_FIELD(uint8[], VariableData)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(AnimNext, InstanceVariableDescriptions)
	UE_TRACE_EVENT_FIELD(uint32, VariableDescriptionHash)
	UE_TRACE_EVENT_FIELD(uint8[], VariableDescriptionData)
UE_TRACE_EVENT_END()

UE_TRACE_CHANNEL_DEFINE(AnimNextChannel);

namespace UE::AnimNext
{

FRWLock GTracedInstancesRWLock;
TSet<uint64> GTracedInstances;

FRWLock GTracedPropertiesRWLock;
TMap<uint32, const UPropertyBag*> GTracedPropertyDescs;

const FGuid FAnimNextTrace::CustomVersionGUID = FGuid(0x83E9BE13, 0xB1C845DC, 0x86C4D3E5, 0xE66CBE91);

namespace FAnimNextTraceCustomVersion
{
enum CustomVersionIndex
{
	// Before any version changes were made in the plugin
	FirstVersion = 0,

	// -----<new versions can be added above this line>-------------------------------------------------
	VersionPlusOne,
	LatestVersion = VersionPlusOne - 1
};
}

FCustomVersionRegistration GPropertyBagCustomVersion(FAnimNextTrace::CustomVersionGUID, FAnimNextTraceCustomVersion::FirstVersion, TEXT("FAnimNextTraceCustomVersion"));

void FAnimNextTrace::Reset()
{
	GTracedInstances.Empty();
	GTracedPropertyDescs.Empty();
}

void FAnimNextTrace::OutputAnimNextInstance(const FAnimNextDataInterfaceInstance* DataInterface, const UObject* OuterObject)
{
	bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(AnimNextChannel);
	if (!bChannelEnabled || DataInterface == nullptr)
	{
		return;
	}

	if (CANNOT_TRACE_OBJECT(OuterObject))
	{
		return;
	}
	
	uint64 InstanceId = DataInterface->GetUniqueId();

	bool bTrace = false;

	{
		FRWScopeLock RWScopeLock(GTracedInstancesRWLock, SLT_ReadOnly);
		
		if (!GTracedInstances.Contains(InstanceId))
		{
			RWScopeLock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();

			// Note that we might double add here due to 2x people writing at the same time, but it doesn't matter
			GTracedInstances.Add(InstanceId);
			bTrace = true;
		}
	}

	if (bTrace)
	{
		if (DataInterface->GetHost())
		{
			OutputAnimNextInstance(DataInterface->GetHost(), OuterObject);
		}
		
		TRACE_OBJECT(OuterObject);
		TRACE_OBJECT(DataInterface->GetDataInterface());
		
		const FAnimNextDataInterfaceInstance* HostInstance = DataInterface->GetHost();

		UE_TRACE_LOG(AnimNext, Instance, AnimNextChannel)
			<< Instance.InstanceId(InstanceId)
			<< Instance.OuterObjectId(FObjectTrace::GetObjectId(OuterObject))
			<< Instance.HostInstanceId(HostInstance ? HostInstance->GetUniqueId() : 0)
			<< Instance.AssetId(FObjectTrace::GetObjectId(DataInterface->GetDataInterface()));
	}
}

uint32 GetPropertyDescHash(const TConstArrayView<FPropertyBagPropertyDesc>& Descs)
{
	FHashBuilder HashBuilder;
	for (const FPropertyBagPropertyDesc& Desc : Descs)
	{
		HashBuilder.Append(Desc.Name);
		HashBuilder.Append(Desc.ContainerTypes);
		HashBuilder.Append(Desc.ID);
		HashBuilder.Append(Desc.PropertyFlags);
		HashBuilder.Append(Desc.ValueType);

		// Q: should we care about metadata?
	}
	return HashBuilder.GetHash();
}

void FAnimNextTrace::OutputAnimNextVariables(const FAnimNextDataInterfaceInstance* Instance, const UObject* OuterObject)
{
	bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(AnimNextChannel);
	if (!bChannelEnabled || Instance == nullptr || OuterObject == nullptr)
	{
		return;
	}

	uint64 InstanceId = Instance->GetUniqueId();

	if (CANNOT_TRACE_OBJECT(OuterObject))
	{
		return;
	}

	OutputAnimNextInstance(Instance, OuterObject);
	
	FInstancedPropertyBag Variables = Instance->GetVariables();
	if (Variables.GetPropertyBagStruct())
	{
		TConstArrayView<FPropertyBagPropertyDesc> PropertyDescs = Variables.GetPropertyBagStruct()->GetPropertyDescs();
		for(int i =0; i<PropertyDescs.Num(); i++)
		{
			uint8* ValueMemory = Instance->GetMemoryForVariable(i, PropertyDescs[i].Name, PropertyDescs[i].CachedProperty);
			PropertyDescs[i].CachedProperty->CopySingleValue(const_cast<uint8*>(Variables.GetValue().GetMemory()) +  PropertyDescs[i].CachedProperty->GetOffset_ForInternal(), ValueMemory);
		}
		
		uint32 PropertyDescHash = GetPropertyDescHash(PropertyDescs);
		{
			FRWScopeLock RWScopeLock(GTracedPropertiesRWLock, SLT_ReadOnly);
			TArray<uint8> ArchiveData;
			ArchiveData.Reserve(1024 * 10);
			
			if (!GTracedPropertyDescs.Contains(PropertyDescHash))
			{
				// Upgrade to write lock
				RWScopeLock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();

				// Check in case someone just serialized before we aquired the write lock
				if (!GTracedPropertyDescs.Contains(PropertyDescHash))
				{
					GTracedPropertyDescs.Add(PropertyDescHash, Variables.GetPropertyBagStruct());
					
					// Serialize prop descs
					TArray<FPropertyBagPropertyDesc, TInlineAllocator<64>> PropertyDescriptions;
					PropertyDescriptions.Reserve(PropertyDescs.Num());
					PropertyDescriptions = PropertyDescs;	// need to copy since we can't serialize an array view

					FMemoryWriter WriterArchive(ArchiveData);
					FObjectAndNameAsStringProxyArchive Archive(WriterArchive, /*bInLoadIfFindFails*/true);
					Archive.UsingCustomVersion(CustomVersionGUID);
					Archive.UsingCustomVersion(FPropertyBagCustomVersion::GUID);
					Archive << PropertyDescriptions;

					UE_TRACE_LOG(AnimNext, InstanceVariableDescriptions, AnimNextChannel)
						<< InstanceVariableDescriptions.VariableDescriptionHash(PropertyDescHash)
						<< InstanceVariableDescriptions.VariableDescriptionData(ArchiveData.GetData(), ArchiveData.Num());
				}
			}

			ArchiveData.SetNum(0, EAllowShrinking::No);
			FMemoryWriter WriterArchive(ArchiveData);
			FObjectAndNameAsStringProxyArchive Archive(WriterArchive, /*bInLoadIfFindFails*/true);
			UPropertyBag* PropertyBag = const_cast<UPropertyBag*>(Variables.GetPropertyBagStruct());
			if (PropertyBag != nullptr)
			{
				PropertyBag->SerializeItem(Archive, Variables.GetMutableValue().GetMemory(), nullptr);
			}

			UE_TRACE_LOG(AnimNext, InstanceVariables, AnimNextChannel)
					<< InstanceVariables.Cycle(FPlatformTime::Cycles64())
					<< InstanceVariables.RecordingTime(FObjectTrace::GetWorldElapsedTime(OuterObject->GetWorld()))
					<< InstanceVariables.InstanceId(InstanceId)
					<< InstanceVariables.VariableDescriptionHash(PropertyDescHash)
					<< InstanceVariables.VariableData(ArchiveData.GetData(), ArchiveData.Num());
		}
	}
	else
	{
		UE_TRACE_LOG(AnimNext, InstanceVariables, AnimNextChannel)
				<< InstanceVariables.Cycle(FPlatformTime::Cycles64())
				<< InstanceVariables.RecordingTime(FObjectTrace::GetWorldElapsedTime(OuterObject->GetWorld()))
				<< InstanceVariables.InstanceId(InstanceId);
	}
}
	
}

#endif
