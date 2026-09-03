// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAudioBaseLiveLinkSubjectMonitorWidget.h"
#include "MetaHumanAudioBaseLiveLinkSubject.h"

#include "Widgets/Notifications/SProgressBar.h"



// A very simple audio level meter - it aint no VU meter! Just display the maximum PCM amplitude.
// A more useable meter would need to work out db levels and average over time. But this meter is 
// good enough for a simple "microphone working or not" check.

void SMetaHumanAudioBaseLiveLinkSubjectMonitorWidget::Construct(const FArguments& InArgs, UMetaHumanAudioBaseLiveLinkSubjectSettings* InSettings)
{
	Settings = InSettings;

	Settings->UpdateDelegate.AddSP(this, &SMetaHumanAudioBaseLiveLinkSubjectMonitorWidget::OnUpdate);
	Settings->Subject->SendLatestUpdate();

	ChildSlot
	[
		SNew(SProgressBar)
		.Percent_Lambda([this]()
		{
			return Settings->Level;
		})
	];
}

void SMetaHumanAudioBaseLiveLinkSubjectMonitorWidget::OnUpdate(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	check(IsInGameThread());

	const UE::MetaHuman::Pipeline::EPipelineExitStatus ExitStatus = InPipelineData->GetExitStatus();
	if (ExitStatus != UE::MetaHuman::Pipeline::EPipelineExitStatus::Unknown)
	{
		Settings->Level = 0;
	}
	else
	{
		Settings->State = "OK";
		Settings->StateLED = FColor::Green;

		const FString AudioPin = TEXT("MediaPlayer.Audio Out");
		const FString AudioSampleTimePin = TEXT("MediaPlayer.Audio Sample Time Out");

		const UE::MetaHuman::Pipeline::FAudioDataType& Audio = InPipelineData->GetData<UE::MetaHuman::Pipeline::FAudioDataType>(AudioPin);

		const int32 NumDataItems = Audio.NumSamples * Audio.NumChannels;
		const float* Data = Audio.Data.GetData();

		Settings->Level = 0;
		for (int32 Index = 0; Index < NumDataItems; Index++)
		{
			Settings->Level = FMath::Max(Settings->Level, fabs(Data[Index]));
		}

		FTimecode Timecode = InPipelineData->GetData<FQualifiedFrameTime>(AudioSampleTimePin).ToTimecode();
		Timecode.Subframe = 0; // For the purpose of display, ignore subframe - just looks wrong
		Settings->Timecode = Timecode.ToString();
	}
}
