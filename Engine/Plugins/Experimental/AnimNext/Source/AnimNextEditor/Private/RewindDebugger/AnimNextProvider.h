// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Model/PointTimeline.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Model/IntervalTimeline.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace TraceServices { class IAnalysisSession; }

struct FPropertyDescriptionData
{
	FPropertyDescriptionData(TConstArrayView<uint8> DataView) : Data(DataView) {}
	TArray<uint8> Data;
};

struct FPropertyVariableData
{
	FPropertyVariableData(uint32 DescriptionHash, TConstArrayView<uint8> Data)
		: DescriptionHash(DescriptionHash) , ValueData(Data) {}
	
	uint32 DescriptionHash;
	TArray<uint8> ValueData;
};

struct FDataInterfaceData
{
	FDataInterfaceData(TraceServices::IAnalysisSession& Session)
		: VariablesTimeline(Session.GetLinearAllocator( )),
			InstanceId (0),
			HostInstanceId (0),
			OuterObjectId (0),
			AssetId (0),
			StartTime (-1),
			EndTime (-1)
	{
	}

	TraceServices::TPointTimeline<FPropertyVariableData> VariablesTimeline;
	uint64 InstanceId;
	uint64 HostInstanceId;
	uint64 OuterObjectId;
	uint64 AssetId;
	double StartTime;
	double EndTime;
};

class FAnimNextProvider : public TraceServices::IProvider
{
public:
	static FName ProviderName;

	FAnimNextProvider(TraceServices::IAnalysisSession& InSession);

	void AppendInstance(uint64 InstanceId, uint64 HostInstanceId,  uint64 AssetId, uint64 OuterObjectId);
	void AppendVariables(double ProfileTime, double RecordingTime, uint64 InModuleInstanceId, uint32 PropertyDescriptionHash, const TArrayView<const uint8>& VariableData);
	void AppendVariableDescriptions(uint32 PropertyDescriptionHash, const TArrayView<const uint8>& VariableDescriptionData);

	bool GetModuleId(uint64 ComponentId, uint64& OutModuleId) const;
	const FDataInterfaceData* GetDataInterfaceData(uint64 DataInterfaceId) const;
	const FPropertyDescriptionData* GetPropertyDescriptionData(uint32 PropertyDescriptionHash) const { return HashToPropertyDescriptionDataMap.Find(PropertyDescriptionHash);}

	void EnumerateChildInstances(uint64 InstanceId, TFunctionRef<void(const FDataInterfaceData&)> Callback) const;

private:
	TraceServices::IAnalysisSession& Session;

	TMap<uint64, uint64> ComponentIdToModuleId;
	TMap<uint64, TSharedRef<FDataInterfaceData>> DataInterfaceData;
	TMap<uint64, TArray<TSharedRef<FDataInterfaceData>>> HostToChildDataMap;
	TMap<uint32, FPropertyDescriptionData> HashToPropertyDescriptionDataMap;
};
