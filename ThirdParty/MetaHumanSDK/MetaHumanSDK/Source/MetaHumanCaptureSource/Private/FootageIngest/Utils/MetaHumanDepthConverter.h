// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "IMediaTextureSample.h"

#include "ImageWriteQueue.h"
#include "Utility/Error.h"

//	depth_data.bin file structure
//	TODO: These implementation is copied from the MetaHumanAnimatorLiveDataReceiver.h; We need to unify.
#pragma pack(push, 1)
struct FFrameHeader
{
	enum class EFrameType : uint8
	{
		TakeMetadata = 0,
		VideoMetadata = 1,
		DepthMetadata = 2,
		AudioMetadata = 3,
		VideoData = 4,
		DepthData = 5,
		AudioData = 6
	};

	EFrameType FrameType;
	char TimeCode[15];
	int64 TimeValue;
	int32 TimeScale;
	uint32 PayloadLength;
};
#pragma pack(pop)

struct FWriteDepthContext
{
	TSharedPtr<class IImageWrapper> ImageWrapper;
	FString ExrSequencePath;
	FIntPoint OutputSize;
	TArray<float> RotatedDepthBuffer;
	int32 FrameIndex;
	bool bShouldCompressFiles;
};

class FDepthWriteError
{
public:

	FDepthWriteError() = default;
	FDepthWriteError(FString InMessage);

	FDepthWriteError(const FDepthWriteError& InOther) = default;
	FDepthWriteError(FDepthWriteError&& InOther) = default;

	FDepthWriteError& operator=(const FDepthWriteError& InOther) = default;
	FDepthWriteError& operator=(FDepthWriteError&& InOther) = default;

	const FString& GetMessage() const;

private:

	FString Message;
};

using FDepthVoidResult = TResult<void, FDepthWriteError>;

class FDepthWriterTask : public IImageWriteTaskBase
{
public:
	using DepthWriteResult = TResult<int32, FDepthWriteError>;

	DECLARE_DELEGATE_OneParam(FOnWriteComplete, DepthWriteResult InResult);
	DECLARE_DELEGATE_TwoParams(FOnWrite, const FWriteDepthContext& InDepthContext, FOnWriteComplete InCallback);

	FDepthWriterTask()
	{
	}

	FDepthWriterTask(FWriteDepthContext InWriteDepthContext, FOnWrite InOnWrite, FOnWriteComplete InOnWriteComplete)
		: WriteDepthContext(MoveTemp(InWriteDepthContext))
		, OnWrite(MoveTemp(InOnWrite))
		, OnWriteComplete(MoveTemp(InOnWriteComplete))
	{
	}

	virtual bool RunTask() override
	{
		OnWrite.ExecuteIfBound(WriteDepthContext, OnWriteComplete);

		return true;
	}

	virtual void OnAbandoned() override
	{
	}

	FWriteDepthContext WriteDepthContext;
	FOnWrite OnWrite;
	FOnWriteComplete OnWriteComplete;
};

//	Depth Converter
class FDepthConverter final
{
public:
	FDepthConverter(bool bInShouldCompressFiles);
	~FDepthConverter();

	bool Open(const FString& InDepthFilePath, const FString& InExrSequencePath);
	void SetGeometry(FIntPoint InSize, EMediaOrientation InOrientation);

	//	Jump to the next frame. 
	bool Next();

	//	Save the current frame to the next exr file in the image sequence.
	void WriteAsync(FDepthWriterTask::FOnWriteComplete InOnWriteComplete);
	void WaitAsync();
	bool Write();
	void Close();

	const FText& GetError() const;

protected:
	FString DepthFilePath;
	FString ExrSequencePath;
	TUniquePtr<class IFileHandle> ReadHandle;
	FIntPoint InputSize;
	FIntPoint OutputSize;
	EMediaOrientation Orientation;
	TArray<uint8> CompressedDepthBuffer;
	TArray<int16> DepthBuffer;
	TArray<float> RotatedDepthBuffer;
	FText ErrorText;
	int	FrameIndex;
	bool bShouldCompressFiles;

	static constexpr float TrueDepthResolutionPerCentimeter = 80.0f;

private:
	class IImageWrapperModule& ImageWrapperModule;
	class IImageWriteQueue& ImageWriteQueue;

	bool Decompress();
	void Transform();
	FDepthVoidResult WriteToFile(const FWriteDepthContext& InWriteDepthContext);

	void OnWriteHandler(const FWriteDepthContext& InWriteDepthContext, FDepthWriterTask::FOnWriteComplete InOnWriteComplete);
};