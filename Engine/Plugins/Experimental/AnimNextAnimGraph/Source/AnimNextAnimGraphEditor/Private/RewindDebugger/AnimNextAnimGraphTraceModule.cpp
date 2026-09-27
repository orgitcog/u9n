// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAnimGraphTraceModule.h"
#include "AnimNextAnimGraphProvider.h"
#include "AnimNextAnimGraphAnalyzer.h"

FName FAnimNextAnimGraphTraceModule::ModuleName("AnimNextAnimGraph");

void FAnimNextAnimGraphTraceModule::GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = ModuleName;
	OutModuleInfo.DisplayName = TEXT("AnimNextAnimGraph");
}

void FAnimNextAnimGraphTraceModule::OnAnalysisBegin(TraceServices::IAnalysisSession& InSession)
{
	TSharedPtr<FAnimNextAnimGraphProvider> AnimNextAnimGraphProvider = MakeShared<FAnimNextAnimGraphProvider>(InSession);
	InSession.AddProvider(FAnimNextAnimGraphProvider::ProviderName, AnimNextAnimGraphProvider);

	InSession.AddAnalyzer(new FAnimNextAnimGraphAnalyzer(InSession, *AnimNextAnimGraphProvider));
}

void FAnimNextAnimGraphTraceModule::GetLoggers(TArray<const TCHAR *>& OutLoggers)
{
	OutLoggers.Add(TEXT("AnimNextAnimGraph"));
}

void FAnimNextAnimGraphTraceModule::GenerateReports(const TraceServices::IAnalysisSession& Session, const TCHAR* CmdLine, const TCHAR* OutputDirectory)
{

}

