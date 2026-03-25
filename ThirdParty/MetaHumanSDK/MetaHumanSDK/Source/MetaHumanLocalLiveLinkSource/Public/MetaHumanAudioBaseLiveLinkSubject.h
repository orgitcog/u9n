// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanMediaSamplerLiveLinkSubject.h"
#include "MetaHumanAudioBaseLiveLinkSubjectSettings.h"



namespace UE::MetaHuman::Pipeline
{
	class FAudioSourceNode;
	class FRealtimeSpeechToAnimNode;
}

class METAHUMANLOCALLIVELINKSOURCE_API FMetaHumanAudioBaseLiveLinkSubject : public FMetaHumanMediaSamplerLiveLinkSubject
{
public:

	FMetaHumanAudioBaseLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanAudioBaseLiveLinkSubjectSettings* InSettings);

protected:

	class FAudioSample
	{
	public:
		int32 NumChannels = -1;
		int32 SampleRate = -1;
		int32 NumSamples = -1;
		TArray<float> Data;
		FQualifiedFrameTime Time;
		ETimeSource TimeSource = ETimeSource::NotSet;
	};

	void AddAudioSample(FAudioSample&& InAudioSample);
	void SetError(const FString& InErrorMessage);

	virtual void ExtractPipelineData(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData) override;

private:

	TSharedPtr<UE::MetaHuman::Pipeline::FAudioSourceNode> AudioSource;
	TSharedPtr<UE::MetaHuman::Pipeline::FRealtimeSpeechToAnimNode> RealtimeAudioSolver;
};