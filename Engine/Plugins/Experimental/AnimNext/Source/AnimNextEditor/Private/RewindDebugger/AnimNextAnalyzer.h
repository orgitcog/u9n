// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"

class FAnimNextProvider;
namespace TraceServices { class IAnalysisSession; }

class FAnimNextAnalyzer : public UE::Trace::IAnalyzer
{
public:
	FAnimNextAnalyzer(TraceServices::IAnalysisSession& InSession, FAnimNextProvider& InProvider);

	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual void OnAnalysisEnd() override {}
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;

private:
	enum : uint16
	{
		RouteId_Module,
		RouteId_InstanceVariables,
		RouteId_InstanceVariableDescriptions
	};

	TraceServices::IAnalysisSession& Session;
	FAnimNextProvider& Provider;
};
