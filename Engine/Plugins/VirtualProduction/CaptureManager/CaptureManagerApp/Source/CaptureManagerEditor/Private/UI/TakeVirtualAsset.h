// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakeThumbnail.h"
#include "Ingest/LiveLinkDeviceCapability_Ingest.h"

#include "TakeVirtualAsset.generated.h"

UCLASS()
class UTakeVirtualAsset : public UObject
{
public:
	GENERATED_BODY()

	UTakeVirtualAsset() {}

	UTakeVirtualAsset(FGuid InCaptureDeviceId, UE::CaptureManager::FTakeId InTakeId, FTakeMetadata InMetadata)
		: CaptureDeviceId(InCaptureDeviceId)
		, TakeId(InTakeId)
		, Metadata(MoveTemp(InMetadata))
	{
	}

	FGuid CaptureDeviceId;
	UE::CaptureManager::FTakeId TakeId;
	FTakeMetadata Metadata;
	FTakeThumbnail Thumbnail;
};
