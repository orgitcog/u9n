// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkSource.h"
#include "Templates/UnrealTemplate.h"
#include "VCamPixelStreamingLiveLink.generated.h"

UCLASS()
class UPixelStreamingLiveLinkSourceSettings : public ULiveLinkSourceSettings
{
public:
	GENERATED_BODY()

	UPixelStreamingLiveLinkSourceSettings();
};

class FPixelStreamingLiveLinkSource : public ILiveLinkSource, public FNoncopyable
{
public:
	
	FPixelStreamingLiveLinkSource();
	virtual ~FPixelStreamingLiveLinkSource() override;
	
	//~ Begin ILiveLinkSource Interface
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	virtual void Update() override;
	virtual bool CanBeDisplayedInUI() const override;
	virtual bool IsSourceStillValid() const override;
	virtual bool RequestSourceShutdown() override;
	virtual FText GetSourceType() const override;
	virtual FText GetSourceMachineName() const override;
	virtual FText GetSourceStatus() const override;
	// Override the settings class to allow us to customize the default values
	virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override { return UPixelStreamingLiveLinkSourceSettings::StaticClass(); }
	//~ End ILiveLinkSource Interface

	// Registers a new subject with the Transform Role to the Live Link Client
	// If called with a subject name that already exists in this source then this will reset any buffered data for that subject
	void CreateSubject(FName SubjectName);
	void RemoveSubject(FName SubjectName);
	void PushTransformForSubject(FName SubjectName, FTransform Transform);
	void PushTransformForSubject(FName SubjectName, FTransform Transform, double Timestamp);

private:
	
	/** Cached information for communicating with the live link client. */
	ILiveLinkClient* LiveLinkClient;
	FGuid SourceGuid;
	uint64 NTransformsPushed = 0;
	uint64 LastTransformGraphedCycles = 0.0f;

	/**
	 * Subjects to create at the end of the frame.
	 *
	 * CreateSubject uses PushSubjectStaticData_AnyThread, which is processed latently by LiveLink; once processed, the subject appears in the UI.
	 * RemoveSubject uses RemoveSubject_AnyThread has, which an immediate effect.
	 * If PushSubjectStaticData_AnyThread is immediately followed by RemoveSubject_AnyThread, which happens if CreateSubject and RemoveSubject are
	 * called in the same frame, the subject technically does not exist internally in LiveLink, yet.
	 * 
	 * To solve this, we check at the end of the frame whether the API user wants a subject to exist this frame or not.
	 */
	TArray<FName> PendingSubjectsToCreate;
	
	void OnEndOfFrame() { ProcessEndOfFrameSubjectCreation(); }
	void ProcessEndOfFrameSubjectCreation();
};
