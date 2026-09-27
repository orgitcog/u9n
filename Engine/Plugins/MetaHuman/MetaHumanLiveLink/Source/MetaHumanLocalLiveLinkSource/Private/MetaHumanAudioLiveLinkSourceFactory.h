// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkSource.h"

#include "MetaHumanAudioLiveLinkSourceFactory.generated.h"



UCLASS()
class UMetaHumanAudioLiveLinkSourceFactory : public ULiveLinkSourceFactory
{
public:
	GENERATED_BODY()

	// ~ULiveLinkSourceFactory

	virtual FText GetSourceDisplayName() const override;
	virtual FText GetSourceTooltip() const override;
	virtual EMenuType GetMenuType() const override { return EMenuType::MenuEntry; }
	virtual TSharedPtr<ILiveLinkSource> CreateSource(const FString& InConnectionString) const override;

	// ~ULiveLinkSourceFactory
};
