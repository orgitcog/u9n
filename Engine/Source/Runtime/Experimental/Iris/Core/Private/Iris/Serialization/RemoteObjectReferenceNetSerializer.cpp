// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteObjectReferenceNetSerializer.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/NetSerializers.h"
#include "UObject/RemoteObjectTransfer.h"

namespace UE::Net
{

struct FRemoteObjectReferenceNetSerializer
{
	// Version
	static const uint32 Version = 0;

	// Traits
	static constexpr bool bIsForwardingSerializer = true; // Triggers asserts if a function is missing
	static constexpr bool bHasDynamicState = true;
	static constexpr bool bHasCustomNetReference = true;

	// Types
	struct FQuantizedType
	{
		alignas(8) uint8 QuantizedStruct[48];
	};

	typedef FRemoteObjectReference SourceType;
	typedef FQuantizedType QuantizedType;
	typedef FRemoteObjectReferenceNetSerializerConfig ConfigType;

	static const ConfigType DefaultConfig;

	static void Serialize(FNetSerializationContext&, const FNetSerializeArgs& Args);
	static void Deserialize(FNetSerializationContext&, const FNetDeserializeArgs& Args);

	static void SerializeDelta(FNetSerializationContext&, const FNetSerializeDeltaArgs& Args);
	static void DeserializeDelta(FNetSerializationContext&, const FNetDeserializeDeltaArgs& Args);

	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args);

	static bool IsEqual(FNetSerializationContext&, const FNetIsEqualArgs& Args);
	static bool Validate(FNetSerializationContext&, const FNetValidateArgs& Args);

	static void CloneDynamicState(FNetSerializationContext&, const FNetCloneDynamicStateArgs&);
	static void FreeDynamicState(FNetSerializationContext&, const FNetFreeDynamicStateArgs&);

	static void CollectNetReferences(FNetSerializationContext&, const FNetCollectReferencesArgs&);

private:
	class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
	{
	private:
		virtual void OnPostFreezeNetSerializerRegistry() override;
	};

	static void RemoteObjectReferenceToRemoteObjectReferenceNetSerializationHelper(const SourceType& RemoteReference, FRemoteObjectReferenceNetSerializationHelper& OutStruct);

	static FRemoteObjectReferenceNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
	static FStructNetSerializerConfig StructNetSerializerConfig;
	static const FNetSerializer* StructNetSerializer;
};
UE_NET_IMPLEMENT_SERIALIZER_INTERNAL(FRemoteObjectReferenceNetSerializer);

const FRemoteObjectReferenceNetSerializer::ConfigType FRemoteObjectReferenceNetSerializer::DefaultConfig;

FRemoteObjectReferenceNetSerializer::FNetSerializerRegistryDelegates FRemoteObjectReferenceNetSerializer::NetSerializerRegistryDelegates;
FStructNetSerializerConfig FRemoteObjectReferenceNetSerializer::StructNetSerializerConfig;
const FNetSerializer* FRemoteObjectReferenceNetSerializer::StructNetSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);

void FRemoteObjectReferenceNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
{
	FNetSerializeArgs InternalArgs = Args;
	InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
	return StructNetSerializer->Serialize(Context, InternalArgs);
}

void FRemoteObjectReferenceNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
{
	FNetDeserializeArgs InternalArgs = Args;
	InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
	return StructNetSerializer->Deserialize(Context, InternalArgs);
}

void FRemoteObjectReferenceNetSerializer::SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args)
{
	FNetSerializeDeltaArgs InternalArgs = Args;
	InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
	return StructNetSerializer->SerializeDelta(Context, InternalArgs);
}

void FRemoteObjectReferenceNetSerializer::DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args)
{
	FNetDeserializeDeltaArgs InternalArgs = Args;
	InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
	return StructNetSerializer->DeserializeDelta(Context, InternalArgs);
}

void FRemoteObjectReferenceNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
{
	const SourceType& SourceValue = *reinterpret_cast<const SourceType*>(Args.Source);

	FRemoteObjectReferenceNetSerializationHelper IntermediateValue;
	RemoteObjectReferenceToRemoteObjectReferenceNetSerializationHelper(SourceValue, IntermediateValue);

	if (IntermediateValue.ObjectId.IsValid())
	{
		if (UObject* ExistingObject = StaticFindObjectFastInternal(IntermediateValue.ObjectId))
		{
			UE::RemoteObject::Transfer::RegisterSharedObject(ExistingObject);
			IntermediateValue.Path = FRemoteObjectPathName(ExistingObject);
		}
	}

	FNetQuantizeArgs InternalArgs = Args;
	InternalArgs.Source = NetSerializerValuePointer(&IntermediateValue);
	InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
	return StructNetSerializer->Quantize(Context, InternalArgs);
}

void FRemoteObjectReferenceNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
{
	SourceType& TargetValue = *reinterpret_cast<SourceType*>(Args.Target);

	FRemoteObjectReferenceNetSerializationHelper IntermediateValue;

	FNetDequantizeArgs InternalArgs = Args;
	InternalArgs.Target = NetSerializerValuePointer(&IntermediateValue);
	InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;

	StructNetSerializer->Dequantize(Context, InternalArgs);

	TargetValue.NetDequantize(IntermediateValue.ObjectId, IntermediateValue.ServerId, IntermediateValue.Path);
}

bool FRemoteObjectReferenceNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		FNetIsEqualArgs InternalArgs = Args;
		InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
		return StructNetSerializer->IsEqual(Context, InternalArgs);
	}
	else
	{
		const SourceType& SourceValue0 = *reinterpret_cast<const SourceType*>(Args.Source0);
		const SourceType& SourceValue1 = *reinterpret_cast<const SourceType*>(Args.Source1);

		return SourceValue0 == SourceValue1;
	}
}

bool FRemoteObjectReferenceNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
{
	const SourceType& SourceValue = *reinterpret_cast<const SourceType*>(Args.Source);

	// The remote reference's ObjectId and ServerId should either both be valid or both be invalid
	if (SourceValue.GetRemoteId().IsValid() != SourceValue.GetSharingServerId().IsValid())
	{
		return false;
	}

	FRemoteObjectReferenceNetSerializationHelper IntermediateValue;
	RemoteObjectReferenceToRemoteObjectReferenceNetSerializationHelper(SourceValue, IntermediateValue);

	FNetValidateArgs InternalArgs = Args;
	InternalArgs.Source = NetSerializerValuePointer(&IntermediateValue);
	InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;

	return StructNetSerializer->Validate(Context, InternalArgs);
}

void FRemoteObjectReferenceNetSerializer::CollectNetReferences(FNetSerializationContext& Context, const FNetCollectReferencesArgs& Args)
{
	using namespace Private;

	const FReplicationStateDescriptor* Descriptor = StructNetSerializerConfig.StateDescriptor;
	if (Descriptor && EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasObjectReference))
	{
		FNetCollectReferencesArgs InternalArgs = Args;
		InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
		StructNetSerializer->CollectNetReferences(Context, InternalArgs);
	}
}

void FRemoteObjectReferenceNetSerializer::CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args)
{
	FNetCloneDynamicStateArgs InternalArgs = Args;
	InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
	return StructNetSerializer->CloneDynamicState(Context, InternalArgs);
}

void FRemoteObjectReferenceNetSerializer::FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args)
{
	FNetFreeDynamicStateArgs InternalArgs = Args;
	InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
	return StructNetSerializer->FreeDynamicState(Context, InternalArgs);
}

void FRemoteObjectReferenceNetSerializer::RemoteObjectReferenceToRemoteObjectReferenceNetSerializationHelper(const SourceType& RemoteObjectReference, FRemoteObjectReferenceNetSerializationHelper& OutStruct)
{
	OutStruct.ObjectId = RemoteObjectReference.ObjectId;
	OutStruct.ServerId = RemoteObjectReference.ServerId;
}

void FRemoteObjectReferenceNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
{
	UStruct* Struct = FRemoteObjectReferenceNetSerializationHelper::StaticStruct();
	StructNetSerializerConfig.StateDescriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(Struct);
	const FReplicationStateDescriptor* Descriptor = StructNetSerializerConfig.StateDescriptor.GetReference();
	check(Descriptor != nullptr);

	// Validate our assumptions regarding quantized state size and alignment.
	static_assert(offsetof(FQuantizedType, QuantizedStruct) == 0U, "Expected buffer for struct to be first member of FQuantizedType.");
	if (sizeof(FQuantizedType::QuantizedStruct) < Descriptor->InternalSize || alignof(FQuantizedType) < Descriptor->InternalAlignment)
	{
		LowLevelFatalError(TEXT("FQuantizedType::QuantizedStruct for FRemoteObjectReferenceNetSerializer has size %u and alignment %u but requires size %u and alignment %u."), uint32(sizeof(FQuantizedType::QuantizedStruct)), uint32(alignof(FQuantizedType)), uint32(Descriptor->InternalSize), uint32(Descriptor->InternalAlignment)); 
	}
}

}
