// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FootageIngest/LiveLinkFaceMetadata.h"
#include "Error/Result.h"
#include "MetaHumanCaptureError.h"
#include "Async/StopToken.h"
#include "Async/AsyncWork.h"

class FIngestTask : public FNonAbandonableTask
{
public:

	DECLARE_DELEGATE_TwoParams(FTaskHandler, const FIngestTask& InThisTask, const FStopToken& InStopToken);

	FIngestTask(int32 InId, FTaskHandler InTaskHandler, const FStopToken& InStopToken)
		: Id(InId)
		, TaskHandler(MoveTemp(InTaskHandler))
		, StopToken(InStopToken)
	{
	}

	TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FIngestTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork()
	{
		TaskHandler.ExecuteIfBound(*this, StopToken);
	}

	int32 Id;
	FTaskHandler TaskHandler;
	const FStopToken& StopToken;
};

class FLiveLinkFaceTakeDataConverter
{
public:
	DECLARE_DELEGATE_OneParam(FOnProgress, const float);
	using FOnFinished = TDelegate<void(TResult<void, FMetaHumanCaptureError>)>;

	virtual ~FLiveLinkFaceTakeDataConverter() {}

	struct FConvertParams
	{
		FLiveLinkFaceTakeInfo TakeInfo;
		FString TargetIngestDirectory;
		FString TargetIngestPackagePath;
	};

	struct FConvertResult
	{
		FString TargetIngestPackagePath;
		FString ImageSequenceDirectory;
		FString DepthSequenceDirectory;
		FString WAVFilePath;
		bool bVideoTimecodePresent = false;
		FTimecode VideoTimecode;
		bool bAudioTimecodePresent = false;
		FTimecode AudioTimecode;
		FFrameRate TimecodeRate;
		TArray<FFrameRange> CaptureExcludedFrames;
	};

	virtual FConvertResult Convert(const FStopToken& InStopToken);
	virtual bool Initialize(const FConvertParams& InConvertParams);

	FOnFinished& OnFinished()
	{
		return OnFinishedDelegate;
	}

	FOnProgress& OnProgress()
	{
		return OnProgressDelegate;
	}

protected:
	//	Steps to perform the task
	void ConvertMovToWav(const FIngestTask& InTask, const FStopToken& InStopToken); 
	bool ConvertVideoToImageSequenceImplementation(const FIngestTask& InTask, const FStopToken& InStopToken);
	double TotalProgressForImageSequence = 1.0;
	void ConvertVideoToImageSequence(const FIngestTask& InTask, const FStopToken& InStopToken);
	void ConvertVideoToDepth(const FIngestTask& InTask, const FStopToken& InStopToken); 

	class FrameLogEntry
	{
	public:
		static const char VideoType = 'V';
		static const char DepthType = 'D';
		static const char AudioType = 'A';
		static const char InvalidType = '\0';

		FrameLogEntry() = default;
		static bool Parse(const FString& InLogLine, FrameLogEntry& OutLogEntry);

		char EntryType();
		int64 FrameIndex();
		double Time();
		bool Timecode(FTimecode& OutTimecode);
		bool IsDroppedFrame();

	private:
		FrameLogEntry(TArray<FString>&& InTokens);

		TArray<FString> Tokens;
	};

	void ExecuteAsyncTasks(const FStopToken& InStopToken); 


	/* Video - depth synchronization */
	void BuildVideoDepthSyncMap(const TArray<TPair<double, bool>>& InVideoFrameTimeInSeconds,
								const TArray<TPair<double, bool>>& InDepthFrameTimeInSeconds);

	void NotifyProgress(float InProgress);
	void NotifyFailure(FMetaHumanCaptureError InError);
	void NotifySuccess();

	void OnAsyncTaskProgressUpdate(const FIngestTask& InTask, float InAsyncTaskProgress);

	void ParseFrameLog(TArray<TPair<double, bool>>& OutVideoFrameTimeInSeconds,
					   TArray<TPair<double, bool>>& OutDepthFrameTimeInSeconds);

	bool bInitialized = false;
	FConvertParams ConvertParams;

	FLiveLinkFaceTakeInfo TakeInfo;
	FString TargetIngestDirectory;
	FString TargetIngestPackagePath;

	FString TargetWAVFilePath;
	FString TargetVideoSequenceDirectory;
	FString TargetDepthSequenceDirectory;

	FTimecode VideoTimecode;
	FTimecode AudioTimecode;
	FFrameRate TimecodeRate;

	FOnFinished OnFinishedDelegate;
	FOnProgress OnProgressDelegate;

	/* (Video frame index, Depth frame index, sync'd) synchronizing pairs */
	TArray<TTuple<int32, int32, bool>> VideoDepthSyncMap;
	TArray<FFrameRange> CaptureExcludedFrames;
	TArray<std::atomic<float>> AsyncTaskProgresses;
};
