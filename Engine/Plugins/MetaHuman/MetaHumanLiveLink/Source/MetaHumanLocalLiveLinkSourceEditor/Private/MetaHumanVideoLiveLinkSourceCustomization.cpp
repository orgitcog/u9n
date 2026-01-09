// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanVideoLiveLinkSourceCustomization.h"



TSharedRef<IDetailCustomization> FMetaHumanVideoLiveLinkSourceCustomization::MakeInstance()
{
	return MakeShared<FMetaHumanVideoLiveLinkSourceCustomization>();
}

void FMetaHumanVideoLiveLinkSourceCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	constexpr bool bIsVideo = true;

	Setup(InDetailBuilder, bIsVideo);
}
