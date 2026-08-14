// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanVideoLiveLinkSourceFactory.h"
#include "MetaHumanVideoLiveLinkSource.h"

#define LOCTEXT_NAMESPACE "MetaHumanVideoLiveLinkSourceFactory"



FText UMetaHumanVideoLiveLinkSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "MetaHuman (Video)");
}

FText UMetaHumanVideoLiveLinkSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "MetaHuman facial animation from a local video source, eg webcam, movie file.");
}

TSharedPtr<ILiveLinkSource> UMetaHumanVideoLiveLinkSourceFactory::CreateSource(const FString& InConnectionString) const
{
	return MakeShared<FMetaHumanVideoLiveLinkSource>();
}

#undef LOCTEXT_NAMESPACE
