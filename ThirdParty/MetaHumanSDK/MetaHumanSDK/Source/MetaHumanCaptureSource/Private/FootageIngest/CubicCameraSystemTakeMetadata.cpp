// Copyright Epic Games, Inc. All Rights Reserved.

#include "CubicCameraSystemTakeMetadata.h"

#include "MetaHumanCaptureSourceLog.h"
#include "ResolutionResolver.h"

#include "Utils/MetaHumanStringUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "CubicCameraSystemTakeMetadata"

#define CHECK_RESULT_AND_RETURN(Fun) if (bool Result = Fun; !Result) {return TOptional<FCubicTakeInfo>{};}

static const FText FailedToDetermineCameraResolution = LOCTEXT("FailedToDetermineCameraResolution", "Failed to determine camera resolution");
static const FText NoImagesFound = LOCTEXT("NoImagesFound", "No images found");
static const FText ImageLoadFailed = LOCTEXT("ImageLoadFailed", "Failed to load an image");
static const FText CameraResolutionMismatch = LOCTEXT("CameraResolutionMismatch", "Camera resolutions do not match");

static FString ConvertPathToAbsolute(const FString& InBasePath, const FString& InPath)
{
	// Convert paths to absolute
	if (FPaths::IsRelative(InPath))
	{
		const FString Directory = FPaths::GetPath(InBasePath);
		return FPaths::Combine(Directory, InPath);
	}

	return InPath;
}

static FString WrapLogMessage(const FString InMessage, const FCubicTakeInfo& InTakeInfo)
{
	return FString::Format(TEXT("{0} ({1} #{2})"), { InMessage, InTakeInfo.Slate, InTakeInfo.Take });
}

static void ReportIssue(FText InMessage, const FCubicTakeInfo& InTakeInfo, FMetaHumanTakeInfo& OutTakeInfo)
{
	// Log and register as an issue, it's important to do both so the user has a reference log that can be sent, rather than just screenshots.
	UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("%s"), *WrapLogMessage(InMessage.ToString(), InTakeInfo));
	OutTakeInfo.Issues.Emplace(MoveTemp(InMessage));
}

static void ReportResolutionAddError(
	const UE::MetaHuman::FResolutionResolver::EAddError AddError,
	const FString& CameraId,
	const FCubicTakeInfo& InCubicTakeInfo,
	FMetaHumanTakeInfo& OutTakeInfo
)
{
	using namespace UE::MetaHuman;

	FText Message;

	switch (AddError)
	{
	case FResolutionResolver::EAddError::FramesPathDoesNotExist:
	{
		// Manually format the message in this case, as we need a little more control
		const FCubicTakeInfo::FCamera *Camera = InCubicTakeInfo.CameraMap.Find(CameraId);

		if (Camera)
		{
			// We can provide more information in this case and we want to put the "for camera" part before the path, which can be very long
			ReportIssue(
				FText::Format(
					LOCTEXT("FramesPathDoesNotExistForCamera", "Frames path does not exist for camera: {0} ({1})"),
					FText::FromString(CameraId), 
					FText::FromString(Camera->FramesPath)
				),
				InCubicTakeInfo,
				OutTakeInfo
			);
		}
		else
		{
			ReportIssue(
				FText::Format(
					LOCTEXT("FramesPathDoesNotExist", "Frames path does not exist: {0}"),
					FText::FromString(CameraId)
				),
				InCubicTakeInfo,
				OutTakeInfo
			);
		}
		break;
	}

	case FResolutionResolver::EAddError::NoImagesFound:
		Message = NoImagesFound;
		break;

	case FResolutionResolver::EAddError::ImageLoadFailed:
		Message = ImageLoadFailed;
		break;

	case FResolutionResolver::EAddError::InvalidImageWrapper:
		// Log detailed message but don't display that to the user (they don't know what the image wrapper is)
		UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("%s"),
			*WrapLogMessage(FString::Format(TEXT("Image wrapper is in an invalid state for camera: {0}"), { CameraId, }), InCubicTakeInfo)
		);
		Message = FailedToDetermineCameraResolution;
		break;

	default:
		Message = FailedToDetermineCameraResolution;
		break;
	}

	if (!Message.IsEmpty())
	{
		FText IssueMessage = FText::Format(LOCTEXT("ResolutionAddError", "{0} for camera: {1}"), Message, FText::FromString(CameraId));
		ReportIssue(MoveTemp(IssueMessage), InCubicTakeInfo, OutTakeInfo);
	}
}

static void ReportResolutionResolveError(
	const UE::MetaHuman::FResolutionResolver::EResolveError ResolutionError,
	const FCubicTakeInfo& InCubicTakeInfo,
	FMetaHumanTakeInfo& OutTakeInfo
)
{
	using namespace UE::MetaHuman;

	FText Message;

	switch (ResolutionError)
	{
	case FResolutionResolver::EResolveError::Mismatched:
		Message = CameraResolutionMismatch;
		break;
	default:
		Message = FailedToDetermineCameraResolution;
		break;
	}

	ReportIssue(MoveTemp(Message), InCubicTakeInfo, OutTakeInfo);
}

static void CheckCameraIds(const FCubicTakeInfo& InCubicTakeInfo, const TMap<FString, FCubicCameraInfo>& OutTakeCameras, FMetaHumanTakeInfo& OutTakeInfo)
{
	for (const TPair<FString, FCubicTakeInfo::FCamera>& ExpectedCamera : InCubicTakeInfo.CameraMap)
	{
		const FString& ExpectedId = ExpectedCamera.Key;

		if (!OutTakeCameras.Contains(ExpectedId))
		{
			ReportIssue(
				FText::Format(
					LOCTEXT("CameraUserIdMismatch", "Camera ID present in take metadata but not present in the calibration: {0}"),
					FText::FromString(ExpectedId)
				),
				InCubicTakeInfo,
				OutTakeInfo
			);
		}
	}
}

static void CheckAudio(const FCubicTakeInfo& InCubicTakeInfo, FMetaHumanTakeInfo& OutTakeInfo)
{
	for (const FCubicTakeInfo::FAudio& Audio : InCubicTakeInfo.AudioArray)
	{
		if (!FPaths::FileExists(Audio.StreamPath))
		{
			ReportIssue(
				FText::Format(LOCTEXT("MissingAudio", "Audio file not found: {0}"), FText::FromString(Audio.StreamPath)),
				InCubicTakeInfo,
				OutTakeInfo
			);
		}
	}
}

static void LoadThumbnail(const FCubicTakeInfo& InCubicTakeInfo, FMetaHumanTakeInfo& OutTakeInfo)
{
	// If the thumbnail is missing or fails to load, this is not considered an "issue" (which will block ingest), so just log any problems instead.

	if (FPaths::FileExists(InCubicTakeInfo.ThumbnailPath))
	{
		if (!FFileHelper::LoadFileToArray(OutTakeInfo.RawThumbnailData, *InCubicTakeInfo.ThumbnailPath))
		{
			UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("Failed to load thumbnail: %s"), *InCubicTakeInfo.ThumbnailPath);
		}
	}
	else
	{
		UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("Thumbnail not found: %s"), *InCubicTakeInfo.ThumbnailPath);
	}
}

static void LoadCalibration(const FCubicTakeInfo& InCubicTakeInfo, const FStopToken& InStopToken, TMap<FString, FCubicCameraInfo>& OutTakeCameras, FMetaHumanTakeInfo& OutTakeInfo)
{
	if (FPaths::FileExists(InCubicTakeInfo.CalibrationFilePath))
	{
		if (!FCubicCameraSystemTakeParser::ParseCalibrationFile(InCubicTakeInfo.CalibrationFilePath, InStopToken, OutTakeCameras))
		{
			ReportIssue(
				FText::Format(
					LOCTEXT("CalibrationLoadFailed", "Failed to load calibration: {0}"),
					FText::FromString(InCubicTakeInfo.CalibrationFilePath)
				),
				InCubicTakeInfo,
				OutTakeInfo
			);
		}
	}
	else
	{
		ReportIssue(
			FText::Format(LOCTEXT("CalibrationNotFound", "Calibration file not found: {0}"), FText::FromString(InCubicTakeInfo.CalibrationFilePath)),
			InCubicTakeInfo,
			OutTakeInfo
		);
	}
}

static void CheckForEmptyFramesPaths(const FCubicTakeInfo& InCubicTakeInfo, FMetaHumanTakeInfo& OutTakeInfo)
{
	// Work-around for lack of enforcement in take parsing code (which allows this to occur)

	for (const TPair<FString, FCubicTakeInfo::FCamera>& Camera : InCubicTakeInfo.CameraMap)
	{
		if (Camera.Value.FramesPath.IsEmpty())
		{
			ReportIssue(
				FText::Format(LOCTEXT("EmptyFramesPath", "Empty frames path for camera: {0}"), FText::FromString(Camera.Value.UserId)),
				InCubicTakeInfo,
				OutTakeInfo
			);
		}
	}
}

static void CheckForNonANSICharacters(const FCubicTakeInfo& InCubicTakeInfo, FMetaHumanTakeInfo& OutTakeInfo)
{
	TArray<FString> NonANSIContainingProperties;

	if (!FCString::IsPureAnsi(*InCubicTakeInfo.Id))
	{
		NonANSIContainingProperties.Add(TEXT("Id"));
	}
	
	if (!FCString::IsPureAnsi(*InCubicTakeInfo.Slate))
	{
		NonANSIContainingProperties.Add(TEXT("Slate"));
	}
	
	if (!FCString::IsPureAnsi(*InCubicTakeInfo.ThumbnailPath))
	{
		NonANSIContainingProperties.Add(TEXT("ThumbnailPath"));
	}
	
	if (!FCString::IsPureAnsi(*InCubicTakeInfo.CalibrationFilePath))
	{
		NonANSIContainingProperties.Add(TEXT("CalibrationFilePath"));
	}

	for (const TPair<FString, FCubicTakeInfo::FCamera>& Camera : InCubicTakeInfo.CameraMap)
	{
		const FString& CameraUserId = Camera.Value.UserId;
		FString CameraPropertyPrefix = FString::Format(TEXT("Camera({0})."), { CameraUserId });
		if (!FCString::IsPureAnsi(*CameraUserId))
		{
			NonANSIContainingProperties.Add(CameraPropertyPrefix + TEXT("UserId"));
		}
	
		if (!FCString::IsPureAnsi(*Camera.Value.FramesPath))
		{
			NonANSIContainingProperties.Add(CameraPropertyPrefix + TEXT("FramesPath"));
		}
	}
	
	for (const FCubicTakeInfo::FAudio& Audio : InCubicTakeInfo.AudioArray)
	{
		const FString& AudioUserId = Audio.UserId;
		FString AudioPropertyPrefix = FString::Format(TEXT("Audio({0})."), { AudioUserId });
		if (!FCString::IsPureAnsi(*AudioUserId))
		{
			NonANSIContainingProperties.Add(AudioPropertyPrefix + TEXT("UserId"));
		}
	
		if (!FCString::IsPureAnsi(*Audio.StreamPath))
		{
			NonANSIContainingProperties.Add(AudioPropertyPrefix + TEXT("StreamPath"));
		}
	}

	for (const FString& NonANSIContainingProperty : NonANSIContainingProperties)
	{
		const FText Message = FText::Format(LOCTEXT("UnsupportedCharactersWithinTakeInfo", "Take '{0}' contains unsupported text characters"), FText::FromString(NonANSIContainingProperty));
		OutTakeInfo.Issues.Add(Message);
		UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("InCubicTakeInfo property '%s' contains unsupported non-ansi text characters."), *NonANSIContainingProperty);
	}
}

FString FCubicTakeInfo::GetName() const
{
	return FString::Format(TEXT("{0}_{1}"), { Slate, Take });
}

bool FCubicCameraSystemTakeParser::ParseCalibrationFile(const FString& InFileName, const FStopToken& InStopToken, TMap<FString, FCubicCameraInfo>& OutCameras)
{
	TArray<TSharedPtr<FJsonValue>> CalibrationJson = ParseJsonArrayFromFile(InFileName);

	if (CalibrationJson.IsEmpty())
	{
		UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("Calibration file is empty: %s"), *InFileName);
		return false;
	}

	for (TSharedPtr<FJsonValue> Value : CalibrationJson)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (Value->TryGetObject(Object))
		{
			const TSharedPtr<FJsonObject>* MetaData = nullptr;
			if (Object->ToSharedRef()->TryGetObjectField(TEXT("metadata"), MetaData))
			{
				FCubicCameraInfo CameraInfo;
				MetaData->ToSharedRef()->TryGetStringField(TEXT("camera"), CameraInfo.Name);

				if (!CameraInfo.Name.IsEmpty())
				{
					bool bOK = true;
					CameraInfo.Calibration.CameraId = CameraInfo.Name;
					bOK &= Object->ToSharedRef()->TryGetNumberField(TEXT("image_size_x"), CameraInfo.Calibration.ImageSize.X);
					bOK &= Object->ToSharedRef()->TryGetNumberField(TEXT("image_size_y"), CameraInfo.Calibration.ImageSize.Y);
					bOK &= Object->ToSharedRef()->TryGetNumberField(TEXT("fx"), CameraInfo.Calibration.FocalLength.X);
					bOK &= Object->ToSharedRef()->TryGetNumberField(TEXT("fy"), CameraInfo.Calibration.FocalLength.Y);
					bOK &= Object->ToSharedRef()->TryGetNumberField(TEXT("cx"), CameraInfo.Calibration.PrincipalPoint.X);
					bOK &= Object->ToSharedRef()->TryGetNumberField(TEXT("cy"), CameraInfo.Calibration.PrincipalPoint.Y);
					bOK &= Object->ToSharedRef()->TryGetNumberField(TEXT("k1"), CameraInfo.Calibration.K1);
					bOK &= Object->ToSharedRef()->TryGetNumberField(TEXT("k2"), CameraInfo.Calibration.K2);
					bOK &= Object->ToSharedRef()->TryGetNumberField(TEXT("k3"), CameraInfo.Calibration.K3);
					bOK &= Object->ToSharedRef()->TryGetNumberField(TEXT("p1"), CameraInfo.Calibration.P1);
					bOK &= Object->ToSharedRef()->TryGetNumberField(TEXT("p2"), CameraInfo.Calibration.P2);

					CameraInfo.Calibration.CameraType = FCameraCalibration::Video;

					const TArray<TSharedPtr<FJsonValue>>* Transform = nullptr;
					bOK &= Object->ToSharedRef()->TryGetArrayField(TEXT("transform"), Transform);
					bOK &= Transform && Transform->Num() == 16;

					if (bOK)
					{
						for (int32 I = 0; I < 4; ++I)
						{
							for (int32 J = 0; J < 4; ++J)
							{
								CameraInfo.Calibration.Transform.M[J][I] = (*Transform)[I * 4 + J]->AsNumber();
							}
						}

						FString Name = CameraInfo.Name;
						OutCameras.Add(MoveTemp(Name), MoveTemp(CameraInfo));
					}
				}
			}
		}
	}

	return true;
}

TOptional<FCubicTakeInfo> FCubicCameraSystemTakeParser::ParseTakeMetadataFile(const FString& InFileName, const FStopToken& InStopToken)
{
	FCubicTakeInfo CubicTakeInfo;
	const FString CurrentFileName = FPaths::GetCleanFilename(InFileName);

	if (CurrentFileName == TEXT("take.json"))
	{
		if (TSharedPtr<FJsonObject> TakeMetadataJson = ParseJsonObjectFromFile(InFileName))
		{
			CHECK_RESULT_AND_RETURN(TakeMetadataJson->TryGetNumberField(TEXT("Version"), CubicTakeInfo.Version));
			CHECK_RESULT_AND_RETURN(TakeMetadataJson->TryGetStringField(TEXT("Id"), CubicTakeInfo.Id));
			CHECK_RESULT_AND_RETURN(TakeMetadataJson->TryGetNumberField(TEXT("Take"), CubicTakeInfo.Take));
			CHECK_RESULT_AND_RETURN(TakeMetadataJson->TryGetStringField(TEXT("Slate"), CubicTakeInfo.Slate));
			// Optional
			FString Thumbnail;
			if (TakeMetadataJson->TryGetStringField(TEXT("Thumbnail"), Thumbnail))
			{
				CubicTakeInfo.ThumbnailPath = ConvertPathToAbsolute(InFileName, Thumbnail);
			}

			FString Date;
			CHECK_RESULT_AND_RETURN(TakeMetadataJson->TryGetStringField(TEXT("LocalDateTime"), Date));

			if (!FDateTime::ParseIso8601(*Date, CubicTakeInfo.Date))
			{
				UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("Failed to parse date string '%s' as an ISO8601 date"), *Date);
			}

			FString CalibrationFilePath;
			CHECK_RESULT_AND_RETURN(TakeMetadataJson->TryGetStringField(TEXT("CalibrationInfo"), CalibrationFilePath));
			CubicTakeInfo.CalibrationFilePath = ConvertPathToAbsolute(InFileName, CalibrationFilePath);

			const TSharedPtr<FJsonObject>* DeviceInfoObject;
			CHECK_RESULT_AND_RETURN(TakeMetadataJson->TryGetObjectField(TEXT("DeviceInfo"), DeviceInfoObject));

			CHECK_RESULT_AND_RETURN((*DeviceInfoObject)->TryGetStringField(TEXT("Model"), CubicTakeInfo.DeviceInfo.Model));
			CHECK_RESULT_AND_RETURN((*DeviceInfoObject)->TryGetStringField(TEXT("Type"), CubicTakeInfo.DeviceInfo.Type));
			CHECK_RESULT_AND_RETURN((*DeviceInfoObject)->TryGetStringField(TEXT("Id"), CubicTakeInfo.DeviceInfo.Id));

			const TArray<TSharedPtr<FJsonValue>>* CamerasJson;
			CHECK_RESULT_AND_RETURN(TakeMetadataJson->TryGetArrayField(TEXT("Cameras"), CamerasJson));

			// Cameras
			for (const TSharedPtr<FJsonValue>& CameraJson : *CamerasJson)
			{
				FCubicTakeInfo::FCamera Camera;
				const TSharedPtr<FJsonObject>& CameraObject = CameraJson->AsObject();

				CHECK_RESULT_AND_RETURN(CameraObject->TryGetStringField(TEXT("UserID"), Camera.UserId));

				// Optional
				const TArray<TSharedPtr<FJsonValue>>* FrameRangeJson = nullptr;
				if (CameraObject->TryGetArrayField(TEXT("FrameRange"), FrameRangeJson))
				{
					CHECK_RESULT_AND_RETURN(((*FrameRangeJson).Num() == 2));
					Camera.FrameRange.Key = (*FrameRangeJson)[0]->AsNumber();
					Camera.FrameRange.Value = (*FrameRangeJson)[1]->AsNumber();
				}

				CHECK_RESULT_AND_RETURN(CameraObject->TryGetNumberField(TEXT("FrameRate"), Camera.FrameRate));

				FString FramesPath;
				CHECK_RESULT_AND_RETURN(CameraObject->TryGetStringField(TEXT("FramesPath"), FramesPath));

				if (!FramesPath.IsEmpty())
				{
					Camera.FramesPath = ConvertPathToAbsolute(InFileName, FramesPath);
				}

				// Optional
				CameraObject->TryGetStringField(TEXT("StartTimecode"), Camera.StartTimecode);

				// Optional
				const TArray<TSharedPtr<FJsonValue>>* FramesDroppedJson;
				if (CameraObject->TryGetArrayField(TEXT("FramesDropped"), FramesDroppedJson))
				{
					for (const TSharedPtr<FJsonValue>& DroppedFrameJson : *FramesDroppedJson)
					{
						FString DroppedFrame;
						CHECK_RESULT_AND_RETURN(DroppedFrameJson->TryGetString(DroppedFrame));

						TArray<FString> FrameTokens;
						DroppedFrame.ParseIntoArray(FrameTokens, TEXT("-"));
						CHECK_RESULT_AND_RETURN(FrameTokens.Num() > 0 && FrameTokens.Num() <= 2);

						FFrameRange FrameRange;
						FrameRange.StartFrame = FCString::Atoi(*FrameTokens[0]);
						FrameRange.EndFrame = FrameTokens.Num() == 2 ? FCString::Atoi(*FrameTokens[1]) : FrameRange.StartFrame;

						CHECK_RESULT_AND_RETURN(FrameRange.StartFrame >= 0);
						CHECK_RESULT_AND_RETURN(FrameRange.EndFrame >= 0);
						CHECK_RESULT_AND_RETURN(FrameRange.EndFrame >= FrameRange.StartFrame);

						if (FrameRangeJson)
						{
							// Dropped frames are specified wrt frame range start
							FrameRange.StartFrame -= Camera.FrameRange.Key;
							FrameRange.EndFrame -= Camera.FrameRange.Key;
						}

						if (FrameRange.StartFrame >= 0)
						{
							Camera.CaptureExcludedFrames.Add(FrameRange);
						}
					}
				}

				FString Name = Camera.UserId;
				CubicTakeInfo.CameraMap.Add(MoveTemp(Name), MoveTemp(Camera));
			}

			const TArray<TSharedPtr<FJsonValue>>* AudioArrayJson;
			// Optional
			bool HasAudio = TakeMetadataJson->TryGetArrayField(TEXT("Audio"), AudioArrayJson);
			if (HasAudio)
			{
				// Audio
				for (const TSharedPtr<FJsonValue>& AudioJson : *AudioArrayJson)
				{
					FCubicTakeInfo::FAudio Audio;
					const TSharedPtr<FJsonObject>& AudioObject = AudioJson->AsObject();

					CHECK_RESULT_AND_RETURN(AudioObject->TryGetStringField(TEXT("UserID"), Audio.UserId));

					FString StreamPath;
					CHECK_RESULT_AND_RETURN(AudioObject->TryGetStringField(TEXT("StreamPath"), StreamPath));
					Audio.StreamPath = ConvertPathToAbsolute(InFileName, StreamPath);

					// Optional
					AudioObject->TryGetNumberField(TEXT("TimecodeRate"), Audio.TimecodeRate);
					AudioObject->TryGetStringField(TEXT("StartTimecode"), Audio.StartTimecode);

					CubicTakeInfo.AudioArray.Add(MoveTemp(Audio));
				}
			}

			CubicTakeInfo.TakeJsonFilePath = InFileName;
			return MoveTemp(CubicTakeInfo);
		}
	}

	return {};
}

void FCubicCameraSystemTakeParser::CubicToMetaHumanTakeInfo(
	const FString& InFilePath,
	const FString InOutputDirectory,
	const FCubicTakeInfo& InCubicTakeInfo,
	const FStopToken& InStopToken,
	const TakeId InNewTakeId,
	const int32 InExpectedCameraCount,
	const FString& InDeviceType,
	FMetaHumanTakeInfo& OutTakeInfo,
	TMap<FString, FCubicCameraInfo>& OutTakeCameras
)
{
	OutTakeInfo.Id = InNewTakeId;
	OutTakeInfo.Name = InCubicTakeInfo.GetName();
	OutTakeInfo.NumFrames = MAX_int32;
	OutTakeInfo.TakeNumber = InCubicTakeInfo.Take;
	OutTakeInfo.DepthResolution = FIntPoint(0, 0); // Unknown, can only calculate by generating the depth itself
	OutTakeInfo.Date = InCubicTakeInfo.Date;
	OutTakeInfo.NumStreams = InCubicTakeInfo.CameraMap.Num() / 2;
	OutTakeInfo.DeviceModel = InCubicTakeInfo.DeviceInfo.Model;
	OutTakeInfo.OutputDirectory = InOutputDirectory;

	if (!FCString::IsPureAnsi(*InFilePath))
	{
		ReportIssue(
			LOCTEXT("UnsupportedCharactersWithinFilePath", "Take File Path contains unsupported text characters"), 
			InCubicTakeInfo, 
			OutTakeInfo
		);
	}

	if (MetaHumanStringContainsWhitespace(OutTakeInfo.OutputDirectory))
	{
		ReportIssue(LOCTEXT("TakeFolderContainsWhiteSpace", "Take Folder contains whitespace"), InCubicTakeInfo, OutTakeInfo);
	}

	if (MetaHumanStringContainsWhitespace(OutTakeInfo.Name))
	{
		ReportIssue(LOCTEXT("TakeNameContainsWhiteSpace", "Take name contains whitespace"), InCubicTakeInfo, OutTakeInfo);
	}

	if (InCubicTakeInfo.DeviceInfo.Type != InDeviceType)
	{
		ReportIssue(
			FText::Format(
				LOCTEXT("UnexpectedDeviceType", "Unexpected device type: {0} instead of {1}"),
				FText::FromString(InCubicTakeInfo.DeviceInfo.Type),
				FText::FromString(InDeviceType)
			),
			InCubicTakeInfo,
			OutTakeInfo
		);
	}

	if (InCubicTakeInfo.CameraMap.Num() != InExpectedCameraCount)
	{
		ReportIssue(
			FText::Format(
				LOCTEXT("UnexpectedNumberOfCameras", "Unexpected number of cameras: expected {0}, found {1}"),
				InExpectedCameraCount,
				InCubicTakeInfo.CameraMap.Num()
			),
			InCubicTakeInfo,
			OutTakeInfo
		);
	}

	CheckForNonANSICharacters(InCubicTakeInfo, OutTakeInfo);

	CheckAudio(InCubicTakeInfo, OutTakeInfo);
	CheckForEmptyFramesPaths(InCubicTakeInfo, OutTakeInfo);

	// Updates the output camera map
	LoadCalibration(InCubicTakeInfo, InStopToken, OutTakeCameras, OutTakeInfo);

	// If the camera map is empty then the calibration load failed and we do not want to display any camera ID mismatch warnings
	if (!OutTakeCameras.IsEmpty())
	{
		CheckCameraIds(InCubicTakeInfo, OutTakeCameras, OutTakeInfo);
	}

	LoadCameras(InCubicTakeInfo, OutTakeInfo);
	LoadThumbnail(InCubicTakeInfo, OutTakeInfo);
}

void FCubicCameraSystemTakeParser::LoadCameras(const FCubicTakeInfo& InCubicTakeInfo, FMetaHumanTakeInfo& OutTakeInfo)
{
	using namespace UE::MetaHuman;

	FResolutionResolver ResolutionResolver;

	for (const TPair<FString, FCubicTakeInfo::FCamera>& CameraIt : InCubicTakeInfo.CameraMap)
	{
		const FCubicTakeInfo::FCamera& Camera = CameraIt.Value;

		// Doesn't take into account dropped frames:
		const int32 CurrentCameraFrames = (Camera.FrameRange.Value - Camera.FrameRange.Key) + 1;

		if (OutTakeInfo.NumFrames > CurrentCameraFrames)
		{
			OutTakeInfo.NumFrames = CurrentCameraFrames;
		}

		const TValueOrError<FIntPoint, FResolutionResolver::EAddError> CameraResolution = ResolutionResolver.Add(Camera);

		if (CameraResolution.HasError())
		{
			ReportResolutionAddError(CameraResolution.GetError(), Camera.UserId, InCubicTakeInfo, OutTakeInfo);
		} 

		if (OutTakeInfo.FrameRate != Camera.FrameRate)
		{
			OutTakeInfo.FrameRate = Camera.FrameRate;
		}
	}

	TValueOrError<FIntPoint, FResolutionResolver::EResolveError> Resolution = ResolutionResolver.Resolve();

	if (Resolution.HasValue())
	{
		OutTakeInfo.Resolution = Resolution.GetValue();
	}
	else
	{
		ReportResolutionResolveError(Resolution.GetError(), InCubicTakeInfo, OutTakeInfo);
		OutTakeInfo.Resolution = FIntPoint::NoneValue;
	}

	if (OutTakeInfo.NumFrames == MAX_uint32)
	{
		OutTakeInfo.NumFrames = 0;
	}
}

TArray<TSharedPtr<FJsonValue>> FCubicCameraSystemTakeParser::ParseJsonArrayFromFile(const FString& InFilePath)
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

TSharedPtr<FJsonObject> FCubicCameraSystemTakeParser::ParseJsonObjectFromFile(const FString& InFilePath)
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
