// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassInsightsAnalysisModule.h"

#include "Model/MassInsightsPrivate.h"
#include "Analyzers/MassInsightsTraceAnalysis.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"

namespace MassInsightsAnalysis
{

void FMassInsightsAnalysisModule::OnAnalysisBegin(TraceServices::IAnalysisSession& InSession)
{
	TSharedPtr<FMassInsightsProvider> MassInsightsProvider = MakeShared<FMassInsightsProvider>(InSession);
	InSession.AddProvider(GetMassInsightsProviderName(), MassInsightsProvider);

	InSession.AddAnalyzer(new FMassInsightsTraceAnalyzer(InSession, *MassInsightsProvider));
}

void FMassInsightsAnalysisModule::GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = TEXT("MassInsightsProvider");
	OutModuleInfo.DisplayName = TEXT("MassInsights");
}

void FMassInsightsAnalysisModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, this);
}

void FMassInsightsAnalysisModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, this);
}

} //namespace MassInsightsAnalysis

IMPLEMENT_MODULE(MassInsightsAnalysis::FMassInsightsAnalysisModule, MassInsightsAnalysis);
