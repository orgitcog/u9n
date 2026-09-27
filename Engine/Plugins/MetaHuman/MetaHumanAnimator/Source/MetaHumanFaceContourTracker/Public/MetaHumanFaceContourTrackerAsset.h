// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeGPU.h"

#include "MetaHumanFaceContourTrackerAsset.generated.h"

/** Face Contour Tracker Asset
* 
*   Contains trackers for different facial features
*   Used in MetaHuman Identity and Performance assets
* 
**/
UCLASS(BlueprintType)
class METAHUMANFACECONTOURTRACKER_API UMetaHumanFaceContourTrackerAsset : public UObject
{
	GENERATED_BODY()

public:

	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& InTransactionEvent) override;
#endif
	virtual void PostLoad() override;
	//~End UObject interface

	TSharedPtr<UE::NNE::IModelInstanceGPU> FaceDetector;
	TSharedPtr<UE::NNE::IModelInstanceGPU> FullFaceTracker;
	TSharedPtr<UE::NNE::IModelInstanceGPU> BrowsDenseTracker;
	TSharedPtr<UE::NNE::IModelInstanceGPU> EyesDenseTracker;
	TSharedPtr<UE::NNE::IModelInstanceGPU> NasioLabialsDenseTracker;
	TSharedPtr<UE::NNE::IModelInstanceGPU> MouthDenseTracker;
	TSharedPtr<UE::NNE::IModelInstanceGPU> LipzipDenseTracker;
	TSharedPtr<UE::NNE::IModelInstanceGPU> ChinDenseTracker;
	TSharedPtr<UE::NNE::IModelInstanceGPU> TeethDenseTracker;
	TSharedPtr<UE::NNE::IModelInstanceGPU> TeethConfidenceTracker;

public:

	UPROPERTY(EditAnywhere, Category = TrackerModels)
	TSoftObjectPtr<UNNEModelData> FaceDetectorModelData;

	UPROPERTY(EditAnywhere, Category = TrackerModels)
	TSoftObjectPtr<UNNEModelData> FullFaceTrackerModelData;

	UPROPERTY(EditAnywhere, Category = TrackerModels)
	TSoftObjectPtr<UNNEModelData> BrowsDenseTrackerModelData;

	UPROPERTY(EditAnywhere, Category = TrackerModels)
	TSoftObjectPtr<UNNEModelData> EyesDenseTrackerModelData;

	UPROPERTY(EditAnywhere, Category = TrackerModels)
	TSoftObjectPtr<UNNEModelData> NasioLabialsDenseTrackerModelData;

	UPROPERTY(EditAnywhere, Category = TrackerModels)
	TSoftObjectPtr<UNNEModelData> MouthDenseTrackerModelData;

	UPROPERTY(EditAnywhere, Category = TrackerModels)
	TSoftObjectPtr<UNNEModelData> LipzipDenseTrackerModelData;

	UPROPERTY(EditAnywhere, Category = TrackerModels)
	TSoftObjectPtr<UNNEModelData> ChinDenseTrackerModelData;

	UPROPERTY(EditAnywhere, Category = TrackerModels)
	TSoftObjectPtr<UNNEModelData> TeethDenseTrackerModelData;

	UPROPERTY(EditAnywhere, Category = TrackerModels)
	TSoftObjectPtr<UNNEModelData> TeethConfidenceTrackerModelData;

public:

	bool CanProcess() const;

	void LoadTrackers(bool bInShowProgressNotification, TFunction<void(bool)>&& Callback);

	void CancelLoadTrackers();

	bool LoadTrackersSynchronous();

	bool IsLoadingTrackers() const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNNEModelData>> LoadedTrackerModelData;

	TArray<TSharedPtr<UE::NNE::IModelInstanceGPU>> LoadedTrackerModels;

	TWeakPtr<class SNotificationItem> LoadNotification;
	TSharedPtr<struct FStreamableHandle> TrackersLoadHandle;

	TArray<TSoftObjectPtr<UNNEModelData>> GetTrackerModelData() const;
	TArray<TSharedPtr<UE::NNE::IModelInstanceGPU>> GetTrackerModels() const;
	bool SetTrackerModels();

	TArray<FSoftObjectPath> GetTrackerModelDataAsSoftObjectPaths() const;
	bool AreTrackerModelsLoaded() const;

	bool CreateTrackerModels();
};
