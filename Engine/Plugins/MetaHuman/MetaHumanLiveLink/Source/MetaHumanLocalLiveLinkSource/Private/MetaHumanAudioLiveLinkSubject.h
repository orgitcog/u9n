// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanAudioBaseLiveLinkSubject.h"
#include "MetaHumanAudioLiveLinkSubjectSettings.h"
#include "MetaHumanPipelineMediaPlayerNode.h"



class METAHUMANLOCALLIVELINKSOURCE_API FMetaHumanAudioLiveLinkSubject : public FMetaHumanAudioBaseLiveLinkSubject
{
public:

	FMetaHumanAudioLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanAudioLiveLinkSubjectSettings* InSettings);
	virtual ~FMetaHumanAudioLiveLinkSubject();

protected:

	virtual void MediaSamplerMain() override;

private:

	TSharedPtr<UE::MetaHuman::Pipeline::FMediaPlayerNode> MediaPlayer;
};