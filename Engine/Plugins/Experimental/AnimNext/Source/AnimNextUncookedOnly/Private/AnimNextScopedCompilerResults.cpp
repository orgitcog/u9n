// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextScopedCompilerResults.h"

#include "AnimNextRigVMAsset.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "UncookedOnlyUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "AnimNextScopedCompilerResults"

namespace UE::AnimNext::UncookedOnly
{

struct FCompilerResultsThreadData
{
	TArray<TWeakPtr<FCompilerResultsLog>> CurrentLogStack;
	TArray<TSharedRef<FTokenizedMessage>> Messages;
	int32 NumErrors = 0;
	int32 NumWarnings = 0;
	bool bDidCompile = false;
};

static thread_local FCompilerResultsThreadData GCompilerResultsData;

FScopedCompilerResults::FScopedCompilerResults(const FText& InJobName)
	: FScopedCompilerResults(InJobName, nullptr, TArrayView<UObject*>())
{
}

FScopedCompilerResults::FScopedCompilerResults(UObject* InObject)
	: FScopedCompilerResults(FText::FromName(InObject->GetFName()), InObject, TArrayView<UObject*>(&InObject, 1))
{
}

FScopedCompilerResults::FScopedCompilerResults(const FText& InJobName, UObject* InObject, TArrayView<UObject*> InAssets)
	: JobName(InJobName)
	, Object(InObject)
{
	FCompilerResultsThreadData* ThreadData = &GCompilerResultsData;
	StartTime = FPlatformTime::Seconds();
	ThreadData->bDidCompile = Object != nullptr;
	Log = MakeShared<FCompilerResultsLog>(); 
	ThreadData->CurrentLogStack.Push(Log);

	for(UObject* Asset : InAssets)
	{
		UAnimNextRigVMAsset* AnimNextRigVMAsset = Cast<UAnimNextRigVMAsset>(Asset);
		if(AnimNextRigVMAsset == nullptr)
		{
			return;
		}

		UAnimNextRigVMAssetEditorData* EditorData = FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(AnimNextRigVMAsset);
		if(EditorData == nullptr)
		{
			return;
		}

		EditorData->ClearErrorInfoForAllEdGraphs();
	}
}

FScopedCompilerResults::~FScopedCompilerResults()
{
	FinishTime = FPlatformTime::Seconds();

	FCompilerResultsThreadData* ThreadData = &GCompilerResultsData;
	
	// Accumulate messages
	ThreadData->Messages.Append(Log->Messages);
	ThreadData->NumErrors += Log->NumErrors;
	ThreadData->NumWarnings += Log->NumWarnings;

	ThreadData->CurrentLogStack.Pop();

	if(ThreadData->CurrentLogStack.IsEmpty())
	{
		if(ThreadData->bDidCompile)
		{
			FMessageLog MessageLog("AnimNextCompilerResults");

			MessageLog.NewPage(FText::Format(LOCTEXT("CompileFormat", "Compile {0}: {1}"), JobName, FText::AsDateTime(FDateTime::UtcNow())));
			MessageLog.AddMessages(ThreadData->Messages);

			// Print summary
			FNumberFormattingOptions TimeFormat;
			TimeFormat.MaximumFractionalDigits = 2;
			TimeFormat.MinimumFractionalDigits = 2;
			TimeFormat.MaximumIntegralDigits = 4;
			TimeFormat.MinimumIntegralDigits = 4;
			TimeFormat.UseGrouping = false;

			FFormatNamedArguments Args;
			Args.Add(TEXT("CurrentTime"), FText::AsNumber(FinishTime - GStartTime, &TimeFormat));
			Args.Add(TEXT("JobName"), JobName);
			Args.Add(TEXT("CompileTime"), (int32)((FinishTime - StartTime) * 1000));
			Args.Add(TEXT("ObjectPath"), Object != nullptr ? FText::Format(LOCTEXT("ObjectPathFormat", "({0})"), FText::FromString(Object->GetPathName())) : FText::GetEmpty());

			if (ThreadData->NumErrors > 0)
			{
				Args.Add(TEXT("NumErrors"), ThreadData->NumErrors);
				Args.Add(TEXT("NumWarnings"), ThreadData->NumWarnings);
				MessageLog.Info(FText::Format(LOCTEXT("CompileFailed", "[{CurrentTime}] Compile of {JobName} failed. {NumErrors} Error(s), {NumWarnings} Warning(s) [in {CompileTime} ms] {ObjectPath}"), MoveTemp(Args)));
			}
			else if(ThreadData->NumWarnings > 0)
			{
				Args.Add(TEXT("NumWarnings"), ThreadData->NumWarnings);
				MessageLog.Info(FText::Format(LOCTEXT("CompileWarning", "[{CurrentTime}] Compile of {JobName} successful. {NumWarnings} Warning(s) [in {CompileTime} ms] {ObjectPath}"), MoveTemp(Args)));
			}
			else
			{
				MessageLog.Info(FText::Format(LOCTEXT("CompileSuccess", "[{CurrentTime}] Compile of {JobName} successful! [in {CompileTime} ms] {ObjectPath}"), MoveTemp(Args)));
			}
		}

		ThreadData->Messages.Empty();
		ThreadData->NumErrors = 0;
		ThreadData->NumWarnings = 0;
		ThreadData->bDidCompile = false;
	}
}

FCompilerResultsLog& FScopedCompilerResults::GetLog()
{
	FCompilerResultsThreadData* ThreadData = &GCompilerResultsData;
	check(ThreadData->CurrentLogStack.Num() && ThreadData->CurrentLogStack.Top().IsValid());
	return *ThreadData->CurrentLogStack.Top().Pin().Get();
}

}

#undef LOCTEXT_NAMESPACE