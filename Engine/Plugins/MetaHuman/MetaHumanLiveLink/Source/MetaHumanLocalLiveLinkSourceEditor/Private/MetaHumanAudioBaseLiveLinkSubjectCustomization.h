// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanLocalLiveLinkSubjectCustomization.h"



class FMetaHumanAudioBaseLiveLinkSubjectCustomization : public FMetaHumanLocalLiveLinkSubjectCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
};
