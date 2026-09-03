// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"

namespace TraceServices
{
	class IAnalysisSession;
}

namespace MassInsightsAnalysis
{
	
class FMassInsightsProvider;

/*
 *  Analyzer for Timing Regions, parses the TimingRegionBegin and TimingRegionEnd events from the 'Misc' Logger 
 */
class FMassInsightsTraceAnalyzer
	: public UE::Trace::IAnalyzer
{
public:
	FMassInsightsTraceAnalyzer(TraceServices::IAnalysisSession& Session,
	                            FMassInsightsProvider& RegionProvider);
	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual void OnAnalysisEnd() override;
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;

private:
	enum : uint16
	{
		RouteId_RegisterMassFragment,
		RouteId_RegisterMassArchetype,
		RouteId_MassBulkAddEntity,
		RouteId_MassEntityMoved,
		RouteId_MassBulkEntityDestroyed,
		RouteId_MassPhaseBegin,
		RouteId_MassPhaseEnd
	};

	TraceServices::IAnalysisSession& Session;
	FMassInsightsProvider& MassInsightsProvider;
};

} // namespace MassInsightsAnalysis
