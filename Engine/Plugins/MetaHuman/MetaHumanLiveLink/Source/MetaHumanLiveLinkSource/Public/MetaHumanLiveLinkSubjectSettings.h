// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkHubSubjectSettings.h"

#include "MetaHumanRealtimeCalibration.h"
#include "MetaHumanRealtimeSmoothing.h"

#include "MetaHumanLiveLinkSubjectSettings.generated.h"



UCLASS()
class METAHUMANLIVELINKSOURCE_API UMetaHumanLiveLinkSubjectSettings : public ULiveLinkHubSubjectSettings
{
public:

	GENERATED_BODY()

	UMetaHumanLiveLinkSubjectSettings();

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR

	// The bIsLiveProcessing flag will be set to true when the settings are being used by a subject 
	// that is producing live data. This is the typical case, eg VideoSubjectSettings being used 
	// by a VideoSubject class.
	// 
	// The bIsLiveProcessing flag will be set to false when the settings are being used by a subject 
	// that is playing back pre-recorded data. This will be the case when using Take Recorder.
	// In this case we should hide all controls that would attempt to change the Live Link data being
	// produced, eg head translation on/off, since these will not apply to pre-recorded data.
	UPROPERTY(Transient)
	bool bIsLiveProcessing = false;

	UPROPERTY(EditAnywhere, Category = "Controls", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	int32 CaptureNeutralsProperty = 0; // A dummy property thats customized to a button

	// Calibration
	UPROPERTY(EditAnywhere, Category = "Controls|Calibration", meta = (ToolTip = "The properties to calibrate.", EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	TArray<FName> Properties;

	UPROPERTY(EditAnywhere, Category = "Controls|Calibration", meta = (ClampMin = 0, ClampMax = 1, EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	float Alpha = 1.0;

	UPROPERTY(EditAnywhere, Category = "Controls|Calibration", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	TArray<float> NeutralFrame;

	UPROPERTY(EditAnywhere, Category = "Controls|Calibration", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	int32 CaptureNeutralFrameCountdown = -1;

	// Smoothing
	UPROPERTY(EditAnywhere, Category = "Controls|Smoothing", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	TObjectPtr<UMetaHumanRealtimeSmoothingParams> Parameters;

	// Head translation
	UPROPERTY(EditAnywhere, Category = "Controls|Head Translation", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	FVector NeutralHeadTranslation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Controls|Head Translation", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	int32 CaptureNeutralHeadTranslationCountdown = -1;

	virtual void CaptureNeutrals();
	virtual void CaptureNeutralFrame();
	virtual void CaptureNeutralHeadTranslation();

	bool PreProcess(const FLiveLinkBaseStaticData& InStaticData, FLiveLinkBaseFrameData& InOutFrameData);

private:

	TSharedPtr<FMetaHumanRealtimeCalibration> Calibration;

	TSharedPtr<FMetaHumanRealtimeSmoothing> Smoothing;
	double LastTime = 0;
};
