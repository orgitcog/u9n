// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/NetTokenStructDefines.h"
#include "Iris/ReplicationSystem/StructNetTokenDataStore.h"
#include "Net/Iris/ReplicationSystem/StructNetTokenDataStoreHelper.h"
#include "SimModule/ModuleInput.h"
#include "ModuleInputTokenStore.generated.h"

USTRUCT()
struct FModuleInputNetTokenData
{
	GENERATED_BODY()
	UPROPERTY()
	TArray<uint8> Types;
	UPROPERTY()
	TArray<bool> DecayValues;
	
	UE_NET_NETTOKEN_GENERATED_BODY(ModuleInputNetTokenData, CHAOSMODULARVEHICLEENGINE_API)
	
	uint64 GetUniqueKey() const
	{
		uint64 HashOfTypes = CityHash64((const char*)Types.GetData(),sizeof(uint8)*Types.Num());
		uint64 HashOfDecayValues = CityHash64((const char*)DecayValues.GetData(),sizeof(bool)*DecayValues.Num());
		return HashOfTypes ^ HashOfDecayValues;
	}

	void Init(const TArray<FModuleInputValue>& ModuleInputs)
	{
		for (int32 Idx = 0; Idx < ModuleInputs.Num(); Idx++)
		{
			Types.Add(static_cast<uint8>(ModuleInputs[Idx].GetValueType()));
			DecayValues.Add(ModuleInputs[Idx].ShouldApplyInputDecay());
		}
	}
};

UE_NET_DECLARE_NAMED_NETTOKEN_STRUCT_SERIALIZERS(ModuleInputNetTokenData, CHAOSMODULARVEHICLEENGINE_API);
