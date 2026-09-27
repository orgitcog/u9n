// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanLocalLiveLinkSubject.h"

#include "HAL/RunnableThread.h"



class METAHUMANLOCALLIVELINKSOURCE_API FMetaHumanMediaSamplerLiveLinkSubject : public FMetaHumanLocalLiveLinkSubject
{
public:

	FMetaHumanMediaSamplerLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanLocalLiveLinkSubjectSettings* InSettings);

	virtual void Start() override;
	virtual void Stop() override;

protected:

	virtual void MediaSamplerMain() = 0;

private:

	class FMediaSamplerRunnable : public FRunnable
	{
	public:

		virtual uint32 Run() override;

		void Start(TFunction<void()> InMainFunction);

		TFunction<void()> MainFunction;

		TUniquePtr<FRunnableThread> Thread;
	};

	FMediaSamplerRunnable MediaSamplerRunnable;
};
