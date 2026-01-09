// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFootageIngestAPI.h"
#include "IMediaTextureSample.h"
#include "Dom/JsonObject.h"

struct FLiveLinkFaceStaticFileNames
{
	inline static const FString AudioMetadata = "audio_metadata.json";
	inline static const FString DepthData = "depth_data.bin";
	inline static const FString DepthMetadata = "depth_metadata.mhaical";
	inline static const FString FrameLog = "frame_log.csv";
	inline static const FString TakeMetadata = "take.json";
	inline static const FString Thumbnail = "thumbnail.jpg";
	inline static const FString VideoMetadata = "video_metadata.json";
};

struct FLiveLinkFaceTakeMetadata
{
	int32 Version;
	FString SlateName;
	FString AppVersion;
	FString DeviceModel;
	FString Subject;
	FString Identifier;
	FDateTime Date;
	int32 TakeNumber;
	int32 NumFrames;
	bool bIsCalibrated;

	FString MOVFileName() const;
	TArray<FString> GetMHAFileNames() const;
	TArray<FString> GetCommonFileNames() const;
	TArray<FString> GetARKitFileNames() const;

private:
	FString CommonFileNamePrefix() const;
	TArray<FString> GetCalibratedBlendshapeFileNames() const;
};

struct FLiveLinkFaceVideoMetadata
{
	FIntPoint Resolution;
	float JpegCompressionQuality;
	FString Quality;
	EMediaOrientation Orientation;
	double FrameRate;
};

struct FLiveLinkFaceOodleMetadata
{
	FString Compressor;
	FString CompressionLevel;
	FString Version;
};

struct FLiveLinkFaceDepthMetadata
{
	FIntPoint Resolution;
	FString Compression;
	FLiveLinkFaceOodleMetadata OodleInfo; // If Compression == "Oodle"
	EMediaOrientation Orientation;
	double FrameRate;
	float PixelSize;
	bool bShouldCompressFiles;

	// Lens distortion info
	TArray<float> LensDistortionLookupTable;
	TArray<float> InverseLensDistortionLookupTable;
	TArray<float> IntrinsicMatrix;
	FVector2D LensDistortionCenter;
	FVector2D IntrinsicMatrixReferenceDimensions;
};

struct FLiveLinkFaceAudioMetadata
{
	int32 BitsPerChannel;
	int32 SampleRate;
	int32 ChannelsPerFrame;
	int32 FormatFlags;
};

struct FLiveLinkFaceTakeInfo
{
	// The path to the folder where the files for this take are
	FString TakeOriginDirectory;

	TakeId Id = INVALID_ID;

	TArray<uint8> RawThumbnailData;
	FLiveLinkFaceTakeMetadata TakeMetadata;
	FLiveLinkFaceVideoMetadata VideoMetadata;
	FLiveLinkFaceDepthMetadata DepthMetadata;
	FLiveLinkFaceAudioMetadata AudioMetadata;

	TArray<FText> Issues;

	FString GetTakeName() const;
	FString GetTakePath() const;
	FString GetMOVFilePath() const;
	FString GetDepthFilePath() const;
	FString GetFrameLogFilePath() const;
	FString GetCameraCalibrationFilePath() const;
	FString GetOutputDirectory(const FString& InTakesOriginDirectory) const;
	float GetTakeDurationInSeconds() const;

	FMetaHumanTakeInfo ConvertToMetaHumanTakeInfo(const FString& InTakesOriginDirectory) const;
};

class FLiveLinkFaceMetadataParser
{
public:
	static EMediaOrientation ParseOrientation(int32 InOrientation = 4);
	static bool ParseVideoMetadata(const FString& InTakeDirectory, FLiveLinkFaceVideoMetadata& OutVideoMetadata);
	static bool ParseDepthMetadata(const FString& InTakeDirectory, FLiveLinkFaceDepthMetadata& OutDepthMetadata);
	static bool ParseAudioMetadata(const FString& InTakeDirectory, FLiveLinkFaceAudioMetadata& OutAudioMetadata);
	static bool ParseTakeInfo(const FString& InTakeDirectory, FLiveLinkFaceTakeInfo& OutTakeInfo);
	static bool ParseThumbnail(const FString& InTakeDirectory, FLiveLinkFaceTakeInfo& OutTakeInfo);

private:
	static TValueOrError<FLiveLinkFaceTakeMetadata, FMetaHumanCaptureError> ParseTakeMetadata(const TSharedPtr<FJsonObject> JsonObject);
	static TOptional<FMetaHumanCaptureError> ParseString(const TSharedPtr<FJsonObject> JsonObject, const FString& Key, FString &OutString);
	static TOptional<FMetaHumanCaptureError> ParseNumber(const TSharedPtr<FJsonObject> JsonObject, const FString& Key, int32 &OutNumber);
	static TOptional<FMetaHumanCaptureError> ParseBool(const TSharedPtr<FJsonObject> JsonObject, const FString& Key, bool &OutBool);
	static FMetaHumanCaptureError CreateErrorForMissingJsonKey(const FString& Key);
	static TArray<TSharedPtr<class FJsonValue>> ParseJsonArrayFromFile(const FString& InFilePath);
	static TSharedPtr<class FJsonObject> ParseJsonObjectFromFile(const FString& InFilePath);
};