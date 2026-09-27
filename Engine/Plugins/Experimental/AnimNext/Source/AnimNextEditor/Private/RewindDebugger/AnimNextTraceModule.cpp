// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextTraceModule.h"
#include "AnimNextProvider.h"
#include "AnimNextAnalyzer.h"

FName FAnimNextTraceModule::ModuleName("AnimNext");

void FAnimNextTraceModule::GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = ModuleName;
	OutModuleInfo.DisplayName = TEXT("AnimNext");
}

void FAnimNextTraceModule::OnAnalysisBegin(TraceServices::IAnalysisSession& InSession)
{
	TSharedPtr<FAnimNextProvider> AnimNextProvider = MakeShared<FAnimNextProvider>(InSession);
	InSession.AddProvider(FAnimNextProvider::ProviderName, AnimNextProvider);

	InSession.AddAnalyzer(new FAnimNextAnalyzer(InSession, *AnimNextProvider));
}

void FAnimNextTraceModule::GetLoggers(TArray<const TCHAR *>& OutLoggers)
{
	OutLoggers.Add(TEXT("AnimNext"));
}

void FAnimNextTraceModule::GenerateReports(const TraceServices::IAnalysisSession& Session, const TCHAR* CmdLine, const TCHAR* OutputDirectory)
{

}

