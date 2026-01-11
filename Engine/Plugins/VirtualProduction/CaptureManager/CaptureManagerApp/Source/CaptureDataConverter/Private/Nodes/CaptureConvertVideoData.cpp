// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureConvertVideoData.h"

#include "Async/TaskProgress.h"
#include "Async/StopToken.h"

#include "CaptureManagerMediaRWModule.h"
#include "CaptureCopyProgressReporter.h"

#include "IImageWrapperModule.h"

#define LOCTEXT_NAMESPACE "CaptureConvertVideoData"

DEFINE_LOG_CATEGORY_STATIC(LogCaptureConvertVideoData, Log, All);

FCaptureConvertVideoData::FCaptureConvertVideoData(const FTakeMetadata::FVideo& InVideo, 
												   const FString& InOutputDirectory, 
												   const FCaptureConvertDataNodeParams& InParams,
												   const FCaptureConvertVideoOutputParams& InVideoParams)
	: FConvertVideoNode(InVideo, InOutputDirectory)
	, Params(InParams)
	, VideoParams(InVideoParams)
{
	checkf(!VideoParams.Format.IsEmpty(), TEXT("Output image format must be specified"));
}

FCaptureConvertVideoData::~FCaptureConvertVideoData() = default;

FCaptureConvertVideoData::FResult FCaptureConvertVideoData::Run()
{
	using namespace UE::CaptureManager;

	if (Params.StopToken.IsStopRequested())
	{
		FText Message = LOCTEXT("CaptureConvertVideo_AbortedByUser", "Video conversion aborted by user");
		return MakeError(MoveTemp(Message));
	}

	if (!Video.PathType.IsSet())
	{
		Video.PathType = FTakeMetadataPathUtils::DetectPathType(Video.Path);
		FString PathTypeString = FTakeMetadataPathUtils::PathTypeToString(Video.PathType.GetValue());
		UE_LOG(LogCaptureConvertVideoData, Display, TEXT("PathType for %s is unspecified, setting to detected type %s"), *Video.Path, *PathTypeString);
	}
	else
	{
		FTakeMetadataPathUtils::ValidatePathType(Video.Path, Video.PathType.GetValue());
	}

	if (Video.PathType.IsSet() && Video.PathType.GetValue() == FTakeMetadata::FVideo::EPathType::File)
	{
		return ConvertData();
	}
	
	return CopyData();
}

FCaptureConvertVideoData::FResult FCaptureConvertVideoData::ConvertData()
{
	using namespace UE::CaptureManager;

	FTaskProgress::FTask Task = Params.TaskProgress->StartTask();

	const FString VideoFilePath = FPaths::ConvertRelativePathToFull(Params.TakeOriginDirectory, Video.Path);
	FString DestinationDirectory = OutputDirectory / Video.Name;

	FCaptureManagerMediaRWModule& MediaRWModule =
		FModuleManager::LoadModuleChecked<FCaptureManagerMediaRWModule>("CaptureManagerMediaRW");

	TValueOrError<TUniquePtr<IVideoReader>, FText> VideoReaderResult = MediaRWModule.Get().CreateVideoReader(VideoFilePath);
	TValueOrError<TUniquePtr<IImageWriter>, FText> ImageWriterResult = MediaRWModule.Get().CreateImageWriter(DestinationDirectory, VideoParams.ImageFileName, VideoParams.Format);

	if (VideoReaderResult.HasError() || ImageWriterResult.HasError())
	{
		FText Message = FText::Format(LOCTEXT("CaptureConvertVideo_UnsupportedFile", "Video file format is unsupported {0}. Consider enabling Third Party Encoder in Capture Manager settings."), FText::FromString(VideoFilePath));
		return MakeError(MoveTemp(Message));
	}

	TUniquePtr<IVideoReader> VideoReader = VideoReaderResult.StealValue();
	TUniquePtr<IImageWriter> ImageWriter = ImageWriterResult.StealValue();

	ON_SCOPE_EXIT
	{
		VideoReader->Close();
		ImageWriter->Close();
	};

	const double TotalDuration = VideoReader->GetDuration().GetTotalSeconds();

	FResult Result = MakeValue();

	while (true)
	{
		TValueOrError<TUniquePtr<UE::CaptureManager::FMediaTextureSample>, FText> VideoSampleResult = VideoReader->Next();

		if (VideoSampleResult.HasError())
		{
			return MakeError(VideoSampleResult.StealError());
		}

		TUniquePtr<UE::CaptureManager::FMediaTextureSample> VideoSample = VideoSampleResult.StealValue();

		// End of stream
		if (!VideoSample.IsValid())
		{
			break;
		}

		FWritingContext Context;
		Context.ReadSample = MoveTemp(VideoSample);
		Context.Writer = ImageWriter.Get();
		Context.TotalDuration = TotalDuration;
		Context.Task = &Task;

		FWriteResult WriteResult = OnWrite(MoveTemp(Context));

		if (WriteResult.HasError())
		{
			return MakeError(WriteResult.StealError());
		}
	}

	return Result;
}

FCaptureConvertVideoData::FResult FCaptureConvertVideoData::CopyData()
{
	using namespace UE::CaptureManager;

	const FString VideoFolderPath = FPaths::ConvertRelativePathToFull(Params.TakeOriginDirectory, Video.Path);

	IFileManager& FileManager = IFileManager::Get();
	if (!Video.FramesCount.IsSet() || Video.FramesCount.GetValue() == 0)
	{
		TArray<FString> FoundFiles;
		FileManager.FindFiles(FoundFiles, *VideoFolderPath, Video.Format.IsEmpty() ? nullptr : *Video.Format);
		
		if (FoundFiles.IsEmpty())
		{
			FText ExtensionPostfix = Video.Format.IsEmpty() ? FText() : FText::Format(LOCTEXT("CaptureConvertVideo_Extension", " with specified extension .{0}"), FText::FromString(Video.Format));
			FText Message = FText::Format(LOCTEXT("CaptureConvertVideo_EmptyData", "Copy image data failed. No image data found at {0}{1}"), FText::FromString(VideoFolderPath), ExtensionPostfix);
			UE_LOG(LogCaptureConvertVideoData, Error, TEXT("%s"), *Message.ToString());

			return MakeError(MoveTemp(Message));
		}

		Video.FramesCount = FoundFiles.Num();
	}

	FTaskProgress::FTask Task = Params.TaskProgress->StartTask();
	TSharedPtr<FTaskProgress> TaskProgress = MakeShared<FTaskProgress>(Video.FramesCount.GetValue(), FTaskProgress::FProgressReporter::CreateLambda([&Task](float InProgress)
	{
		Task.Update(InProgress);
	}));

	FString DestinationDirectory = OutputDirectory / Video.Name;

	FCaptureConvertVideoData::FResult Result = MakeValue();

	IImageWrapperModule& ImageWrapperModule = FModuleManager::Get().LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	FileManager.IterateDirectoryRecursively(*VideoFolderPath, [this, &TaskProgress, &Result, DestinationDirectory, &FileManager, &ImageWrapperModule, VideoFolderPath](const TCHAR* InFileName, bool bInIsDirectory)
	{
		if (bInIsDirectory)
		{
			return true;
		}

		if (ImageWrapperModule.GetImageFormatFromExtension(InFileName) == EImageFormat::Invalid)
		{
			FText Message = FText::Format(LOCTEXT("CaptureConvertVideoData_UnsupportedFileFormat", "Image file format is unsupported {0}"), 
										  FText::FromString(InFileName));
			Result = MakeError(MoveTemp(Message));
			return false;
		}
		
		FTaskProgress::FTask Task = TaskProgress->StartTask();

		FCopyProgressReporter ProgressReporter(Task, Params.StopToken);

		FString Destination = DestinationDirectory / FPaths::GetCleanFilename(InFileName);

		uint32 CopyResult = FileManager.Copy(*Destination, InFileName, true, true, false, &ProgressReporter);

		if (CopyResult == COPY_Fail)
		{
			FText Message = FText::Format(LOCTEXT("CaptureConvertVideoData_CopyFailed", "Failed to copy file {0} from {1} to {2}"), FText::FromString(InFileName), FText::FromString(VideoFolderPath), FText::FromString(DestinationDirectory));
			Result = MakeError(MoveTemp(Message));
			return false;
		}

		if (CopyResult == COPY_Canceled)
		{
			FText Message = LOCTEXT("CaptureConvertVideoData_AbortedByUser", "Image data copy aborted by user");
			Result = MakeError(MoveTemp(Message));
			return false;
		}

		return true;
	});

	return MakeValue();
}

FCaptureConvertVideoData::FWriteResult FCaptureConvertVideoData::OnWrite(FWritingContext InContext)
{
	IImageWriter* Writer = InContext.Writer;

	TUniquePtr<UE::CaptureManager::FMediaTextureSample>& VideoSample = InContext.ReadSample;

	EMediaOrientation Orientation = ConvertOrientation(Video.Orientation.Get(FTakeMetadata::FVideo::EOrientation::Original));
	VideoSample->Orientation = Orientation;
	VideoSample->Rotation = VideoParams.Rotation;
	VideoSample->DesiredFormat = VideoParams.OutputPixelFormat;

	TOptional<FText> WriterResult = Writer->Append(VideoSample.Get());

	if (WriterResult.IsSet())
	{
		return MakeError(MoveTemp(WriterResult.GetValue()));
	}

	const double Time = VideoSample->Time.GetTotalSeconds();
	if (InContext.TotalDuration > 0.0)
	{
		InContext.Task->Update(Time / InContext.TotalDuration);
	}

	if (Params.StopToken.IsStopRequested())
	{
		FText Message = LOCTEXT("CaptureConvertVideo_AbortedByUser", "Video conversion aborted by user");
		return MakeError(MoveTemp(Message));
	}

	return MakeValue();
}

EMediaOrientation FCaptureConvertVideoData::ConvertOrientation(FTakeMetadata::FVideo::EOrientation InOrientation) const
{
	switch (InOrientation)
	{
		case FTakeMetadata::FVideo::EOrientation::CW90:
			return EMediaOrientation::CW90;
		case FTakeMetadata::FVideo::EOrientation::CW180:
			return EMediaOrientation::CW180;
		case FTakeMetadata::FVideo::EOrientation::CW270:
			return EMediaOrientation::CW270;
		case FTakeMetadata::FVideo::EOrientation::Original:
		default:
			return EMediaOrientation::Original;
	}
}

#undef LOCTEXT_NAMESPACE