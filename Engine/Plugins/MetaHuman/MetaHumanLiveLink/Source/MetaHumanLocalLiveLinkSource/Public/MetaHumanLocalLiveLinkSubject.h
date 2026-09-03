// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanLocalLiveLinkSubjectSettings.h"

#include "HAL/Runnable.h"
#include "ILiveLinkClient.h"
#include "Pipeline/Pipeline.h"

METAHUMANLOCALLIVELINKSOURCE_API DECLARE_LOG_CATEGORY_EXTERN(LogMetaHumanLocalLiveLinkSubject, Log, All);



class METAHUMANLOCALLIVELINKSOURCE_API FMetaHumanLocalLiveLinkSubject : public FRunnable
{
public:

	FMetaHumanLocalLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanLocalLiveLinkSubjectSettings* InSettings);
	virtual ~FMetaHumanLocalLiveLinkSubject() override;

	//~ Begin FRunnable interface
	virtual uint32 Run() override;
	virtual void Stop() override;
	//~ End FRunnable interface

	virtual void Start();
	void SendLatestUpdate();

	void RemoveSubject();

	enum class ETimeSource : uint8
	{
		NotSet = 0,
		System,
		Media,
	};

	static void GetSampleTime(const FFrameRate& InFrameRate, FQualifiedFrameTime& OutSampleTime, ETimeSource& OutSampleTimeSource);
	static void GetSampleTime(const TOptional<FTimecode>& InOptionalTimecode, const FFrameRate& InFrameRate, FQualifiedFrameTime& OutSampleTime, ETimeSource& OutSampleTimeSource);

protected:

	/** Name to use as the Live Link subject */
	FName SubjectName;

	virtual void ExtractPipelineData(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData) = 0;

	virtual void FinalizeAnalyticsItems() { }

	/** Processing pipeline */
	UE::MetaHuman::Pipeline::FPipeline Pipeline;

	FFrameAnimationData Animation;
	bool bIsNeutralFrame = false;
	int32 HeadPoseMode = 0;
	TMap<FName, double> Timestamps;
	FQualifiedFrameTime SceneTime;
	float HeadControlSwitch = 0;

	bool IsRunning() const;
	const bool* GetIsRunningPtr() const;

	TMap<FString, FString> AnalyticsItems;

private:

	/** The Live Link client used to push Live Link data to the editor */
	ILiveLinkClient* LiveLinkClient = nullptr;

	/** The GUID of the Live Link Source */
	FGuid SourceGuid;

	/** The thread currently running this instance of the class */
	TUniquePtr<FRunnableThread> Thread;

	TWeakObjectPtr<UMetaHumanLocalLiveLinkSubjectSettings> Settings;

	bool bStopTask = false;
	bool bPipelineCompleted = true;
	double ProcessingStarted = 0;
	int32 NumProcessedFrames = 0;
	int32 NumAnimationFrames = 0;
	double LatencyTotal = 0;

	/** Processing pipeline support */
	bool bIsFirstFrame = false;

	void FrameComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);
	void ProcessComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);
	TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> ProcessCompletePipelineData;

	/** Push new static data through Live Link */
	FLiveLinkBaseStaticData StaticData;
	bool PushStaticData();

	/** Push a new frame through Live Link */
	bool PushFrameData();

	void SendAnalytics();
};