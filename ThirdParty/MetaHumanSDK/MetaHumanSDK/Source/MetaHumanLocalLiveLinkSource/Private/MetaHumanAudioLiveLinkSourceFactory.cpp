// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAudioLiveLinkSourceFactory.h"
#include "MetaHumanAudioLiveLinkSource.h"

#define LOCTEXT_NAMESPACE "MetaHumanAudioLiveLinkSourceFactory"



FText UMetaHumanAudioLiveLinkSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "MetaHuman (Audio)");
}

FText UMetaHumanAudioLiveLinkSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "MetaHuman facial animation from a local microphone.");
}

TSharedPtr<ILiveLinkSource> UMetaHumanAudioLiveLinkSourceFactory::CreateSource(const FString& InConnectionString) const
{
	return MakeShared<FMetaHumanAudioLiveLinkSource>();
}

#undef LOCTEXT_NAMESPACE
