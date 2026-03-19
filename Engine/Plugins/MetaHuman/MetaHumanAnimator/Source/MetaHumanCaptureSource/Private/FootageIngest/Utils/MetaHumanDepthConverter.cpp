// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDepthConverter.h"
#include "MetaHumanCaptureSourceLog.h"

#include "Modules/ModuleManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <oodle2.h>


#define LOCTEXT_NAMESPACE "MetaHumanDepthConverter"

FDepthWriteError::FDepthWriteError(FString InMessage)
	: Message(MoveTemp(InMessage))
{
}

const FString& FDepthWriteError::GetMessage() const
{
	return Message;
}

FDepthConverter::FDepthConverter(bool bInShouldCompressFiles)
	: ReadHandle(nullptr)
	, Orientation()
	, FrameIndex(0)
	, bShouldCompressFiles(bInShouldCompressFiles)
	, ImageWrapperModule(FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper")))
	, ImageWriteQueue(FModuleManager::Get().LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue").GetWriteQueue())
{
}

bool FDepthConverter::Open(const FString& InDepthFilePath, const FString& InExrSequencePath)
{
	DepthFilePath = InDepthFilePath;
	ExrSequencePath = InExrSequencePath;

	ReadHandle.Reset(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*InDepthFilePath));
	if (!ReadHandle)
	{
		ErrorText = FText::Format(LOCTEXT("DepthFileOpenFailed", "Failed to open the depth file: {0}."), FText::FromString(InDepthFilePath));
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("%s"), *ErrorText.ToString());
		return false;
	}

	if (const bool bCreatedDirectory = FPlatformFileManager::Get().GetPlatformFile().CreateDirectory(*InExrSequencePath)
		; !bCreatedDirectory)
	{
		ErrorText = FText::Format(
			LOCTEXT("DirectoryCreationFailed", "Failed to create the directory: {0}."),
			FText::FromString(InExrSequencePath));
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("%s"), *ErrorText.ToString());
		return false;
	}

	return true;
}

void FDepthConverter::SetGeometry(const FIntPoint InSize, const EMediaOrientation InOrientation)
{
	InputSize = InSize;
	Orientation = InOrientation;

	switch (Orientation)
	{
	case EMediaOrientation::CW90:
	case EMediaOrientation::CW270:
		OutputSize = FIntPoint(InputSize.Y, InputSize.X);
		break;
	case EMediaOrientation::Original:
	case EMediaOrientation::CW180:
	default:
		OutputSize = InputSize;
		break;
	}

	DepthBuffer.SetNumZeroed(InputSize.X * InputSize.Y);
	RotatedDepthBuffer.SetNumZeroed(InputSize.X * InputSize.Y);
}

bool FDepthConverter::Next()
{
	FFrameHeader Header;

	if (!ReadHandle)
	{
		return false;
	}

	while (ReadHandle->Read(reinterpret_cast<uint8_t*>(&Header), sizeof Header))
	{
		switch (Header.FrameType)
		{
		case FFrameHeader::EFrameType::DepthData:
			CompressedDepthBuffer.SetNum(Header.PayloadLength);
			if (!ReadHandle->Read(CompressedDepthBuffer.GetData(), Header.PayloadLength))
			{
				ErrorText = FText::Format(LOCTEXT("DepthFileDepthDataReadFailed", "Failed to read the depth file: {0}."), FText::FromString(DepthFilePath));
				UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("%s"), *ErrorText.ToString());
				return false;
			}
			return true;
		default:
			if (!ReadHandle->Seek(ReadHandle->Tell() + Header.PayloadLength))
			{
				ErrorText = FText::Format(
					LOCTEXT("DepthFileReadFailed", "Failed to read from the depth file: {0}."),
					FText::FromString(ExrSequencePath));
				UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("%s"), *ErrorText.ToString());
			}
		}
	}

	ErrorText = FText::FromString(TEXT("End of file reached."));
	return false;
}

inline int16 ZigzagDecode(int16 InValue)
{
	return (InValue >> 1) ^ (-(InValue & 1));
}

void FDepthConverter::WriteAsync(FDepthWriterTask::FOnWriteComplete InOnWriteComplete)
{
	if (!Decompress())
	{
		InOnWriteComplete.ExecuteIfBound(FDepthWriteError(ErrorText.ToString()));
	}

	Transform();

	FWriteDepthContext WriteContext = { ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR), ExrSequencePath, OutputSize, RotatedDepthBuffer, FrameIndex, bShouldCompressFiles };
	FDepthWriterTask::FOnWrite OnWriteHandler = FDepthWriterTask::FOnWrite::CreateRaw(this, &FDepthConverter::OnWriteHandler);

	TUniquePtr<FDepthWriterTask> Task = MakeUnique<FDepthWriterTask>(MoveTemp(WriteContext), MoveTemp(OnWriteHandler), MoveTemp(InOnWriteComplete));

	ImageWriteQueue.Enqueue(MoveTemp(Task));

	++FrameIndex;
}

void FDepthConverter::WaitAsync()
{
	TFuture<void> Fence = ImageWriteQueue.CreateFence();
	Fence.Wait();
}

bool FDepthConverter::Write()
{
	if (!Decompress())
	{
		return false;
	}

	Transform();

	FWriteDepthContext WriteContext = { ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR), ExrSequencePath, OutputSize, RotatedDepthBuffer, FrameIndex, bShouldCompressFiles };
	FDepthVoidResult WriteToFileResult = WriteToFile(WriteContext);
	if (WriteToFileResult.IsError())
	{
		ErrorText = FText::FromString(WriteToFileResult.ClaimError().GetMessage());
	}

	++FrameIndex;

	return WriteToFileResult.IsValid();
}

bool FDepthConverter::Decompress()
{
	//	Decompress and rotate the depth data.
	if (OodleLZ_Decompress(CompressedDepthBuffer.GetData(), CompressedDepthBuffer.Num(),
						   DepthBuffer.GetData(), DepthBuffer.Num() * sizeof(int16)) != DepthBuffer.Num() * sizeof(int16))
	{
		ErrorText = FText::Format(
			LOCTEXT("DepthDataCorrupted", "Corrupted depth data: {0}."),
			FText::FromString(DepthFilePath));
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("%s"), *ErrorText.ToString());
		return false;
	}

	//	Process the zigzag decoding and un-differentiate the depth values
	int16 PreviousValue = 0;

	for (int32 Index = 0; Index < DepthBuffer.Num(); ++Index)
	{
		const int16 ThisValue = PreviousValue + ZigzagDecode(DepthBuffer[Index]);
		PreviousValue = ThisValue;
		DepthBuffer[Index] = ThisValue;
	}

	return true;
}

void FDepthConverter::Transform()
{
	switch (Orientation)
	{
		case EMediaOrientation::Original:
			for (int32 Y = 0; Y < OutputSize.Y; ++Y)
			{
				for (int32 X = 0; X < OutputSize.X; ++X)
				{
					RotatedDepthBuffer[Y * OutputSize.X + X] =
						DepthBuffer[Y * InputSize.X + X] / TrueDepthResolutionPerCentimeter;
				}
			}
			break;
		case EMediaOrientation::CW90:
			for (int32 Y = 0; Y < OutputSize.Y; ++Y)
			{
				for (int32 X = 0; X < OutputSize.X; ++X)
				{
					RotatedDepthBuffer[Y * OutputSize.X + X] =
						DepthBuffer[X * InputSize.X + (OutputSize.Y - Y - 1)] / TrueDepthResolutionPerCentimeter;
				}
			}
		case EMediaOrientation::CW180:
			for (int32 Y = 0; Y < OutputSize.Y; ++Y)
			{
				for (int32 X = 0; X < OutputSize.X; ++X)
				{
					RotatedDepthBuffer[Y * OutputSize.X + X] =
						DepthBuffer[(InputSize.Y - Y - 1) * InputSize.X + (InputSize.X - X - 1)] / TrueDepthResolutionPerCentimeter;
				}
			}
			break;
		case EMediaOrientation::CW270:
			for (int32 Y = 0; Y < OutputSize.Y; ++Y)
			{
				for (int32 X = 0; X < OutputSize.X; ++X)
				{
					RotatedDepthBuffer[Y * OutputSize.X + X] =
						DepthBuffer[(OutputSize.X - X - 1) * InputSize.X + Y] / TrueDepthResolutionPerCentimeter;
				}
			}
			break;
	}
}

FDepthVoidResult FDepthConverter::WriteToFile(const FWriteDepthContext& InWriteContext)
{
	const FString ExrFilePath = FPaths::Combine(InWriteContext.ExrSequencePath, FString::Printf(TEXT("depth_%06d.exr"), InWriteContext.FrameIndex));

	if (!InWriteContext.ImageWrapper)
	{
		FString Error = TEXT("Failed to create the image wrapper.");
		return FDepthWriteError(MoveTemp(Error));
	}

	if (!InWriteContext.ImageWrapper->SetRaw(InWriteContext.RotatedDepthBuffer.GetData(), InWriteContext.RotatedDepthBuffer.Num() * sizeof(float), InWriteContext.OutputSize.X, InWriteContext.OutputSize.Y, ERGBFormat::GrayF, sizeof(float) * 8))
	{
		FText Error = FText::Format(LOCTEXT("DepthImageCreationFailed", "Failed to create the depth image: {0}."), FText::FromString(ExrFilePath));
		return FDepthWriteError(Error.ToString());
	}

	EImageCompressionQuality Compression = InWriteContext.bShouldCompressFiles ? EImageCompressionQuality::Default : EImageCompressionQuality::Uncompressed;

	const TArray64<uint8> ExrBuffer = InWriteContext.ImageWrapper->GetCompressed((int32) Compression);

	if (!FFileHelper::SaveArrayToFile(ExrBuffer, *ExrFilePath))
	{
		FText Error = FText::Format(LOCTEXT("DepthImageSaveFailed", "Failed to save the depth image: {0}."), FText::FromString(ExrFilePath));
		return FDepthWriteError(Error.ToString());
	}

	return ResultOk;
}

void FDepthConverter::Close()
{
	DepthBuffer.Reset();
	RotatedDepthBuffer.Reset();

	ReadHandle = nullptr;
}

const FText& FDepthConverter::GetError() const
{
	return ErrorText;
}

FDepthConverter::~FDepthConverter()
{
	Close();

	WaitAsync();
}

void FDepthConverter::OnWriteHandler(const FWriteDepthContext& InWriteDepthContext, FDepthWriterTask::FOnWriteComplete InOnWriteComplete)
{
	FDepthVoidResult WriteToFileResult = WriteToFile(InWriteDepthContext);
	if (WriteToFileResult.IsError())
	{
		InOnWriteComplete.ExecuteIfBound(WriteToFileResult.ClaimError());
		return;
	}

	InOnWriteComplete.ExecuteIfBound(InWriteDepthContext.FrameIndex);
}

#undef LOCTEXT_NAMESPACE