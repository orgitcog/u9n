// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAnalyzer.h"

#include "HAL/LowLevelMemTracker.h"
#include "Serialization/MemoryReader.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "AnimNextProvider.h"
#include "TraceServices/Utils.h"
#include "StructUtils/PropertyBag.h"
#include "Serialization/ObjectReader.h"

FAnimNextAnalyzer::FAnimNextAnalyzer(TraceServices::IAnalysisSession& InSession, FAnimNextProvider& InProvider)
	: Session(InSession)
	, Provider(InProvider)
{
}

void FAnimNextAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;

	Builder.RouteEvent(RouteId_Module, "AnimNext", "Instance");
	Builder.RouteEvent(RouteId_InstanceVariables, "AnimNext", "InstanceVariables");
	Builder.RouteEvent(RouteId_InstanceVariableDescriptions, "AnimNext", "InstanceVariableDescriptions");
}

bool FAnimNextAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FAnimNextAnalyzer"));

	TraceServices::FAnalysisSessionEditScope _(Session);

	const auto& EventData = Context.EventData;
	switch (RouteId)
	{
		case RouteId_Module:
		{
			uint64 InstanceId = EventData.GetValue<uint64>("InstanceId");
			uint64 HostInstanceId = EventData.GetValue<uint64>("HostInstanceId");
			uint64 AssetId = EventData.GetValue<uint64>("AssetId");
			uint64 OuterObjectId = EventData.GetValue<uint64>("OuterObjectId");
			Provider.AppendInstance(InstanceId, HostInstanceId, AssetId, OuterObjectId);
			break;
		}
		case RouteId_InstanceVariables:
		{
			uint64 ModuleInstanceId = EventData.GetValue<uint64>("InstanceId");
			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			double RecordingTime = EventData.GetValue<double>("RecordingTime");
			uint32 VariableDescHash = EventData.GetValue<uint32>("VariableDescriptionHash");
			TArrayView<const uint8> VariableData = EventData.GetArrayView<uint8>("VariableData");

			Provider.AppendVariables(Context.EventTime.AsSeconds(Cycle), RecordingTime, ModuleInstanceId, VariableDescHash, VariableData);
			break;
		}
		case RouteId_InstanceVariableDescriptions:
		{
			uint32 VariableDescHash = EventData.GetValue<uint32>("VariableDescriptionHash");
			TArrayView<const uint8> VariableDescData = EventData.GetArrayView<uint8>("VariableDescriptionData");

			Provider.AppendVariableDescriptions(VariableDescHash, VariableDescData);
			break;
		}
	}

	return true;
}
