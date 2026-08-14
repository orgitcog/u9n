// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanVideoBaseLiveLinkSubject.h"

#include "MetaHumanPipelineVideoSourceNode.h"
#include "Nodes/ImageUtilNodes.h"
#include "Nodes/HyprsenseRealtimeNode.h"
#include "Nodes/NeutralFrameNode.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanVideoBaseLiveLinkSourceProcessing, Log, All);



FMetaHumanVideoBaseLiveLinkSubject::FMetaHumanVideoBaseLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanVideoBaseLiveLinkSubjectSettings* InSettings) : FMetaHumanMediaSamplerLiveLinkSubject(InLiveLinkClient, InSourceGuid, InSubjectName, InSettings)
{
	VideoBaseLiveLinkSubjectSettings = InSettings;

	AnalyticsItems.Add(TEXT("DeviceType"), TEXT("Video"));

	// Create pipeline

	VideoSource = MakeShared<UE::MetaHuman::Pipeline::FVideoSourceNode>("MediaPlayer");

	Rotation = MakeShared<UE::MetaHuman::Pipeline::FUEImageRotateNode>("Rotate");

	NeutralFrame = MakeShared<UE::MetaHuman::Pipeline::FNeutralFrameNode>("NeutralFrame");

	RealtimeMonoSolver = MakeShared<UE::MetaHuman::Pipeline::FHyprsenseRealtimeNode>("RealtimeMonoSolver");
	if (!RealtimeMonoSolver->LoadModels())
	{
		UE_LOG(LogMetaHumanVideoBaseLiveLinkSourceProcessing, Warning, TEXT("Failed to load realtime model"));
	}

	Pipeline.AddNode(VideoSource);
	Pipeline.AddNode(Rotation);
	Pipeline.AddNode(NeutralFrame);
	Pipeline.AddNode(RealtimeMonoSolver);

	Pipeline.MakeConnection(VideoSource, Rotation);
	Pipeline.MakeConnection(Rotation, NeutralFrame);
	Pipeline.MakeConnection(NeutralFrame, RealtimeMonoSolver);

	SetHeadOrientation(InSettings->bHeadOrientation);
	SetHeadTranslation(InSettings->bHeadTranslation);
	SetHeadStabilization(InSettings->bHeadStabilization);
	SetMonitorImage(InSettings->MonitorImage);
	SetRotation(InSettings->Rotation);
	SetFocalLength(InSettings->FocalLength);

	SolverStates.SetNumZeroed(StaticEnum<EHyprsenseRealtimeNodeState>()->NumEnums() - 1);
}

void FMetaHumanVideoBaseLiveLinkSubject::SetHeadOrientation(bool bInHeadOrientation)
{
	bHeadOrientation = bInHeadOrientation;

	AnalyticsItems.Add(TEXT("HeadOrientation"), LexToString(bHeadOrientation));
}

void FMetaHumanVideoBaseLiveLinkSubject::SetHeadTranslation(bool bInHeadTranslation)
{
	bHeadTranslation = bInHeadTranslation;

	AnalyticsItems.Add(TEXT("HeadTranslation"), LexToString(bHeadTranslation));
}

void FMetaHumanVideoBaseLiveLinkSubject::SetHeadStabilization(bool bInHeadStabilization)
{
	RealtimeMonoSolver->SetHeadStabilization(bInHeadStabilization);

	AnalyticsItems.Add(TEXT("HeadStabilization"), LexToString(bInHeadStabilization));
}

void FMetaHumanVideoBaseLiveLinkSubject::SetMonitorImage(EHyprsenseRealtimeNodeDebugImage InMonitorImage)
{
	if (!MonitorImageHistory.IsEmpty())
	{
		MonitorImageHistory += ", ";
	}
	MonitorImageHistory += UEnum::GetDisplayValueAsText(InMonitorImage).ToString();

	AnalyticsItems.Add(TEXT("MonitorImage"), MonitorImageHistory);

	RealtimeMonoSolver->SetDebugImage(InMonitorImage);
}

void FMetaHumanVideoBaseLiveLinkSubject::SetRotation(EMetaHumanVideoRotation InRotation)
{
	AnalyticsItems.Add(TEXT("Rotation"), UEnum::GetDisplayValueAsText(InRotation).ToString());

	float Angle = 0;

	switch (InRotation)
	{
		case EMetaHumanVideoRotation::Zero:
			Angle = 0;
			break;

		case EMetaHumanVideoRotation::Ninety:
			Angle = 90;
			break;

		case EMetaHumanVideoRotation::OneEighty:
			Angle = 180;
			break;

		case EMetaHumanVideoRotation::TwoSeventy:
			Angle = 270;
			break;

		default:
			UE_LOG(LogMetaHumanVideoBaseLiveLinkSourceProcessing, Warning, TEXT("Unsupported rotation"));
			break;
	};

	Rotation->SetAngle(Angle);
}

void FMetaHumanVideoBaseLiveLinkSubject::SetFocalLength(float InFocalLength)
{
	RealtimeMonoSolver->SetFocalLength(InFocalLength);
}

void FMetaHumanVideoBaseLiveLinkSubject::MarkNeutralFrame()
{
	NeutralFrame->bIsNeutralFrame = true;
}

void FMetaHumanVideoBaseLiveLinkSubject::ExtractPipelineData(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	int32 SolverState = InPipelineData->GetData<int32>(RealtimeMonoSolver->Name + TEXT(".State Out"));
	SolverStates[SolverState]++;

	if (InPipelineData->GetData<bool>(VideoSource->Name + TEXT(".Dropped Frame Out")))
	{
		DroppedFrames++;
	}

	Animation = InPipelineData->MoveData<FFrameAnimationData>(RealtimeMonoSolver->Name + TEXT(".Animation Out"));

	if (!bHeadOrientation)
	{
		Animation.Pose.SetRotation(FRotator(0, 90, 0).Quaternion());
	}

	bIsNeutralFrame = InPipelineData->GetData<bool>(NeutralFrame->Name + TEXT(".Neutral Frame Out"));
	if (bIsNeutralFrame)
	{
		if (InPipelineData->GetData<float>(RealtimeMonoSolver->Name + TEXT(".Confidence Out")) > 0.5)
		{
			VideoBaseLiveLinkSubjectSettings->FocalLength = InPipelineData->GetData<float>(RealtimeMonoSolver->Name + TEXT(".Focal Length Out"));
		}
		else
		{
			NeutralFrame->bIsNeutralFrame = true;
		}
	}

	HeadPoseMode = bHeadTranslation ? 1 : 0;

	HeadControlSwitch = (bHeadOrientation || bHeadTranslation) ? 1 : 0;

	SceneTime = InPipelineData->GetData<FQualifiedFrameTime>(VideoSource->Name + TEXT(".UE Image Sample Time Out"));

	// Latency timestamps
	Timestamps.Reset();
	Timestamps.Add(TEXT("Sample Timestamp"), SceneTime.AsSeconds());
	Timestamps.Add(TEXT("Processing Start"), InPipelineData->GetMarkerEndTime(VideoSource->Name));
	Timestamps.Add(TEXT("Processing End"), FDateTime::Now().GetTimeOfDay().GetTotalSeconds());
}

void FMetaHumanVideoBaseLiveLinkSubject::FinalizeAnalyticsItems()
{
	FString SolverStateString;

	for (int32 Index = 0; Index < SolverStates.Num(); ++Index)
	{
		if (Index != 0)
		{
			SolverStateString += TEXT(", ");
		}

		EHyprsenseRealtimeNodeState SolverState = static_cast<EHyprsenseRealtimeNodeState>(Index);
		SolverStateString += FString::Printf(TEXT("%s = %i"), *UEnum::GetDisplayValueAsText(SolverState).ToString(), SolverStates[Index]);
	}

	AnalyticsItems.Add(TEXT("SolverStates"), SolverStateString);

	AnalyticsItems.Add(TEXT("FocalLength"), LexToString(VideoBaseLiveLinkSubjectSettings->FocalLength));
	AnalyticsItems.Add(TEXT("HasCalibrationNeutral"), LexToString(!VideoBaseLiveLinkSubjectSettings->NeutralFrame.IsEmpty()));
	AnalyticsItems.Add(TEXT("HasHeadTranslationNeutral"), LexToString(VideoBaseLiveLinkSubjectSettings->NeutralHeadTranslation.Length() > 0));
	// Cant add smoothing to analytics as that would be an asset name and potentially EPGI
}

void FMetaHumanVideoBaseLiveLinkSubject::AddVideoSample(FVideoSample&& InVideoSample)
{
	UE::MetaHuman::Pipeline::FVideoSourceNode::FVideoSample PipelineVideoSample;

	PipelineVideoSample.Image.Width = InVideoSample.Width;
	PipelineVideoSample.Image.Height = InVideoSample.Height;
	PipelineVideoSample.Image.Data = MoveTemp(InVideoSample.Data);
	PipelineVideoSample.Time = InVideoSample.Time;
	PipelineVideoSample.TimeSource = InVideoSample.TimeSource;

	VideoSource->AddVideoSample(MoveTemp(PipelineVideoSample));
}

void FMetaHumanVideoBaseLiveLinkSubject::SetError(const FString& InErrorMessage)
{
	VideoSource->SetError(InErrorMessage);
}
