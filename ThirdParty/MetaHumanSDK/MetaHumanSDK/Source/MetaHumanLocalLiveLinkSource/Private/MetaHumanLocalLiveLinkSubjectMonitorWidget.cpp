// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanLocalLiveLinkSubjectMonitorWidget.h"
#include "MetaHumanLocalLiveLinkSubjectSettings.h"
#include "MetaHumanLocalLiveLinkSubject.h"

#define LOCTEXT_NAMESPACE "MetaHumanLocalLiveLinkSubjectMonitorWidget"



void SMetaHumanLocalLiveLinkSubjectMonitorWidget::Construct(const FArguments& InArgs, UMetaHumanLocalLiveLinkSubjectSettings* InSettings)
{
	Settings = InSettings;

	Settings->UpdateDelegate.AddSP(this, &SMetaHumanLocalLiveLinkSubjectMonitorWidget::OnUpdate);
	Settings->Subject->SendLatestUpdate();
}

void SMetaHumanLocalLiveLinkSubjectMonitorWidget::OnUpdate(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	// No need to ensure the update happens on EditorTick like in MHA since the problem that works around only 
	// effects in UEFN, and this code will never run in UEFN.

	check(IsInGameThread());

	const UE::MetaHuman::Pipeline::EPipelineExitStatus ExitStatus = InPipelineData->GetExitStatus();
	if (ExitStatus != UE::MetaHuman::Pipeline::EPipelineExitStatus::Unknown)
	{
		if (ExitStatus == UE::MetaHuman::Pipeline::EPipelineExitStatus::Ok || ExitStatus == UE::MetaHuman::Pipeline::EPipelineExitStatus::Aborted)
		{
			Settings->State = "Completed";
			Settings->StateLED = FColor::Green;
		}
		else
		{
			Settings->State = FString::Printf(TEXT("Error (%s)"), *InPipelineData->GetErrorNodeMessage());
			Settings->StateLED = FColor::Red;
		}
	}
	else
	{
		Settings->Frame = FString::Printf(TEXT("%05d"), InPipelineData->GetFrameNumber());

		double Now = FPlatformTime::Seconds();

		if (FPSCount == 0)
		{
			FPSStart = Now;
		}

		FPSCount++;

		if (Now - FPSStart > 2)
		{
			FPS = (FPSCount - 1) / (Now - FPSStart);
			FPSCount = 0;
		}

		if (FPS > 0)
		{
			Settings->FPS = FString::Printf(TEXT("%0.2f"), FPS);
		}
		else
		{
			Settings->FPS = TEXT("Calculating...");
		}
	}
}

#undef LOCTEXT_NAMESPACE
