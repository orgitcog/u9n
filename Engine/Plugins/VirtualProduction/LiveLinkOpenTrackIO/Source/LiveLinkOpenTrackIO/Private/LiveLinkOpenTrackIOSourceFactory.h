// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkSourceFactory.h"
#include "LiveLinkOpenTrackIOSource.h"
#include "LiveLinkOpenTrackIOSourceFactory.generated.h"

UCLASS()
class LIVELINKOPENTRACKIO_API ULiveLinkOpenTrackIOSourceFactory : public ULiveLinkSourceFactory
{
public:
	GENERATED_BODY()

	virtual FText GetSourceDisplayName() const override;
	virtual FText GetSourceTooltip() const override;

	virtual EMenuType GetMenuType() const override { return EMenuType::SubPanel; }
	virtual TSharedPtr<SWidget> BuildCreationPanel(FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const override;
	virtual TSharedPtr<ILiveLinkSource> CreateSource(const FString& ConnectionString) const override;

private:
	void CreateSourceFromSettings(FLiveLinkOpenTrackIOConnectionSettings ConnectionSettings, FOnLiveLinkSourceCreated OnSourceCreated) const;
};
