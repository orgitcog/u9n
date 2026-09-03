// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "ChaosVDRecordingDetails.generated.h"

UENUM()
enum class EChaosVDRecordingMode : uint8
{
	Invalid,
	Live,
	File
};

USTRUCT()
struct FChaosVDTraceDetails
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid TraceGuid;
	
	UPROPERTY()
	FGuid SessionGuid;

	UPROPERTY()
	FString TraceTarget;

	UPROPERTY()
	bool bIsConnected = false;

	UPROPERTY()
	EChaosVDRecordingMode Mode = EChaosVDRecordingMode::Invalid;
};