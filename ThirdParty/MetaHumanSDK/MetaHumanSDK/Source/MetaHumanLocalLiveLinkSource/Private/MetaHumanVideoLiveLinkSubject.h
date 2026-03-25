// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanVideoBaseLiveLinkSubject.h"
#include "MetaHumanVideoLiveLinkSubjectSettings.h"
#include "MetaHumanPipelineMediaPlayerNode.h"



class METAHUMANLOCALLIVELINKSOURCE_API FMetaHumanVideoLiveLinkSubject : public FMetaHumanVideoBaseLiveLinkSubject
{
public:

	FMetaHumanVideoLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanVideoLiveLinkSubjectSettings* InSettings);
	virtual ~FMetaHumanVideoLiveLinkSubject();

protected:

	virtual void MediaSamplerMain() override;

private:

	TSharedPtr<UE::MetaHuman::Pipeline::FMediaPlayerNode> MediaPlayer;
};