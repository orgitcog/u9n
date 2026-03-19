// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCaptureSourceSync.h"
#include "MetaHumanCaptureSourceLog.h"

#include "MetaHumanCaptureIngester.h"

#include "FootageIngest/LiveLinkFaceFootageIngest.h"
#include "FootageIngest/LiveLinkFaceConnectionIngest.h"
#include "FootageIngest/HMCArchiveIngest.h"

#include "UObject/UnrealTypePrivate.h"

#include "Algo/AllOf.h"

UMetaHumanCaptureSourceSync::UMetaHumanCaptureSourceSync() : 
	MetaHumanCaptureSource(NewObject<UMetaHumanCaptureSource>())
{
	using namespace UE::MetaHuman;

	Ingester = MakeUnique<FIngester>(
		FIngesterParams(
			MetaHumanCaptureSource->CaptureSourceType,
			MetaHumanCaptureSource->StoragePath,
			MetaHumanCaptureSource->DeviceIpAddress,
			MetaHumanCaptureSource->DeviceControlPort,
			MetaHumanCaptureSource->ShouldCompressDepthFiles,
			MetaHumanCaptureSource->CopyImagesToProject,
			MetaHumanCaptureSource->MinDistance,
			MetaHumanCaptureSource->MaxDistance,
			MetaHumanCaptureSource->DepthPrecision,
			MetaHumanCaptureSource->DepthResolution
		)
	);

}

UMetaHumanCaptureSourceSync::UMetaHumanCaptureSourceSync(FVTableHelper& InHelper) : 
	Super(InHelper),
	MetaHumanCaptureSource(NewObject<UMetaHumanCaptureSource>())
{
}

UMetaHumanCaptureSourceSync::~UMetaHumanCaptureSourceSync()
{
}

bool UMetaHumanCaptureSourceSync::CanStartup() const
{
	return Ingester->CanStartup();
}

bool UMetaHumanCaptureSourceSync::CanIngestTakes() const
{
	return Ingester->CanIngestTakes();
}

bool UMetaHumanCaptureSourceSync::CanCancel() const
{
	return Ingester->CanCancel();
}

void UMetaHumanCaptureSourceSync::Startup()
{
	Ingester->Startup(ETakeIngestMode::Blocking);
}

TArray<FMetaHumanTakeInfo> UMetaHumanCaptureSourceSync::Refresh()
{
	using namespace UE::MetaHuman;

	TPromise<void> Promise;
	TFuture<void> Future = Promise.GetFuture();

	FIngester::FRefreshCallback Callback = FIngester::FRefreshCallback([&Promise](FMetaHumanCaptureVoidResult InResult)
	{
		Promise.SetValue();
	}, EDelegateExecutionThread::InternalThread);

	Ingester->Refresh(MoveTemp(Callback));

	Future.Wait();

	TArray<TakeId> TakeIds = GetTakeIds();

	TArray<FMetaHumanTakeInfo> TakeInfos;
	TakeInfos.Reserve(TakeIds.Num());

	for (TakeId Id : TakeIds)
	{
		FMetaHumanTakeInfo TakeInfo;
		GetTakeInfo(Id, TakeInfo);

		TakeInfos.Add(MoveTemp(TakeInfo));
	}

	return TakeInfos;
}

void UMetaHumanCaptureSourceSync::SetTargetPath(const FString& InTargetIngestDirectory, const FString& InTargetFolderAssetPath)
{
	Ingester->SetTargetPath(InTargetIngestDirectory, InTargetFolderAssetPath);
}

void UMetaHumanCaptureSourceSync::Shutdown()
{
	Ingester->Shutdown();
}

bool UMetaHumanCaptureSourceSync::IsProcessing() const
{
	return Ingester->IsProcessing();
}

bool UMetaHumanCaptureSourceSync::IsCancelling() const
{
	return Ingester->IsCancelling();
}

void UMetaHumanCaptureSourceSync::CancelProcessing(const TArray<TakeId>& InTakeIdList)
{
	Ingester->CancelProcessing(InTakeIdList);
}

int32 UMetaHumanCaptureSourceSync::GetNumTakes() const
{
	return Ingester->GetNumTakes();
}

TArray<TakeId> UMetaHumanCaptureSourceSync::GetTakeIds() const
{
	return Ingester->GetTakeIds();
}

bool UMetaHumanCaptureSourceSync::GetTakeInfo(int32 InTakeId, FMetaHumanTakeInfo& OutTakeInfo) const
{
	return Ingester->GetTakeInfo(InTakeId, OutTakeInfo);
}

TArray<FMetaHumanTake> UMetaHumanCaptureSourceSync::GetTakes(const TArray<TakeId>& InTakeIdList)
{
	using namespace UE::MetaHuman;

	FIngester::FGetTakesCallbackPerTake PerTakeCallback = FIngester::FGetTakesCallbackPerTake([](FMetaHumanCapturePerTakeVoidResult InResult)
	{
		if (!InResult.Result.bIsValid)
		{
			UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to import a take(%d): %s"), InResult.TakeId, *InResult.Result.Message);
		}
	}, EDelegateExecutionThread::InternalThread);

	TPromise<TArray<FMetaHumanTake>> Promise;
	TFuture<TArray<FMetaHumanTake>> Future = Promise.GetFuture();

	FDelegateHandle Handle = Ingester->OnGetTakesFinishedDelegate.AddLambda([&Promise](const TArray<FMetaHumanTake>& InTakes)
	{
		Promise.SetValue(InTakes);
	});

	bool bHasStarted = Ingester->GetTakes(InTakeIdList, MoveTemp(PerTakeCallback));

	if (!bHasStarted)
	{
		Promise.SetValue(TArray<FMetaHumanTake>());
	}

	Future.Wait();

	Ingester->OnGetTakesFinishedDelegate.Remove(MoveTemp(Handle));

	return Future.Get();
}

#if WITH_EDITOR

#define SET_TRANSATIONAL_PROPERTY(Property, PropertyEvent) if (GET_MEMBER_NAME_CHECKED(UMetaHumanCaptureSourceSync, Property) == PropertyEvent.GetPropertyName()) { MetaHumanCaptureSource->Property = Property; }

void UMetaHumanCaptureSourceSync::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	using namespace UE::MetaHuman;

	Super::PostEditChangeProperty(InPropertyChangedEvent);

	verify(MetaHumanCaptureSource->GetClass()->FindPropertyByName(InPropertyChangedEvent.GetPropertyName()));

	SET_TRANSATIONAL_PROPERTY(CaptureSourceType, InPropertyChangedEvent);
	SET_TRANSATIONAL_PROPERTY(StoragePath, InPropertyChangedEvent);
	SET_TRANSATIONAL_PROPERTY(DeviceIpAddress, InPropertyChangedEvent);
	SET_TRANSATIONAL_PROPERTY(DeviceControlPort, InPropertyChangedEvent);
	SET_TRANSATIONAL_PROPERTY(ShouldCompressDepthFiles, InPropertyChangedEvent);
	SET_TRANSATIONAL_PROPERTY(CopyImagesToProject, InPropertyChangedEvent);
	SET_TRANSATIONAL_PROPERTY(MinDistance, InPropertyChangedEvent);
	SET_TRANSATIONAL_PROPERTY(MaxDistance, InPropertyChangedEvent);
	SET_TRANSATIONAL_PROPERTY(DepthPrecision, InPropertyChangedEvent);
	SET_TRANSATIONAL_PROPERTY(DepthResolution, InPropertyChangedEvent);

	Ingester->SetParams(
		FIngesterParams(
			MetaHumanCaptureSource->CaptureSourceType,
			MetaHumanCaptureSource->StoragePath,
			MetaHumanCaptureSource->DeviceIpAddress,
			MetaHumanCaptureSource->DeviceControlPort,
			MetaHumanCaptureSource->ShouldCompressDepthFiles,
			MetaHumanCaptureSource->CopyImagesToProject,
			MetaHumanCaptureSource->MinDistance,
			MetaHumanCaptureSource->MaxDistance,
			MetaHumanCaptureSource->DepthPrecision,
			MetaHumanCaptureSource->DepthResolution
		)
	);
}

#undef SET_TRANSATIONAL_PROPERTY

#endif