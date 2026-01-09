// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanLocalLiveLinkSource.h"

#include "UObject/Package.h"

DEFINE_LOG_CATEGORY(LogMetaHumanLocalLiveLinkSource);

#define LOCTEXT_NAMESPACE "MetaHumanLocalLiveLinkSource"



FMetaHumanLocalLiveLinkSource::~FMetaHumanLocalLiveLinkSource()
{
	UE_LOG(LogMetaHumanLocalLiveLinkSource, Verbose, TEXT("Destroying Source"));
}

void FMetaHumanLocalLiveLinkSource::ReceiveClient(ILiveLinkClient* InLiveLinkClient, FGuid InSourceGuid)
{
	UE_LOG(LogMetaHumanLocalLiveLinkSource, Verbose, TEXT("Creating Source"));

	LiveLinkClient = InLiveLinkClient;
	SourceGuid = InSourceGuid;

	LiveLinkClient->OnLiveLinkSubjectAdded().AddSP(this, &FMetaHumanLocalLiveLinkSource::SubjectAdded);
	LiveLinkClient->OnLiveLinkSubjectRemoved().AddSP(this, &FMetaHumanLocalLiveLinkSource::SubjectRemoved);

	bIsActive = true;
}

bool FMetaHumanLocalLiveLinkSource::IsSourceStillValid() const
{
	return bIsActive;
}

bool FMetaHumanLocalLiveLinkSource::RequestSourceShutdown()
{
	bIsActive = false;

	for (const TPair<FLiveLinkSubjectKey, TSharedPtr<FMetaHumanLocalLiveLinkSubject>>& SubjectPair : Subjects)
	{
		SubjectPair.Value->Stop();
	}

	return true;
}

FText FMetaHumanLocalLiveLinkSource::GetSourceMachineName() const
{
	return FText::FromString(FPlatformProcess::ComputerName());
}

FText FMetaHumanLocalLiveLinkSource::GetSourceStatus() const
{
	return bIsActive ? LOCTEXT("ActiveSourceStatus", "Active") : LOCTEXT("InactiveSourceStatus", "Inactive");
}

TSubclassOf<ULiveLinkSourceSettings> FMetaHumanLocalLiveLinkSource::GetSettingsClass() const
{
	return UMetaHumanLocalLiveLinkSourceSettings::StaticClass();
}

void FMetaHumanLocalLiveLinkSource::InitializeSettings(ULiveLinkSourceSettings* InSettings)
{
	Settings = Cast<UMetaHumanLocalLiveLinkSourceSettings>(InSettings);
	Settings->SetSource(this);

	OnSourceCreated(Settings->bIsPreset);

	Settings->bIsPreset = true;
}

void FMetaHumanLocalLiveLinkSource::RequestSubjectCreation(const FString& InSubjectName, UMetaHumanLocalLiveLinkSubjectSettings* InMetaHumanLocalLiveLinkSubjectSettings)
{
	FLiveLinkSubjectKey SubjectKey(SourceGuid, FName(InSubjectName));

	FLiveLinkSubjectPreset Preset;
	Preset.Key = SubjectKey;
	Preset.Role = InMetaHumanLocalLiveLinkSubjectSettings->Role;
	Preset.Settings = InMetaHumanLocalLiveLinkSubjectSettings;
	Preset.bEnabled = true;

	if (!LiveLinkClient->CreateSubject(Preset))
	{
		UE_LOG(LogMetaHumanLocalLiveLinkSource, Warning, TEXT("Failed to create subject"));
	}
}

void FMetaHumanLocalLiveLinkSource::SubjectAdded(FLiveLinkSubjectKey InSubject)
{
	if (InSubject.Source == SourceGuid)
	{
		UE_LOG(LogMetaHumanLocalLiveLinkSource, Display, TEXT("Created subject \"%s\""), *InSubject.SubjectName.ToString());

		UMetaHumanLocalLiveLinkSubjectSettings* SubjectSettings = Cast<UMetaHumanLocalLiveLinkSubjectSettings>(LiveLinkClient->GetSubjectSettings(InSubject));

		TSharedPtr<FMetaHumanLocalLiveLinkSubject> Subject = CreateSubject(InSubject.SubjectName, SubjectSettings);

		SubjectSettings->SetSubject(Subject.Get());

		Subject->Start();

		Subjects.Add(InSubject, Subject);
	}
}

void FMetaHumanLocalLiveLinkSource::SubjectRemoved(FLiveLinkSubjectKey InSubject)
{
	// LLH config reloads require careful handling. A config reload causes the old source/subject
	// to be deleted, calling this function, but only after the new source is created which
	// reuses the same GUID as the old source. So, checking source guid is not enough to determine
	// if this instance of the source is the one that manages the subject. 
	// 
	// Instead also check if the subject truly no longer exists. This will be the case when the subject
	// (but not the source) has been deleted which is what needs handling here. If however the subject
	// does exist (ie it was recreated when the config reloaded) then this instance of the source is not
	// the one that created the subject in the first place. No handling needed here for that case.

	if (InSubject.Source == SourceGuid && !LiveLinkClient->GetSubjects(true, true).Contains(InSubject))
	{
		check(Subjects.Contains(InSubject));

		Subjects[InSubject]->Stop();
		Subjects.Remove(InSubject);

		UE_LOG(LogMetaHumanLocalLiveLinkSource, Display, TEXT("Removed subject \"%s\""), *InSubject.SubjectName.ToString());
	}
}

#undef LOCTEXT_NAMESPACE