// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanMediaSamplerLiveLinkSubject.h"
#include "MetaHumanVideoBaseLiveLinkSubjectSettings.h"



namespace UE::MetaHuman::Pipeline
{
	class FNeutralFrameNode;
	class FVideoSourceNode;
	class FUEImageRotateNode;
	class FHyprsenseRealtimeNode;
}

class METAHUMANLOCALLIVELINKSOURCE_API FMetaHumanVideoBaseLiveLinkSubject : public FMetaHumanMediaSamplerLiveLinkSubject
{
public:

	FMetaHumanVideoBaseLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanVideoBaseLiveLinkSubjectSettings* InSettings);

	void SetHeadOrientation(bool bInHeadOrientation);
	void SetHeadTranslation(bool bInHeadTranslation);
	void SetHeadStabilization(bool bInHeadStabilization);
	void SetMonitorImage(EHyprsenseRealtimeNodeDebugImage InMonitorImage);
	void SetRotation(EMetaHumanVideoRotation InRotation);
	void SetFocalLength(float bInFocalLength);

	void MarkNeutralFrame();

protected:

	class FVideoSample
	{
	public:
		int32 Width = 0;
		int32 Height = 0;
		TArray<uint8> Data;
		FQualifiedFrameTime Time;
		ETimeSource TimeSource = ETimeSource::NotSet;
	};

	void AddVideoSample(FVideoSample&& InVideoSample);
	void SetError(const FString& InErrorMessage);

	virtual void ExtractPipelineData(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData) override;

	virtual void FinalizeAnalyticsItems() override;

private:

	UMetaHumanVideoBaseLiveLinkSubjectSettings* VideoBaseLiveLinkSubjectSettings = nullptr;

	bool bHeadOrientation = true;
	bool bHeadTranslation = true;

	FString MonitorImageHistory;
	TArray<int32> SolverStates;
	int32 DroppedFrames = 0;

	TSharedPtr<UE::MetaHuman::Pipeline::FNeutralFrameNode> NeutralFrame;
	TSharedPtr<UE::MetaHuman::Pipeline::FVideoSourceNode> VideoSource;
	TSharedPtr<UE::MetaHuman::Pipeline::FUEImageRotateNode> Rotation;
	TSharedPtr<UE::MetaHuman::Pipeline::FHyprsenseRealtimeNode> RealtimeMonoSolver;
};