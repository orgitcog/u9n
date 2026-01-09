// Copyright Epic Games, Inc.All Rights Reserved.

#include "MetaHumanMediaSamplerLiveLinkSubject.h"



FMetaHumanMediaSamplerLiveLinkSubject::FMetaHumanMediaSamplerLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanLocalLiveLinkSubjectSettings* InSettings) : FMetaHumanLocalLiveLinkSubject(InLiveLinkClient, InSourceGuid, InSubjectName, InSettings)
{
}

void FMetaHumanMediaSamplerLiveLinkSubject::Start()
{
	FMetaHumanLocalLiveLinkSubject::Start();

	MediaSamplerRunnable.Start([this]() { MediaSamplerMain(); });
}

void FMetaHumanMediaSamplerLiveLinkSubject::Stop()
{
	FMetaHumanLocalLiveLinkSubject::Stop();

	if (MediaSamplerRunnable.Thread.IsValid())
	{
		MediaSamplerRunnable.Thread->WaitForCompletion();
	}
}

void FMetaHumanMediaSamplerLiveLinkSubject::FMediaSamplerRunnable::Start(TFunction<void()> InMainFunction)
{
	MainFunction = InMainFunction;

	Thread.Reset(FRunnableThread::Create(this, TEXT("FMediaSamplerRunnable"), 0, TPri_Normal));
}

uint32 FMetaHumanMediaSamplerLiveLinkSubject::FMediaSamplerRunnable::Run()
{
	MainFunction();

	return 0;
}