// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanFaceContourTrackerAsset.h"
#include "MetaHumanAuthoringObjects.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "NNE.h"
#include "NNEModelData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanFaceContourTrackerAsset)

#define LOCTEXT_NAMESPACE "FaceContourTracker"

#if WITH_EDITOR
void UMetaHumanFaceContourTrackerAsset::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	LoadedTrackerModels.Reset();
}

void UMetaHumanFaceContourTrackerAsset::UMetaHumanFaceContourTrackerAsset::PostTransacted(const FTransactionObjectEvent& InTransactionEvent)
{
	Super::PostTransacted(InTransactionEvent);

	LoadedTrackerModels.Reset();
}
#endif

void UMetaHumanFaceContourTrackerAsset::PostLoad()
{
	Super::PostLoad();

	// Find the tracking model objects. These could be in the plugin specified by the TSoftObjectPtr
	// path that was loaded, but they could also be in a different plugin. Update TSoftObjectPtr path
	// accordingly. This supports freely moving these large tracking models between plugins, ie 
	// from /MetaHuman/GenericTracker/Chin to /MetaHumanAuthoring/GenericTracker/Chin
	FMetaHumanAuthoringObjects::FindObject<UNNEModelData>(FaceDetectorModelData);
	FMetaHumanAuthoringObjects::FindObject<UNNEModelData>(FullFaceTrackerModelData);
	FMetaHumanAuthoringObjects::FindObject<UNNEModelData>(BrowsDenseTrackerModelData);
	FMetaHumanAuthoringObjects::FindObject<UNNEModelData>(EyesDenseTrackerModelData);
	FMetaHumanAuthoringObjects::FindObject<UNNEModelData>(MouthDenseTrackerModelData);
	FMetaHumanAuthoringObjects::FindObject<UNNEModelData>(LipzipDenseTrackerModelData);
	FMetaHumanAuthoringObjects::FindObject<UNNEModelData>(NasioLabialsDenseTrackerModelData);
	FMetaHumanAuthoringObjects::FindObject<UNNEModelData>(ChinDenseTrackerModelData);
	FMetaHumanAuthoringObjects::FindObject<UNNEModelData>(TeethDenseTrackerModelData);
	FMetaHumanAuthoringObjects::FindObject<UNNEModelData>(TeethConfidenceTrackerModelData);
}

TArray<TSoftObjectPtr<UNNEModelData>> UMetaHumanFaceContourTrackerAsset::GetTrackerModelData() const
{
	return
	{
		FaceDetectorModelData,
		FullFaceTrackerModelData,
		BrowsDenseTrackerModelData,
		EyesDenseTrackerModelData,
		MouthDenseTrackerModelData,
		LipzipDenseTrackerModelData,
		NasioLabialsDenseTrackerModelData,
		ChinDenseTrackerModelData,
		TeethDenseTrackerModelData,
		TeethConfidenceTrackerModelData
	};
}

TArray<TSharedPtr<UE::NNE::IModelInstanceGPU>> UMetaHumanFaceContourTrackerAsset::GetTrackerModels() const
{
	return
	{
		FaceDetector,
		FullFaceTracker,
		BrowsDenseTracker,
		EyesDenseTracker,
		MouthDenseTracker,
		LipzipDenseTracker,
		NasioLabialsDenseTracker,
		ChinDenseTracker,
		TeethDenseTracker,
		TeethConfidenceTracker
	};
}

bool UMetaHumanFaceContourTrackerAsset::SetTrackerModels()
{
	if (LoadedTrackerModels.Num() != GetTrackerModels().Num())
	{
		return false;
	}

	FaceDetector = LoadedTrackerModels[0];
	FullFaceTracker = LoadedTrackerModels[1];
	BrowsDenseTracker = LoadedTrackerModels[2];
	EyesDenseTracker = LoadedTrackerModels[3];
	MouthDenseTracker = LoadedTrackerModels[4];
	LipzipDenseTracker = LoadedTrackerModels[5];
	NasioLabialsDenseTracker = LoadedTrackerModels[6];
	ChinDenseTracker = LoadedTrackerModels[7];
	TeethDenseTracker = LoadedTrackerModels[8];
	TeethConfidenceTracker = LoadedTrackerModels[9];

	return true;
}

TArray<FSoftObjectPath> UMetaHumanFaceContourTrackerAsset::GetTrackerModelDataAsSoftObjectPaths() const
{
	TArray<TSoftObjectPtr<UNNEModelData>> TrackerModelData = GetTrackerModelData();
	TArray<FSoftObjectPath> TrackerModelDataSoftObjectPaths;
	TrackerModelDataSoftObjectPaths.Reserve(TrackerModelData.Num());

	for (const TSoftObjectPtr<UNNEModelData>& ModelData : TrackerModelData)
	{
		TrackerModelDataSoftObjectPaths.Emplace(ModelData.ToSoftObjectPath());
	}

	return TrackerModelDataSoftObjectPaths;
}

bool UMetaHumanFaceContourTrackerAsset::CanProcess() const
{
	// TODO we want to add more validation here that the NNE models have the right number of outputs if possible
	// but needs extra functionality adding to the Pipeline HyprSense node
	for (const TSoftObjectPtr<UNNEModelData>& ModelData : GetTrackerModelData())
	{
		if (ModelData.IsNull())
		{
			return false;
		}
	}

	// we don't need to check the tracker models are valid, just the tracker model data

	return true;
}

void UMetaHumanFaceContourTrackerAsset::LoadTrackers(bool bInShowProgressNotification, TFunction<void(bool)>&& Callback)
{
	// Show a progress indicator if requested.
	if (bInShowProgressNotification)
	{
		// Only show if the trackers aren't loaded already.
		if (!AreTrackerModelsLoaded())
		{
			FNotificationInfo Info(LOCTEXT("LoadTrackersNotification", "Loading trackers..."));
			Info.bFireAndForget = false;
			LoadNotification = FSlateNotificationManager::Get().AddNotification(Info);
			if (LoadNotification.IsValid())
			{
				LoadNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
			}
		}
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	TrackersLoadHandle = StreamableManager.RequestAsyncLoad(GetTrackerModelDataAsSoftObjectPaths(), [this, Callback]()
	{
		bool bLoadSucceeded = true;

		for (const TSoftObjectPtr<UNNEModelData>& ModelData : GetTrackerModelData())
		{
			if (ModelData.IsValid())
			{
				LoadedTrackerModelData.Add(ModelData.Get());
			}
			else
			{
				bLoadSucceeded = false;
			}
		}

		bLoadSucceeded &= CreateTrackerModels();

		if (LoadNotification.IsValid())
		{
			LoadNotification.Pin()->SetCompletionState(SNotificationItem::CS_None);
			LoadNotification.Pin()->ExpireAndFadeout();
		}

		Callback(bLoadSucceeded);
	});
}

void UMetaHumanFaceContourTrackerAsset::CancelLoadTrackers()
{
	if (TrackersLoadHandle.IsValid())
	{
		TrackersLoadHandle->CancelHandle();
	}

	if (LoadNotification.IsValid() && LoadNotification.Pin().IsValid())
	{
		LoadNotification.Pin()->SetCompletionState(SNotificationItem::CS_Fail);
		LoadNotification.Pin()->ExpireAndFadeout();
	}
}

bool UMetaHumanFaceContourTrackerAsset::LoadTrackersSynchronous()
{
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	StreamableManager.RequestSyncLoad(GetTrackerModelDataAsSoftObjectPaths());
	bool bLoadSucceeded = true;

	for (const TSoftObjectPtr<UNNEModelData>& ModelData : GetTrackerModelData())
	{
		if (ModelData.IsValid())
		{
			LoadedTrackerModelData.Add(ModelData.Get());
		}
		else
		{
			bLoadSucceeded = false;
		}
	}

	bLoadSucceeded &= CreateTrackerModels();

	return bLoadSucceeded;
}

bool UMetaHumanFaceContourTrackerAsset::AreTrackerModelsLoaded() const
{
	TArray<TSoftObjectPtr<UNNEModelData>> ModelData = GetTrackerModelData();
	TArray<TSharedPtr<UE::NNE::IModelInstanceGPU>> Models = GetTrackerModels();
	for (int i = 0; i < ModelData.Num(); i++)
	{
		if (!ModelData[i].IsNull() && (!ModelData[i].IsValid() || !Models[i].IsValid()))
		{
			return false;
		}
	}

	return true;
}

bool UMetaHumanFaceContourTrackerAsset::IsLoadingTrackers() const
{
	return TrackersLoadHandle.IsValid() && TrackersLoadHandle->IsLoadingInProgress();
}

bool UMetaHumanFaceContourTrackerAsset::CreateTrackerModels()
{
	if (LoadedTrackerModels.IsEmpty())
	{
		using namespace UE::NNE;

		TWeakInterfacePtr<INNERuntimeGPU> Runtime = GetRuntime<INNERuntimeGPU>("NNERuntimeORTDml");
		if (!Runtime.IsValid())
		{
			return false;
		}

		TArray<TSoftObjectPtr<UNNEModelData>> ModelDataArray = GetTrackerModelData();
		LoadedTrackerModels.Empty();
		LoadedTrackerModels.Reserve(ModelDataArray.Num());
		for (const TSoftObjectPtr<UNNEModelData>& ModelData : ModelDataArray)
		{
			if (!ModelData.IsNull() && ModelData.IsValid())
			{
				TSharedPtr<IModelGPU> UniqueModel = Runtime->CreateModelGPU(ModelData.Get());
				if (!UniqueModel.IsValid())
				{
					return false;
				}
				LoadedTrackerModels.Emplace(TSharedPtr<IModelInstanceGPU>(UniqueModel->CreateModelInstanceGPU()));
			}
		}
	}

	return SetTrackerModels();
}

#undef LOCTEXT_NAMESPACE

