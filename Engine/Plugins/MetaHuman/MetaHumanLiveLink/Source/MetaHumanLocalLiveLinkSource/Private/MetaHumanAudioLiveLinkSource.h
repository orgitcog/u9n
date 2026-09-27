// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanLocalLiveLinkSource.h"



class FMetaHumanAudioLiveLinkSource : public FMetaHumanLocalLiveLinkSource
{
public:

	virtual FText GetSourceType() const override;
	virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override;

protected:

	virtual TSharedPtr<FMetaHumanLocalLiveLinkSubject> CreateSubject(const FName& InSubjectName, UMetaHumanLocalLiveLinkSubjectSettings* InSettings) override;
};
