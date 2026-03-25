// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanMediaSourceCreateParams.generated.h"



USTRUCT()
struct METAHUMANLOCALLIVELINKSOURCE_API FMetaHumanMediaSourceCreateParams
{
public:

	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Params")
	FString VideoName;

	UPROPERTY(EditAnywhere, Category = "Params")
	FString VideoURL;

	UPROPERTY(EditAnywhere, Category = "Params")
	int32 VideoTrack = -1;

	UPROPERTY(EditAnywhere, Category = "Params")
	int32 VideoTrackFormat = -1;

	UPROPERTY(EditAnywhere, Category = "Params")
	FString VideoTrackFormatName;

	UPROPERTY(EditAnywhere, Category = "Params")
	FString AudioName;

	UPROPERTY(EditAnywhere, Category = "Params")
	FString AudioURL;

	UPROPERTY(EditAnywhere, Category = "Params")
	int32 AudioTrack = -1;

	UPROPERTY(EditAnywhere, Category = "Params")
	int32 AudioTrackFormat = -1;

	UPROPERTY(EditAnywhere, Category = "Params")
	FString AudioTrackFormatName;

	UPROPERTY(EditAnywhere, Category = "Params")
	double StartTimeout = 5;

	UPROPERTY(EditAnywhere, Category = "Params")
	double FormatWaitTime = 0.1;

	UPROPERTY(EditAnywhere, Category = "Params")
	double SampleTimeout = 5;
};
