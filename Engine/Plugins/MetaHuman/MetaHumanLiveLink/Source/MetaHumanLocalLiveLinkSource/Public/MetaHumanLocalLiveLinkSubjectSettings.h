// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanLiveLinkSubjectSettings.h"
#include "MetaHumanMediaSourceCreateParams.h"

#include "Pipeline/PipelineData.h"

#include "MetaHumanLocalLiveLinkSubjectSettings.generated.h"



UCLASS()
class METAHUMANLOCALLIVELINKSOURCE_API UMetaHumanLocalLiveLinkSubjectSettings : public UMetaHumanLiveLinkSubjectSettings
{
public:

	GENERATED_BODY()

	virtual void Setup();
	virtual void SetSubject(class FMetaHumanLocalLiveLinkSubject* InSubject);

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUpdate, TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);
	FOnUpdate UpdateDelegate;

	class FMetaHumanLocalLiveLinkSubject* Subject = nullptr;

	/* The state of the processing. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Information", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides, DisplayPriority = "10"))
	FString State;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Information", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides, DisplayPriority = "15"))
	FColor StateLED;

	/* Frame number being processed. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Information", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides, DisplayPriority = "100"))
	FString Frame;

	/* Processing frame rate. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Information", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides, DisplayPriority = "110"))
	FString FPS;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Information", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides, DisplayPriority = "120"))
	FString Timecode;

	UPROPERTY(Transient, VisibleAnywhere, Category = "nocategory", meta = (EditCondition = "bIsLiveProcessing", HideEditConditionToggle, EditConditionHides))
	FString Remove;
};