// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceMetadata.h"

#include "MetaHumanCaptureSourceLog.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "FootageIngest"

TArray<FString> FLiveLinkFaceTakeMetadata::GetCommonFileNames() const
{
	return {
		FLiveLinkFaceStaticFileNames::AudioMetadata,
		FLiveLinkFaceStaticFileNames::FrameLog,
		FLiveLinkFaceStaticFileNames::TakeMetadata,
		FLiveLinkFaceStaticFileNames::Thumbnail,
		FLiveLinkFaceStaticFileNames::VideoMetadata,
		MOVFileName()
	};
}

TArray<FString> FLiveLinkFaceTakeMetadata::GetMHAFileNames() const
{
	TArray FileNames = GetCommonFileNames();
	FileNames.Append({
		FLiveLinkFaceStaticFileNames::DepthData,
		FLiveLinkFaceStaticFileNames::DepthMetadata
	});
	return FileNames;
}

TArray<FString> FLiveLinkFaceTakeMetadata::GetARKitFileNames() const
{
	TArray FileNames = GetCommonFileNames();
	if (bIsCalibrated)
	{
		FileNames.Append(GetCalibratedBlendshapeFileNames());
	}
	else
	{
		const FString BlendshapeFileName = FString::Format(TEXT("{0}.csv"), { CommonFileNamePrefix() });
		FileNames.Add(BlendshapeFileName);
	}
	return FileNames;
}

FString FLiveLinkFaceTakeMetadata::CommonFileNamePrefix() const
{
	return FString::Format(TEXT("{0}_{1}_{2}"), { SlateName, TakeNumber, Subject });
}

TArray<FString> FLiveLinkFaceTakeMetadata::GetCalibratedBlendshapeFileNames() const
{
	FString Prefix = CommonFileNamePrefix();
	return {
		FString::Format(TEXT("{0}_cal.csv"), { Prefix }),
		FString::Format(TEXT("{0}_neutral.csv"), { Prefix }),
		FString::Format(TEXT("{0}_raw.csv"), { Prefix })
	};
}

FString FLiveLinkFaceTakeMetadata::MOVFileName() const
{
	return FString::Format(TEXT("{0}.mov"), { CommonFileNamePrefix() });
}

FMetaHumanTakeInfo FLiveLinkFaceTakeInfo::ConvertToMetaHumanTakeInfo(const FString& InTakesOriginDirectory) const
{
	FMetaHumanTakeInfo TakeInfo;
	TakeInfo.Name = GetTakeName();
	TakeInfo.Id = Id;
	TakeInfo.NumFrames = TakeMetadata.NumFrames;
	TakeInfo.FrameRate = VideoMetadata.FrameRate;
	TakeInfo.TakeNumber = TakeMetadata.TakeNumber;
	TakeInfo.Resolution = VideoMetadata.Resolution;
	TakeInfo.DepthResolution = DepthMetadata.Resolution;
	TakeInfo.Date = TakeMetadata.Date;
	TakeInfo.NumStreams = 1;
	TakeInfo.DeviceModel = TakeMetadata.DeviceModel;
	TakeInfo.RawThumbnailData = RawThumbnailData;
	TakeInfo.OutputDirectory = GetOutputDirectory(InTakesOriginDirectory);
	TakeInfo.Issues = Issues;
	return TakeInfo;
}

FString FLiveLinkFaceTakeInfo::GetTakeName() const
{	
	return FString::Format(TEXT("{0}_{1}"), { TakeMetadata.SlateName, TakeMetadata.TakeNumber });
}

FString FLiveLinkFaceTakeInfo::GetTakePath() const
{
	return FString::Format(TEXT("{0}_{1}_{2}"), { TakeMetadata.SlateName, TakeMetadata.TakeNumber, TakeMetadata.Subject });
}

FString FLiveLinkFaceTakeInfo::GetMOVFilePath() const
{
	return FString::Format(TEXT("{0}/{1}"), { TakeOriginDirectory, TakeMetadata.MOVFileName() });
}

FString FLiveLinkFaceTakeInfo::GetDepthFilePath() const
{
	return FString::Format(TEXT("{0}/{1}"), { TakeOriginDirectory, FLiveLinkFaceStaticFileNames::DepthData });
}

FString FLiveLinkFaceTakeInfo::GetFrameLogFilePath() const
{
	return FString::Format(TEXT("{0}/{1}"), { TakeOriginDirectory, FLiveLinkFaceStaticFileNames::FrameLog });
}

FString FLiveLinkFaceTakeInfo::GetCameraCalibrationFilePath() const
{
	return FString::Format(TEXT("{0}/{1}"), { TakeOriginDirectory, FLiveLinkFaceStaticFileNames::DepthMetadata });
}

FString FLiveLinkFaceTakeInfo::GetOutputDirectory(const FString& InTakesOriginDirectory) const
{
	return TakeOriginDirectory.Mid(InTakesOriginDirectory.Len());
}

float FLiveLinkFaceTakeInfo::GetTakeDurationInSeconds() const
{
	return static_cast<float>(TakeMetadata.NumFrames) / VideoMetadata.FrameRate;
}

EMediaOrientation FLiveLinkFaceMetadataParser::ParseOrientation(int32 InOrientation)
{
	// 0: Portrait, 1: Landscape, 2: Portrait, 3: Landscape
	switch (InOrientation)
	{
		case 1: // Portrait
			return EMediaOrientation::Original;
		case 2:	// PortraitUpsideDown
			return EMediaOrientation::CW180;
		case 3: // landscapeLeft
			return EMediaOrientation::CW90;
		case 4: // LandscapeRight
		default:
			return EMediaOrientation::CW270;
	}
}

bool FLiveLinkFaceMetadataParser::ParseVideoMetadata(const FString& InTakeDirectory, FLiveLinkFaceVideoMetadata& OutVideoMetadata)
{
	const FString& VideoMetadataFile = InTakeDirectory / FLiveLinkFaceStaticFileNames::VideoMetadata;

	if (TSharedPtr<FJsonObject> VideoMetadataJson = ParseJsonObjectFromFile(VideoMetadataFile))
	{
		// Sample video_metadata.json
		// {
		// 	"FrameRate" : 60,
		// 	"Dimensions" : {
		// 		"width" : 1280,
		// 		"height" : 720
		// 	},
		// 	"Quality" : "high",
		// 	"JpegCompressionQuality" : 0.90000000000000002
		// }

		bool success = true;
		success &= VideoMetadataJson->TryGetNumberField(TEXT("FrameRate"), OutVideoMetadata.FrameRate);

		const TSharedPtr<FJsonObject>* DimensionsJson;
		success &= VideoMetadataJson->TryGetObjectField(TEXT("Dimensions"), DimensionsJson);
		if (success)
		{
			success &= DimensionsJson->ToSharedRef()->TryGetNumberField(TEXT("width"), OutVideoMetadata.Resolution.X);
			success &= DimensionsJson->ToSharedRef()->TryGetNumberField(TEXT("height"), OutVideoMetadata.Resolution.Y);
		}

		success &= VideoMetadataJson->TryGetStringField(TEXT("Quality"), OutVideoMetadata.Quality);
		success &= VideoMetadataJson->TryGetNumberField(TEXT("JpegCompressionQuality"), OutVideoMetadata.JpegCompressionQuality);

		int32 Orientation = 4; // Default value in case the field does not exist
		bool bHasOrientation = VideoMetadataJson->HasField(TEXT("Orientation"));
		if (bHasOrientation)
		{
			success &= VideoMetadataJson->TryGetNumberField(TEXT("Orientation"), Orientation); // Use TryGet as conversion might fail
		}
		OutVideoMetadata.Orientation = ParseOrientation(Orientation);

		return success;
	}

	return false;
}

bool FLiveLinkFaceMetadataParser::ParseDepthMetadata(const FString& InTakeDirectory, FLiveLinkFaceDepthMetadata& OutDepthMetadata)
{
	const FString& DepthMetadataFile = InTakeDirectory / FLiveLinkFaceStaticFileNames::DepthMetadata;

	if (TSharedPtr<FJsonObject> DepthMetadataJson = ParseJsonObjectFromFile(DepthMetadataFile))
	{
		bool success = true;
		
		success &= DepthMetadataJson->TryGetStringField(TEXT("Compression"), OutDepthMetadata.Compression);
		success &= DepthMetadataJson->TryGetNumberField(TEXT("PixelSize"), OutDepthMetadata.PixelSize);
		success &= DepthMetadataJson->TryGetNumberField(TEXT("DepthFrameRate"), OutDepthMetadata.FrameRate);

		int32 Orientation = 4; // Default value in case the field does not exist
		bool bHasOrientation = DepthMetadataJson->HasField(TEXT("Orientation"));
		if (bHasOrientation)
		{
			success &= DepthMetadataJson->TryGetNumberField(TEXT("Orientation"), Orientation); // Use TryGet as conversion might fail
		}
		OutDepthMetadata.Orientation = ParseOrientation(Orientation);

		if (OutDepthMetadata.Compression == TEXT("Oodle"))
		{
			const TSharedPtr<FJsonObject>* OodleCompression;
			success &= DepthMetadataJson->TryGetObjectField(TEXT("Oodle"), OodleCompression);
			if (success)
			{
				success &= OodleCompression->ToSharedRef()->TryGetStringField(TEXT("CompressionLevel"), OutDepthMetadata.OodleInfo.CompressionLevel);
				success &= OodleCompression->ToSharedRef()->TryGetStringField(TEXT("Compressor"), OutDepthMetadata.OodleInfo.Compressor);
				success &= OodleCompression->ToSharedRef()->TryGetStringField(TEXT("Version"), OutDepthMetadata.OodleInfo.Version);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* LensDistortionLUTArray;
		success &= DepthMetadataJson->TryGetArrayField(TEXT("LensDistortionLookupTable"), LensDistortionLUTArray);
		if (success)
		{
			for (const TSharedPtr<FJsonValue>& Value : *LensDistortionLUTArray)
			{
				OutDepthMetadata.LensDistortionLookupTable.Add(Value->AsNumber());
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* InverseLensDistortionLUTArray;
		success &= DepthMetadataJson->TryGetArrayField(TEXT("InverseLensDistortionLookupTable"), InverseLensDistortionLUTArray);
		if (success)
		{
			for (const TSharedPtr<FJsonValue>& Value : *InverseLensDistortionLUTArray)
			{
				OutDepthMetadata.InverseLensDistortionLookupTable.Add(Value->AsNumber());
			}
		}

		const TSharedPtr<FJsonObject>* IntrinsicMatrixReferenceDimensions;
		success &= DepthMetadataJson->TryGetObjectField(TEXT("IntrinsicMatrixReferenceDimensions"), IntrinsicMatrixReferenceDimensions);
		if (success)
		{
			success &= IntrinsicMatrixReferenceDimensions->ToSharedRef()->TryGetNumberField(TEXT("Width"), OutDepthMetadata.IntrinsicMatrixReferenceDimensions.X);
			success &= IntrinsicMatrixReferenceDimensions->ToSharedRef()->TryGetNumberField(TEXT("Height"), OutDepthMetadata.IntrinsicMatrixReferenceDimensions.Y);
		}

		const TSharedPtr<FJsonObject>* DepthDimensions;
		success &= DepthMetadataJson->TryGetObjectField(TEXT("DepthDimensions"), DepthDimensions);
		if (success)
		{
			success &= DepthDimensions->ToSharedRef()->TryGetNumberField(TEXT("Width"), OutDepthMetadata.Resolution.X);
			success &= DepthDimensions->ToSharedRef()->TryGetNumberField(TEXT("Height"), OutDepthMetadata.Resolution.Y);
		}

		const TSharedPtr<FJsonObject>* LensDistorionCenter;
		success &= DepthMetadataJson->TryGetObjectField(TEXT("LensDistortionCenter"), LensDistorionCenter);
		if (success)
		{
			success &= LensDistorionCenter->ToSharedRef()->TryGetNumberField(TEXT("X"), OutDepthMetadata.LensDistortionCenter.X);
			success &= LensDistorionCenter->ToSharedRef()->TryGetNumberField(TEXT("Y"), OutDepthMetadata.LensDistortionCenter.Y);
		}

		const TArray<TSharedPtr<FJsonValue>>* IntrinsicMatrix;
		success &= DepthMetadataJson->TryGetArrayField(TEXT("IntrinsicMatrix"), IntrinsicMatrix);
		if (success)
		{
			for (const TSharedPtr<FJsonValue>& Value : *IntrinsicMatrix)
			{
				OutDepthMetadata.IntrinsicMatrix.Add(Value->AsNumber());
			}
		}

		return success;
	}

	return false;
}

bool FLiveLinkFaceMetadataParser::ParseAudioMetadata(const FString& InTakeDirectory, FLiveLinkFaceAudioMetadata& OutAudioMetadata)
{
	const FString& AudioMetadataFile = InTakeDirectory / FLiveLinkFaceStaticFileNames::AudioMetadata;

	if (TSharedPtr<FJsonObject> AudioMetadataJson = ParseJsonObjectFromFile(AudioMetadataFile))
	{
		// Sample audio_metadata.json
		// {
		// 	"BitsPerChannel" : 16,
		// 	"SampleRate" : 44100,
		// 	"ChannelsPerFrame" : 1,
		// 	"FormatFlags" : 12
		// }
		bool success = true;

		success &= AudioMetadataJson->TryGetNumberField(TEXT("BitsPerChannel"), OutAudioMetadata.BitsPerChannel);
		success &= AudioMetadataJson->TryGetNumberField(TEXT("SampleRate"), OutAudioMetadata.SampleRate);
		success &= AudioMetadataJson->TryGetNumberField(TEXT("ChannelsPerFrame"), OutAudioMetadata.ChannelsPerFrame);
		success &= AudioMetadataJson->TryGetNumberField(TEXT("FormatFlags"), OutAudioMetadata.FormatFlags);

		return success;
	}

	return false;
}

bool FLiveLinkFaceMetadataParser::ParseTakeInfo(const FString& InTakeDirectory, FLiveLinkFaceTakeInfo& OutTakeInfo)
{
	const FString& TakeMetadataFile = InTakeDirectory / FLiveLinkFaceStaticFileNames::TakeMetadata;

	const TSharedPtr TakeMetadataJson = ParseJsonObjectFromFile(TakeMetadataFile);

	if (!TakeMetadataJson.IsValid())
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to parse JSON object from take metadata file"));
		return false;
	}
	
	// Sample take.json
	// {
	// 	"frames" : 1058,
	// 	"appVersion" : "v0.1.0 (build 17)",
	// 	"slate" : "HH_Neutral_Rotation",
	// 	"calibrated" : false,
	// 	"subject" : "iPhone12Pro",
	// 	"identifier" : "20211006_HH_Neutral_Rotation_1",
	// 	"date" : "2021-10-06T11:31:46Z",
	// 	"version" : 1,
	// 	"take" : 1,
	// 	"deviceModel" : "iPhone13,3"
	// }

	TValueOrError<FLiveLinkFaceTakeMetadata, FMetaHumanCaptureError> ParseResult = ParseTakeMetadata(TakeMetadataJson);

	if (ParseResult.HasError())
	{
		const FMetaHumanCaptureError& Error = ParseResult.StealError();
		UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("Failed to parse take metadata: %s"), *Error.GetMessage());
		return false;
	}

	if (!ParseResult.HasValue())
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Take metadata parse result did not contain an error but no value was provided"));
		return false;
	}

	OutTakeInfo.TakeMetadata = ParseResult.StealValue();
	OutTakeInfo.TakeOriginDirectory = InTakeDirectory;
	
	return true;
}

bool FLiveLinkFaceMetadataParser::ParseThumbnail(const FString& InTakeDirectory, FLiveLinkFaceTakeInfo& OutTakeInfo)
{
	const FString& ThumbnailFile = InTakeDirectory / FLiveLinkFaceStaticFileNames::Thumbnail;

	return FFileHelper::LoadFileToArray(OutTakeInfo.RawThumbnailData, *ThumbnailFile);
}

TValueOrError<FLiveLinkFaceTakeMetadata, FMetaHumanCaptureError> FLiveLinkFaceMetadataParser::ParseTakeMetadata(const TSharedPtr<FJsonObject> JsonObject)
{
	FLiveLinkFaceTakeMetadata TakeMetadata;

	FString DateString;
	const TArray ParseErrors = {
		ParseString(JsonObject, TEXT("slate"), TakeMetadata.SlateName),
		ParseString(JsonObject, TEXT("appVersion"), TakeMetadata.AppVersion),
		ParseString(JsonObject, TEXT("deviceModel"), TakeMetadata.DeviceModel),
		ParseString(JsonObject, TEXT("subject"), TakeMetadata.Subject),
		ParseString(JsonObject, TEXT("identifier"), TakeMetadata.Identifier),
		ParseString(JsonObject, TEXT("date"), DateString),
		ParseNumber(JsonObject, TEXT("version"), TakeMetadata.Version),
		ParseNumber(JsonObject, TEXT("take"), TakeMetadata.TakeNumber),
		ParseNumber(JsonObject, TEXT("frames"), TakeMetadata.NumFrames),
		ParseBool(JsonObject, TEXT("calibrated"), TakeMetadata.bIsCalibrated)
	};

	TArray Errors = ParseErrors.FilterByPredicate([](const TOptional<FMetaHumanCaptureError>& Error) { return Error.IsSet(); });

	if (!Errors.IsEmpty())
	{			
		return MakeError(Errors[0].GetValue());
	}
	
	if (!FDateTime::ParseIso8601(*DateString, TakeMetadata.Date))
	{
		return MakeError(FMetaHumanCaptureError(InternalError, FString::Format(TEXT("Failed to parse DateString '{0}' as an Iso8601 date"), { *DateString })));
	}

	return MakeValue(TakeMetadata);
}

TOptional<FMetaHumanCaptureError> FLiveLinkFaceMetadataParser::ParseString(const TSharedPtr<FJsonObject> JsonObject, const FString& Key, FString &OutString)
{
	if (!JsonObject->TryGetStringField(Key, OutString))
	{
		return { CreateErrorForMissingJsonKey(Key) };
	}
	return {};
}

TOptional<FMetaHumanCaptureError> FLiveLinkFaceMetadataParser::ParseNumber(const TSharedPtr<FJsonObject> JsonObject, const FString& Key, int32 &OutNumber)
{
	if (!JsonObject->TryGetNumberField(Key, OutNumber))
	{
		return { CreateErrorForMissingJsonKey(Key) };
	}
	return {};
}

TOptional<FMetaHumanCaptureError> FLiveLinkFaceMetadataParser::ParseBool(const TSharedPtr<FJsonObject> JsonObject, const FString& Key, bool &OutBool)
{
	if (!JsonObject->TryGetBoolField(Key, OutBool))
	{
		return { CreateErrorForMissingJsonKey(Key) };
	}
	return {};
}

FMetaHumanCaptureError FLiveLinkFaceMetadataParser::CreateErrorForMissingJsonKey(const FString& Key)
{
	return FMetaHumanCaptureError(NotFound, FString::Format(TEXT("Missing value for key '{0}' in Live Link Face Metadata"), {Key}));
}

TArray<TSharedPtr<FJsonValue>> FLiveLinkFaceMetadataParser::ParseJsonArrayFromFile(const FString& InFilePath)
{
	FString JsonStringBuffer;
	TArray<TSharedPtr<FJsonValue>> Result;

	if (FFileHelper::LoadFileToString(JsonStringBuffer, *InFilePath) &&
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonStringBuffer), Result))
	{
		return Result;
	}

	return TArray<TSharedPtr<FJsonValue>>();
}

TSharedPtr<FJsonObject> FLiveLinkFaceMetadataParser::ParseJsonObjectFromFile(const FString& InFilePath)
{
	FString JsonStringBuffer;
	TSharedPtr<FJsonObject> Result;

	if (FFileHelper::LoadFileToString(JsonStringBuffer, *InFilePath) &&
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonStringBuffer), Result))
	{
		return Result;
	}

	return TSharedPtr<FJsonObject>();
}

#undef LOCTEXT_NAMESPACE
