// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextProvider.h"
#include "ObjectTrace.h"

FName FAnimNextProvider::ProviderName("AnimNextProvider");

#define LOCTEXT_NAMESPACE "AnimNextProvider"

FAnimNextProvider::FAnimNextProvider(TraceServices::IAnalysisSession& InSession)
	: Session(InSession)
{
}

void FAnimNextProvider::AppendInstance(uint64 InstanceId, uint64 HostInstanceId, uint64 AssetId, uint64 OuterObjectId)
{
	Session.WriteAccessCheck();

	if (HostInstanceId == 0)
	{
		ComponentIdToModuleId.Add(OuterObjectId, InstanceId);
	}

	TSharedRef<FDataInterfaceData> Data = MakeShared<FDataInterfaceData>(Session);
	Data->InstanceId = InstanceId;
	Data->HostInstanceId = HostInstanceId;
	Data->OuterObjectId = OuterObjectId;
	Data->AssetId = AssetId;

	DataInterfaceData.Add(InstanceId, Data);
	
	TArray<TSharedRef<FDataInterfaceData>>& ChildList = HostToChildDataMap.FindOrAdd(HostInstanceId);
	ChildList.AddUnique(Data);
}

void FAnimNextProvider::AppendVariables(double ProfileTime, double RecordingTime, uint64 DataInterfaceId, uint32 PropertyDescriptionHash, const TArrayView<const uint8>& VariableData)
{
	Session.WriteAccessCheck();
	
	if (const TSharedRef<FDataInterfaceData>* Data = DataInterfaceData.Find(DataInterfaceId))
	{
		if (!VariableData.IsEmpty())
		{
			(*Data)->VariablesTimeline.AppendEvent(ProfileTime, FPropertyVariableData(PropertyDescriptionHash, VariableData));
		}
		(*Data)->EndTime = RecordingTime;
		if ((*Data)->StartTime < 0)
		{
			(*Data)->StartTime = RecordingTime;
		}
	}
}

void FAnimNextProvider::AppendVariableDescriptions(uint32 PropertyDescriptionHash, const TArrayView<const uint8>& VariableDescriptionData)
{
	Session.WriteAccessCheck();
	
	if (HashToPropertyDescriptionDataMap.Contains(PropertyDescriptionHash) == false)
	{
		if (!VariableDescriptionData.IsEmpty())
		{
			HashToPropertyDescriptionDataMap.Add(PropertyDescriptionHash, FPropertyDescriptionData(VariableDescriptionData));
		}
	}
}


bool FAnimNextProvider::GetModuleId(uint64 ComponentId, uint64& OutModuleId) const
{
	Session.ReadAccessCheck();
	
	if (const uint64* Id = ComponentIdToModuleId.Find(ComponentId))
	{
		OutModuleId = *Id;
		return true;
	}
	return false;
}

const FDataInterfaceData* FAnimNextProvider::GetDataInterfaceData(uint64 DataInterfaceId) const
{
	const TSharedRef<FDataInterfaceData>* Data = DataInterfaceData.Find(DataInterfaceId);
	if (Data)
	{
		return &**Data;
	}
	return nullptr;
}

void FAnimNextProvider::EnumerateChildInstances(uint64 InstanceId, TFunctionRef<void(const FDataInterfaceData&)> Callback) const
{
	Session.ReadAccessCheck();

	if (const TArray<TSharedRef<FDataInterfaceData>>* Children = HostToChildDataMap.Find(InstanceId))
	{
		for (auto Data : *Children)
		{
			check (Data->HostInstanceId == InstanceId);
			Callback(*Data);
		}
	}

}

#undef LOCTEXT_NAMESPACE
