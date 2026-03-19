// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanLocalLiveLinkSubject.h"
#include "MetaHumanLocalLiveLinkSourceSettings.h"
#include "MetaHumanMediaSourceCreateParams.h"

#include "ILiveLinkSource.h"

METAHUMANLOCALLIVELINKSOURCE_API DECLARE_LOG_CATEGORY_EXTERN(LogMetaHumanLocalLiveLinkSource, Log, All);



class METAHUMANLOCALLIVELINKSOURCE_API FMetaHumanLocalLiveLinkSource : public ILiveLinkSource, public TSharedFromThis<FMetaHumanLocalLiveLinkSource>
{
public:

	// ~ILiveLinkSource Interface

	virtual ~FMetaHumanLocalLiveLinkSource() override;
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	virtual bool IsSourceStillValid() const override;
	virtual bool RequestSourceShutdown() override;
	virtual FText GetSourceMachineName() const override;
	virtual FText GetSourceStatus() const override;
	virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override;
	virtual void InitializeSettings(ULiveLinkSourceSettings* InSettings) override;

	// ~ILiveLinkSource Interface

	template<class T>
	T* CreateSubjectSettings()
	{
		T* SubjectSettings = NewObject<T>(GetTransientPackage());
		SubjectSettings->Setup();

		return SubjectSettings;
	}

	void RequestSubjectCreation(const FString& InSubjectName, UMetaHumanLocalLiveLinkSubjectSettings* InMetaHumanLocalLiveLinkSubjectSettings);

protected:

	/** The Live Link client used to push Live Link data to the editor */
	ILiveLinkClient* LiveLinkClient = nullptr;

	/** The GUID of the Live Link Source */
	FGuid SourceGuid;

	virtual void OnSourceCreated(bool bIsPreset) { }

	virtual TSharedPtr<FMetaHumanLocalLiveLinkSubject> CreateSubject(const FName& InSubjectName, UMetaHumanLocalLiveLinkSubjectSettings* InSettings) = 0;

private:

	void SubjectAdded(FLiveLinkSubjectKey InSubject);
	void SubjectRemoved(FLiveLinkSubjectKey InSubject);

	TMap<FLiveLinkSubjectKey, TSharedPtr<FMetaHumanLocalLiveLinkSubject>> Subjects;

	bool bIsActive = false;

	UMetaHumanLocalLiveLinkSourceSettings* Settings = nullptr;
};
