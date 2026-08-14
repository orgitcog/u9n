// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAudioLiveLinkSourceCustomization.h"



TSharedRef<IDetailCustomization> FMetaHumanAudioLiveLinkSourceCustomization::MakeInstance()
{
	return MakeShared<FMetaHumanAudioLiveLinkSourceCustomization>();
}

void FMetaHumanAudioLiveLinkSourceCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	constexpr bool bIsVideo = false;

	Setup(InDetailBuilder, bIsVideo);
}
