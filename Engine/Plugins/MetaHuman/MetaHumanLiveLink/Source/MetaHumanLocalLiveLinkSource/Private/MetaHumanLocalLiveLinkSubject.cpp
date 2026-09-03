// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanLocalLiveLinkSubject.h"
#include "MetaHumanHeadTransform.h"

#include "GuiToRawControlsUtils.h"

#include "HAL/RunnableThread.h"
#include "Async/Async.h"
#include "EngineAnalytics.h"
#include "Engine/Engine.h"
#include "Roles/LiveLinkBasicRole.h"

DEFINE_LOG_CATEGORY(LogMetaHumanLocalLiveLinkSubject);



FMetaHumanLocalLiveLinkSubject::FMetaHumanLocalLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanLocalLiveLinkSubjectSettings* InSettings)
{
	LiveLinkClient = InLiveLinkClient;
	SourceGuid = InSourceGuid;
	SubjectName = InSubjectName;
	Settings = InSettings;
}

FMetaHumanLocalLiveLinkSubject::~FMetaHumanLocalLiveLinkSubject()
{
	Stop();

	if (Thread.IsValid())
	{
		Thread->WaitForCompletion();
	}
}

void FMetaHumanLocalLiveLinkSubject::Start()
{
	Thread.Reset(FRunnableThread::Create(this, TEXT("FMetaHumanLocalLiveLinkSourceProcessing"), 0, TPri_BelowNormal));
}

uint32 FMetaHumanLocalLiveLinkSubject::Run()
{
	UE_LOG(LogMetaHumanLocalLiveLinkSubject, Display, TEXT("Started"));

	UE::MetaHuman::Pipeline::FFrameComplete OnFrameComplete;
	OnFrameComplete.AddRaw(this, &FMetaHumanLocalLiveLinkSubject::FrameComplete);

	UE::MetaHuman::Pipeline::FProcessComplete OnProcessComplete;
	OnProcessComplete.AddRaw(this, &FMetaHumanLocalLiveLinkSubject::ProcessComplete);

	UE::MetaHuman::Pipeline::FPipelineRunParameters PipelineRunParameters;
	PipelineRunParameters.SetMode(UE::MetaHuman::Pipeline::EPipelineMode::PushSyncNodes);
	PipelineRunParameters.SetOnFrameComplete(OnFrameComplete);
	PipelineRunParameters.SetOnProcessComplete(OnProcessComplete);
	PipelineRunParameters.SetRestrictStartingToGameThread(false);
//	PipelineRunParameters.SetVerbosity(ELogVerbosity::VeryVerbose); // uncomment for full logging

	bIsFirstFrame = true;
	bPipelineCompleted = false;
	ProcessingStarted = FPlatformTime::Seconds();

	// If analytics shutdowns while processing is running, because editor was closed, ensure analytics are still sent.
	// It would be too late to try to send them after pipeline completes.
#if WITH_EDITOR
	FDelegateHandle AnalyticsShutdownHandler = FEngineAnalytics::OnShutdownEngineAnalytics.AddRaw(this, &FMetaHumanLocalLiveLinkSubject::SendAnalytics);
#endif

	Pipeline.Run(PipelineRunParameters);

	SendAnalytics();

#if WITH_EDITOR
	FEngineAnalytics::OnShutdownEngineAnalytics.Remove(AnalyticsShutdownHandler);
#endif

	UE_LOG(LogMetaHumanLocalLiveLinkSubject, Display, TEXT("Finished"));

	return 0;
}

void FMetaHumanLocalLiveLinkSubject::Stop()
{
	bStopTask = true;
	bPipelineCompleted = true;

	Pipeline.Cancel();
}

void FMetaHumanLocalLiveLinkSubject::SendLatestUpdate()
{
	if (ProcessCompletePipelineData.IsValid())
	{
		AsyncTask(ENamedThreads::GameThread, [SettingsCopy = Settings, ProcessCompletePipelineDataCopy = ProcessCompletePipelineData]() {
			if (SettingsCopy.IsValid())
			{
				SettingsCopy->UpdateDelegate.Broadcast(ProcessCompletePipelineDataCopy);
			}
		});
	}
}

void FMetaHumanLocalLiveLinkSubject::RemoveSubject()
{
	AsyncTask(ENamedThreads::GameThread, [this]() {
		if (!bStopTask)
		{
			Stop();

			LiveLinkClient->RemoveSubject_AnyThread({ SourceGuid, SubjectName });
		}
	});
}

void FMetaHumanLocalLiveLinkSubject::FrameComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	NumProcessedFrames++;

	AsyncTask(ENamedThreads::GameThread, [SettingsCopy = Settings, InPipelineData]() {
		if (SettingsCopy.IsValid())
		{
			SettingsCopy->UpdateDelegate.Broadcast(InPipelineData);
		}
	});

	ExtractPipelineData(InPipelineData);

	if (Timestamps.Num() > 1)
	{
		Timestamps.ValueSort(TLess<double>());

		TArray<double> SortedValues;
		Timestamps.GenerateValueArray(SortedValues);

		LatencyTotal += SortedValues[SortedValues.Num() - 1] - SortedValues[0];
	}

	if (bIsFirstFrame)
	{
		bIsFirstFrame = false;
		PushStaticData();
	}

	if (Animation.AnimationData.IsEmpty())
	{
		return;
	}

	NumAnimationFrames++;

	PushFrameData();
}

void FMetaHumanLocalLiveLinkSubject::ProcessComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	bPipelineCompleted = true;

	ProcessCompletePipelineData = InPipelineData;

	AsyncTask(ENamedThreads::GameThread, [SettingsCopy = Settings, InPipelineData]() {
		if (SettingsCopy.IsValid())
		{
			SettingsCopy->UpdateDelegate.Broadcast(InPipelineData);
		}
	});
}

bool FMetaHumanLocalLiveLinkSubject::PushStaticData()
{
	StaticData = FLiveLinkBaseStaticData();

	const TMap<FString, float> RawControls = GuiToRawControlsUtils::ConvertGuiToRawControls(TMap<FString, float> {});
	for (const TPair<FString, float>& RawControl : RawControls)
	{
		StaticData.PropertyNames.Add(FName(RawControl.Key));
	}

	StaticData.PropertyNames.Add("HeadControlSwitch");
	StaticData.PropertyNames.Add("HeadRoll");
	StaticData.PropertyNames.Add("HeadPitch");
	StaticData.PropertyNames.Add("HeadYaw");
	StaticData.PropertyNames.Add("HeadTranslationX");
	StaticData.PropertyNames.Add("HeadTranslationY");
	StaticData.PropertyNames.Add("HeadTranslationZ");

	StaticData.PropertyNames.Add("MHFDSVersion");
	StaticData.PropertyNames.Add("DisableFaceOverride");

	FLiveLinkStaticDataStruct StaticDataStruct = FLiveLinkStaticDataStruct(FLiveLinkBaseStaticData::StaticStruct());
	*StaticDataStruct.Cast<FLiveLinkBaseStaticData>() = StaticData;

	if (!bStopTask)
	{
		LiveLinkClient->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkBasicRole::StaticClass(), MoveTemp(StaticDataStruct));
	}

	UE_LOG(LogMetaHumanLocalLiveLinkSubject, Display, TEXT("New static data for subject \"%s\""), *SubjectName.ToString());

	return true;
}

bool FMetaHumanLocalLiveLinkSubject::PushFrameData()
{
	FLiveLinkFrameDataStruct FrameDataStruct = FLiveLinkFrameDataStruct(FLiveLinkBaseFrameData::StaticStruct());
	FLiveLinkBaseFrameData& FrameData = *FrameDataStruct.Cast<FLiveLinkBaseFrameData>();

	for (const TPair<FString, float>& AnimationData : Animation.AnimationData)
	{
		FrameData.PropertyValues.Add(AnimationData.Value);
	}

	// Make animation pose relative to the head bone
	const FTransform HeadPose = FMetaHumanHeadTransform::MeshToBone(Animation.Pose);

	const FRotator HeadRotator = HeadPose.Rotator();
	const FVector HeadTranslation = Animation.Pose.GetTranslation();

	FrameData.PropertyValues.Add(HeadControlSwitch);
	FrameData.PropertyValues.Add(HeadRotator.Roll);
	FrameData.PropertyValues.Add(HeadRotator.Pitch);
	FrameData.PropertyValues.Add(HeadRotator.Yaw);
	FrameData.PropertyValues.Add(HeadTranslation.X);
	FrameData.PropertyValues.Add(HeadTranslation.Y);
	FrameData.PropertyValues.Add(HeadTranslation.Z);

	FrameData.PropertyValues.Add(1); // MHFDSVersion
	FrameData.PropertyValues.Add(1); // DisableFaceOverride

#if WITH_EDITOR
	FrameData.Timestamps = Timestamps;
#endif

	FrameData.MetaData.SceneTime = SceneTime;

	FrameData.MetaData.StringMetaData.Add("IsNeutralFrame", bIsNeutralFrame ? "true" : "false");
	FrameData.MetaData.StringMetaData.Add("HeadPoseMode", FString::Printf(TEXT("%i"), HeadPoseMode));

	Settings->PreProcess(StaticData, FrameData);

	if (!bStopTask)
	{
		LiveLinkClient->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(FrameDataStruct));
	}

	UE_LOG(LogMetaHumanLocalLiveLinkSubject, Verbose, TEXT("New frame"));

	return true;
}

bool FMetaHumanLocalLiveLinkSubject::IsRunning() const 
{ 
	return !bPipelineCompleted;
}

const bool* FMetaHumanLocalLiveLinkSubject::GetIsRunningPtr() const 
{ 
	return &bPipelineCompleted;
}

void FMetaHumanLocalLiveLinkSubject::GetSampleTime(const FFrameRate& InFrameRate, FQualifiedFrameTime& OutSampleTime, ETimeSource& OutSampleTimeSource)
{
	const FTimecode Timecode = FTimecode::FromTimespan(FDateTime::Now().GetTimeOfDay(), InFrameRate, false);
	OutSampleTime = FQualifiedFrameTime(Timecode, InFrameRate);
	OutSampleTimeSource = ETimeSource::System; // from system time of day
}

void FMetaHumanLocalLiveLinkSubject::GetSampleTime(const TOptional<FTimecode>& InOptionalTimecode, const FFrameRate& InFrameRate, FQualifiedFrameTime& OutSampleTime, ETimeSource& OutSampleTimeSource)
{
	if (InOptionalTimecode.IsSet())
	{
		OutSampleTime = FQualifiedFrameTime(InOptionalTimecode.GetValue(), InFrameRate);
		OutSampleTimeSource = ETimeSource::Media; // from media
	}
	else
	{
		FMetaHumanLocalLiveLinkSubject::GetSampleTime(InFrameRate, OutSampleTime, OutSampleTimeSource);
	}
}

void FMetaHumanLocalLiveLinkSubject::SendAnalytics()
{
	if (GEngine->AreEditorAnalyticsEnabled() && FEngineAnalytics::IsAvailable())
	{
		FinalizeAnalyticsItems();

		AnalyticsItems.Add(TEXT("NumProcessedFrames"), LexToString(NumProcessedFrames));
		AnalyticsItems.Add(TEXT("NumAnimationFrames"), LexToString(NumAnimationFrames));
		AnalyticsItems.Add(TEXT("Duration"), LexToString(FPlatformTime::Seconds() - ProcessingStarted));
		if (LatencyTotal > 0 && NumProcessedFrames > 0)
		{
			AnalyticsItems.Add(TEXT("Latency"), LexToString((LatencyTotal / NumProcessedFrames) * 1000)); // In milliseconds
		}

		TArray<FAnalyticsEventAttribute> AnalyticsEvents;

		for (const TPair<FString, FString>& AnalyticsItem : AnalyticsItems)
		{
			AnalyticsEvents.Add(FAnalyticsEventAttribute(AnalyticsItem.Key, AnalyticsItem.Value));
		}

#if 0
		for (const FAnalyticsEventAttribute& Attr : AnalyticsEvents)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] [%s]"), *Attr.GetName(), *Attr.GetValue());
		}
#else
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.MetaHumanLiveLinkPlugin.ProcessInfo"), AnalyticsEvents);
#endif
	}
}
