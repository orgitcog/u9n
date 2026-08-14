// Copyright Epic Games, Inc. All Rights Reserved.

#include "Verification/VerifyObjectValid.h"

#include "MetaHumanAssetReport.h"

#include "Misc/DataValidation.h"
#include "Misc/RuntimeErrors.h"

#define LOCTEXT_NAMESPACE "VerifyObjectValid"

void UVerifyObjectValid::Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const
{
	if (!ensureAsRuntimeWarning(ToVerify) || !ensureAsRuntimeWarning(Report))
	{
		return;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), FText::FromString(ToVerify->GetName()));
	FMetaHumanAssetReportItem Item;
	Item.ProjectItem = ToVerify;
	if (!ToVerify->IsAsset())
	{
		Item.Message = FText::Format(LOCTEXT("ObjectNotAnAsset", "The UObject {AssetName} is not an asset"), Args);
		Report->AddError(Item);
	}
	else
	{
		Item.Message = FText::Format(LOCTEXT("ObjectIsAnAsset", "Verifying the asset {AssetName}"), Args);
		Report->AddVerbose(Item);
	}

	FDataValidationContext Context(false, EDataValidationUsecase::Script, {});
	if (ToVerify->IsDataValid(Context) == EDataValidationResult::Invalid)
	{
		Item.Message = FText::Format(LOCTEXT("ObjectFailedDataValidation", "{AssetName} has failed UE Data Validation"), Args);
		Report->AddError(Item);
	}

	for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
	{
		Args.Add(TEXT("InnerMessage"), Issue.Message);
		Item.Message = FText::Format(LOCTEXT("WrappedUeDataValidationMessage", "{AssetName} Data Validation: {InnerMessage}"), Args);
		if (Issue.Severity == EMessageSeverity::Error)
		{
			Report->AddError(Item);
		}
		if (Issue.Severity == EMessageSeverity::Warning || Issue.Severity == EMessageSeverity::PerformanceWarning)
		{
			Report->AddWarning(Item);
		}
		if (Issue.Severity == EMessageSeverity::Info)
		{
			Report->AddInfo(Item);
		}
	}
}

#undef LOCTEXT_NAMESPACE
