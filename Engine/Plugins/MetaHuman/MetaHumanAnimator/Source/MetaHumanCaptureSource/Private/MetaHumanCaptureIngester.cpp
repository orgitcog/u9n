// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCaptureIngester.h"

#include "MetaHumanCaptureSourceLog.h"
#include "MetaHumanCaptureEvents.h"

#include "FootageIngest/Utils/CommandHandler.h"
#include "FootageIngest/LiveLinkFaceFootageIngest.h"
#include "FootageIngest/LiveLinkFaceConnectionIngest.h"
#include "FootageIngest/HMCArchiveIngest.h"

namespace UE::MetaHuman
{
FIngesterParams::FIngesterParams(
	EMetaHumanCaptureSourceType InCaptureSourceType,
	FDirectoryPath InStoragePath,
	FDeviceAddress InDeviceAddress,
	uint16 InDeviceControlPort,
	bool InShouldCompressDepthFiles,
	bool InCopyImagesToProject,
	float InMinDistance,
	float InMaxDistance,
	EMetaHumanCaptureDepthPrecisionType InDepthPrecision,
	EMetaHumanCaptureDepthResolutionType InDepthResolution
) :
	CaptureSourceType(InCaptureSourceType),
	StoragePath(InStoragePath),
	DeviceAddress(InDeviceAddress),
	DeviceControlPort(InDeviceControlPort),
	ShouldCompressDepthFiles(InShouldCompressDepthFiles),
	CopyImagesToProject(InCopyImagesToProject),
	MinDistance(InMinDistance),
	MaxDistance(InMaxDistance),
	DepthPrecision(InDepthPrecision),
	DepthResolution(InDepthResolution)
{
}

FIngester::FIngester(FIngesterParams InIngesterParams) :
	Params(MoveTemp(InIngesterParams))
{
	RegisterEvent(FTakeListResetEvent::Name);
	RegisterEvent(FNewTakesAddedEvent::Name);
	RegisterEvent(FThumbnailChangedEvent::Name);
	RegisterEvent(FConnectionChangedEvent::Name);
	RegisterEvent(FRecordingStatusChangedEvent::Name);
	RegisterEvent(FTakesRemovedEvent::Name);
}

void FIngester::SetParams(FIngesterParams InIngesterParams)
{
	Params = MoveTemp(InIngesterParams);
}

FIngester::~FIngester()
{
	Shutdown();
}

EMetaHumanCaptureSourceType FIngester::GetCaptureSourceType() const
{
	return Params.CaptureSourceType;
}

bool FIngester::CanStartup() const
{
	if (IsProcessing())
	{
		return false;
	}

	switch (Params.CaptureSourceType)
	{
	case EMetaHumanCaptureSourceType::LiveLinkFaceArchives:
	case EMetaHumanCaptureSourceType::HMCArchives:
		return !Params.StoragePath.Path.IsEmpty() && FPaths::DirectoryExists(Params.StoragePath.Path);

	case EMetaHumanCaptureSourceType::LiveLinkFaceConnection:
		return !Params.DeviceAddress.IpAddress.IsEmpty();

	case EMetaHumanCaptureSourceType::Undefined:
	default:
		break;
	}

	return false;
}

bool FIngester::CanIngestTakes() const
{
	return !IsProcessing() && GetNumTakes() > 0;
}

bool FIngester::CanCancel() const
{
	return IsProcessing() && !IsCancelling();
}

void FIngester::Startup(ETakeIngestMode InMode)
{
	// Shutdown the API in case its already valid
	Shutdown();

	// Assert if startup is called with invalid data
	if (!CanStartup())
	{
		return;
	}

	switch (Params.CaptureSourceType)
	{
	case EMetaHumanCaptureSourceType::LiveLinkFaceArchives:
		FootageIngestAPI = MakeUnique<FLiveLinkFaceArchiveIngest>(Params.StoragePath.Path, Params.ShouldCompressDepthFiles);
		break;

	case EMetaHumanCaptureSourceType::LiveLinkFaceConnection:
		FootageIngestAPI = MakeUnique<FLiveLinkFaceConnectionIngest>(
			Params.DeviceAddress.IpAddress,
			Params.DeviceControlPort,
			Params.ShouldCompressDepthFiles
		);
		break;

	case EMetaHumanCaptureSourceType::HMCArchives:
		FootageIngestAPI = MakeUnique<FHMCArchiveIngest>(
			Params.StoragePath.Path,
			Params.ShouldCompressDepthFiles,
			Params.CopyImagesToProject,
			TRange<float>(Params.MinDistance, Params.MaxDistance),
			Params.DepthPrecision,
			Params.DepthResolution
		);
		break;

	case EMetaHumanCaptureSourceType::Undefined:
	default:
		// TODO: Log error
		FootageIngestAPI.Reset();
		break;
	}

	if (FootageIngestAPI.IsValid())
	{
		FCaptureEventHandler CaptureEventHandler = FCaptureEventHandler(
			FCaptureEventHandler::Type::CreateRaw(this, &FIngester::ProxyEvent),
			EDelegateExecutionThread::InternalThread
		);

		// Subscribe to all supported events so we can pass them on
		FootageIngestAPI->SubscribeToEvent(FTakeListResetEvent::Name, CaptureEventHandler);
		FootageIngestAPI->SubscribeToEvent(FNewTakesAddedEvent::Name, CaptureEventHandler);
		FootageIngestAPI->SubscribeToEvent(FThumbnailChangedEvent::Name, CaptureEventHandler);
		FootageIngestAPI->SubscribeToEvent(FConnectionChangedEvent::Name, CaptureEventHandler);
		FootageIngestAPI->SubscribeToEvent(FRecordingStatusChangedEvent::Name, CaptureEventHandler);
		FootageIngestAPI->SubscribeToEvent(FTakesRemovedEvent::Name, CaptureEventHandler);

		FootageIngestAPI->OnGetTakesFinished().BindLambda([this](const TArray<FMetaHumanTake>& InTakes)
			{
				// Broadcast the newly ingested takes for interested parties
				OnGetTakesFinishedDelegate.Broadcast(InTakes);
			});

		FootageIngestAPI->Startup(InMode);

	}
}

void FIngester::Refresh(FRefreshCallback InCallback)
{
	if (FootageIngestAPI.IsValid())
	{
		IFootageIngestAPI::TCallback<void> Callback =
			IFootageIngestAPI::TCallback<void>([UserCallback = MoveTemp(InCallback)](TResult<void, FMetaHumanCaptureError> InResult)
				{
					FMetaHumanCaptureVoidResult Result;
					Result.SetResult(MoveTemp(InResult));
					UserCallback(MoveTemp(Result));

				}, EDelegateExecutionThread::InternalThread);

		FootageIngestAPI->RefreshTakeListAsync(MoveTemp(Callback));
	}
}

void FIngester::SetTargetPath(const FString& InTargetIngestDirectory, const FString& InTargetFolderAssetPath)
{
	if (FootageIngestAPI.IsValid())
	{
		FootageIngestAPI->SetTargetPath(InTargetIngestDirectory, InTargetFolderAssetPath);
	}
}

void FIngester::Shutdown()
{
	if (FootageIngestAPI.IsValid())
	{
		FootageIngestAPI->UnsubscribeAll();
		FootageIngestAPI->Shutdown();
	}
}

bool FIngester::IsProcessing() const
{
	if (FootageIngestAPI.IsValid())
	{
		return FootageIngestAPI->IsProcessing();
	}

	return false;
}

bool FIngester::IsCancelling() const
{
	if (FootageIngestAPI.IsValid())
	{
		return FootageIngestAPI->IsCancelling();
	}

	return false;
}

void FIngester::CancelProcessing(const TArray<TakeId>& InTakeIdList)
{
	if (FootageIngestAPI.IsValid())
	{
		FootageIngestAPI->CancelProcessing(InTakeIdList);
	}
}

int32 FIngester::GetNumTakes() const
{
	if (FootageIngestAPI.IsValid())
	{
		return FootageIngestAPI->GetNumTakes();
	}

	return 0;
}

TArray<TakeId> FIngester::GetTakeIds() const
{
	if (FootageIngestAPI.IsValid())
	{
		return FootageIngestAPI->GetTakeIds();
	}

	return {};
}

bool FIngester::GetTakeInfo(TakeId InTakeId, FMetaHumanTakeInfo& OutTakeInfo) const
{
	if (FootageIngestAPI.IsValid())
	{
		OutTakeInfo = FootageIngestAPI->GetTakeInfo(InTakeId);
		return true;
	}

	return false;
}

bool FIngester::GetTakes(const TArray<TakeId>& InTakeIdList, FGetTakesCallbackPerTake InCallback)
{
	if (FootageIngestAPI.IsValid() && !IsProcessing())
	{
		IFootageIngestAPI::TPerTakeCallback<void> PerTakeCallback =
			IFootageIngestAPI::TPerTakeCallback<void>([UserCallback = MoveTemp(InCallback)](IFootageIngestAPI::TPerTakeResult<void> InResult)
				{
					FMetaHumanCapturePerTakeVoidResult PerTakeResult;
					PerTakeResult.TakeId = InResult.Key;
					PerTakeResult.Result.SetResult(MoveTemp(InResult.Value));

					UserCallback(MoveTemp(PerTakeResult));

				}, EDelegateExecutionThread::InternalThread);

		FootageIngestAPI->GetTakes(InTakeIdList, MoveTemp(PerTakeCallback));
		return true;
	}

	return false;
}

TOptional<float> FIngester::GetProcessingProgress(TakeId InTakeId) const
{
	if (FootageIngestAPI.IsValid())
	{
		return FootageIngestAPI->GetTaskProgress(InTakeId);
	}

	return 0.0f;
}

FText FIngester::GetProcessName(TakeId InTakeId) const
{
	if (FootageIngestAPI.IsValid())
	{
		return FootageIngestAPI->GetTaskName(InTakeId);
	}

	return FText();
}

bool FIngester::ExecuteCommand(TSharedPtr<FBaseCommandArgs> InCommand)
{
	if (FootageIngestAPI.IsValid())
	{
		return FootageIngestAPI->Execute(MoveTemp(InCommand));
	}

	return false;
}

void FIngester::ProxyEvent(TSharedPtr<const FCaptureEvent> Event)
{
	PublishEventPtr(MoveTemp(Event));
}

}
