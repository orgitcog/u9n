// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageSequenceWriter.h"

#if WITH_LIBJPEGTURBO

#include "IMediaTextureSample.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"

THIRD_PARTY_INCLUDES_START
#pragma push_macro("DLLEXPORT")
#undef DLLEXPORT // libjpeg-turbo defines DLLEXPORT as well
#include "turbojpeg.h"
#pragma pop_macro("DLLEXPORT")
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanImageSequenceWriter, Log, All);

class FImageSequenceWriter : public IImageSequenceWriter
{
public:
	FImageSequenceWriter();
	virtual ~FImageSequenceWriter() override;

	virtual bool Open(const FString& InDirPath) override;
	virtual bool Append(IMediaTextureSample* InTexture) override;
	virtual void Close() override;

protected:
	int32 SequenceIndex;
	FString DirPath;
	tjhandle TransformInstance;
	TArray<uint8> TransformBuffer;
};

FImageSequenceWriter::FImageSequenceWriter() : SequenceIndex(0), TransformInstance()
{
	TransformInstance = tjInitTransform();
}

FImageSequenceWriter::~FImageSequenceWriter()
{
	if (TransformInstance)
	{
		tjDestroy(TransformInstance);
		TransformInstance = nullptr;
	}
}

bool FImageSequenceWriter::Open(const FString& InDirPath)
{
	//	Create the directory if it doesn't exist.
	if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*InDirPath))
	{
		UE_LOG(LogMetaHumanImageSequenceWriter, Error, TEXT("Failed to create the image sequence directory: %s"), *InDirPath);
		return false;
	}

	DirPath = InDirPath;

	return true;
}

bool FImageSequenceWriter::Append(IMediaTextureSample* InTexture)
{
	const FString JPEGFileName = FPaths::Combine(DirPath, FString::Printf(TEXT("video_%06d.jpg"), SequenceIndex));

	const void* Buffer = InTexture->GetBuffer();
	checkf(Buffer != nullptr, TEXT("The texture should have a valid bitmap buffer"));

	checkf(InTexture->GetFormat() == EMediaTextureSampleFormat::Undefined, TEXT("Only MJPG textures are supported"));

	TUniquePtr<IFileHandle> Handle(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*JPEGFileName));

	if (!Handle)
	{
		UE_LOG(LogMetaHumanImageSequenceWriter, Error, TEXT("Cannot open the file: %s"), *JPEGFileName);
		return false;
	}

	if (InTexture->GetOrientation() == EMediaOrientation::Original)
	{
		if (!Handle->Write(static_cast<const uint8*>(Buffer), InTexture->GetStride()) || !Handle->Flush())
		{
			UE_LOG(LogMetaHumanImageSequenceWriter, Error, TEXT("Cannot write the content of the file: %s"), *JPEGFileName);
			return false;
		}
	}
	else {
		const int32 MaxSize = tjBufSize(InTexture->GetDim().X, InTexture->GetDim().Y, TJSAMP_444);
		tjtransform Transform;
		if (TransformBuffer.Num() < MaxSize)
		{
			TransformBuffer.SetNum(MaxSize);
		}
		Transform.customFilter = nullptr;
		Transform.data = nullptr;
		Transform.r.x = 0;
		Transform.r.y = 0;
		Transform.r.h = InTexture->GetDim().X;
		Transform.r.w = InTexture->GetDim().Y;
		switch (InTexture->GetOrientation())
		{
		case EMediaOrientation::CW90:
			Transform.op = TJXOP_ROT270;
			break;
		case EMediaOrientation::CW180:
			Transform.op = TJXOP_ROT180;
			break;
		case EMediaOrientation::CW270:
			Transform.op = TJXOP_ROT90;
			break;
		case EMediaOrientation::Original:
		default:
			Transform.op = TJXOP_NONE;
		}
		Transform.options = 0;
		unsigned char* OutBuffer = TransformBuffer.GetData();
		unsigned long OutBufferSize = TransformBuffer.Num();
		if (0 != tjTransform(TransformInstance, static_cast<const unsigned char*>(InTexture->GetBuffer()),
							 InTexture->GetStride(), 1, &OutBuffer, &OutBufferSize, &Transform, TJFLAG_NOREALLOC))
		{
			char* Error = tjGetErrorStr2(TransformInstance);
			if (Error == nullptr)
			{
				Error = tjGetErrorStr();
			}

			FString ErrorString(Error);

			UE_LOG(LogMetaHumanImageSequenceWriter, Error, TEXT("Failed to apply transform to image: %d. Error: %s"), SequenceIndex, *ErrorString);
			return false;
		}
		if (!Handle->Write(OutBuffer, OutBufferSize) || !Handle->Flush())
		{
			UE_LOG(LogMetaHumanImageSequenceWriter, Error, TEXT("Cannot write the content of the file: %s"), *JPEGFileName);
			return false;
		}
	}

	Handle->Flush();

	SequenceIndex += 1;

	return true;
}

void FImageSequenceWriter::Close()
{
	if (TransformInstance)
	{
		tjDestroy(TransformInstance);
		TransformInstance = nullptr;
	}
}

TSharedPtr<IImageSequenceWriter, ESPMode::ThreadSafe> IImageSequenceWriter::Create()
{
	return MakeShared<FImageSequenceWriter, ESPMode::ThreadSafe>();
}

#else // WITH_LIBJPEGTURBO

TSharedPtr<IImageSequenceWriter, ESPMode::ThreadSafe> IImageSequenceWriter::Create()
{
	checkf(false, TEXT("Image sequence writer is not supported on this platform"));
	return nullptr;
}

#endif // WITH_LIBJPEGTURBO
