// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UIngestJobSettings.h"

#include "IngestJob.h"

namespace UE::CaptureManager
{

class FIngestJobSettingsManager
{
public:
	FIngestJobSettingsManager();

	void ApplyJobSpecificSettings(const FGuid& InJobGuid, const FIngestJob::FSettings& InSettings);
	TArray<TWeakObjectPtr<UIngestJobSettings>> GetSettings(const TArray<FGuid>& InJobGuids) const;

	int32 RemoveSettings(const TArray<FGuid>& InJobGuids);

private:
	mutable FCriticalSection CriticalSection;
	TArray<TStrongObjectPtr<UIngestJobSettings>> Settings;
};
}