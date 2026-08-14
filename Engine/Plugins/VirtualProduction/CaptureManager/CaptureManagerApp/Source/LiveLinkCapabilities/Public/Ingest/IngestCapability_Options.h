// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkDevice.h"

#include "IngestCapability_Options.generated.h"

UENUM()
enum class EIngestCapability_ImagePixelFormat
{
	Undefined = -1		UMETA(Hidden),
	U8_RGB = 0			UMETA(DisplayName = "RGB 8-bit"),
	U8_BGR				UMETA(DisplayName = "BGR 8-bit"),
	U8_RGBA				UMETA(DisplayName = "RGBA 8-bit"),
	U8_BGRA				UMETA(DisplayName = "BGRA 8-bit"),
	U8_I444				UMETA(DisplayName = "I444 8-bit"),
	U8_I420				UMETA(DisplayName = "I420 8-bit"),
	U8_NV12				UMETA(DisplayName = "NV12 8-bit"),
	U8_Mono				UMETA(DisplayName = "Mono 8-bit"),
	U16_Mono			UMETA(DisplayName = "Mono 16-bit"),
	F_Mono				UMETA(DisplayName = "Mono Float"),

	Default = U8_RGB	UMETA(DisplayName = "Default (U8_RGB)")
};

UENUM()
enum class EIngestCapability_ImageRotation
{
	None = 0			UMETA(DisplayName = "None"),
	CW_90				UMETA(DisplayName = "Clockwise 90°"),
	CW_180				UMETA(DisplayName = "Clockwise 180°"),
	CW_270				UMETA(DisplayName = "Clockwise 270°"),

	Default = None		UMETA(DisplayName = "Default (None)")
};

USTRUCT(BlueprintType)
struct LIVELINKCAPABILITIES_API FIngestCapability_VideoOptions
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Device|Ingest")
	FString FileNamePrefix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Device|Ingest")
	FString Format;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Device|Ingest")
	EIngestCapability_ImagePixelFormat PixelFormat = EIngestCapability_ImagePixelFormat::U8_BGRA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Device|Ingest")
	EIngestCapability_ImageRotation Rotation = EIngestCapability_ImageRotation::None;
};

USTRUCT(BlueprintType)
struct LIVELINKCAPABILITIES_API FIngestCapability_AudioOptions
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Device|Ingest")
	FString FileNamePrefix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Device|Ingest")
	FString Format;
};

UCLASS(BlueprintType)
class LIVELINKCAPABILITIES_API UIngestCapability_Options : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Device|Ingest")
	FString WorkingDirectory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Device|Ingest")
	FString DownloadDirectory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Device|Ingest")
	FIngestCapability_VideoOptions Video;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Device|Ingest")
	FIngestCapability_AudioOptions Audio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Device|Ingest")
	FString UploadHostName;
};
