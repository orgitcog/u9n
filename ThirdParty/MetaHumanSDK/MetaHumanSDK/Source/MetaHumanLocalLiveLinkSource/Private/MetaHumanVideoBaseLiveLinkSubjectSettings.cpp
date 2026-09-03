// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanVideoBaseLiveLinkSubjectSettings.h"

#include "MetaHumanVideoBaseLiveLinkSubject.h"
#include "MetaHumanVideoLiveLinkSettings.h"



UMetaHumanVideoBaseLiveLinkSubjectSettings::UMetaHumanVideoBaseLiveLinkSubjectSettings()
{
	const UMetaHumanVideoLiveLinkSettings* DefaultSettings = GetDefault<UMetaHumanVideoLiveLinkSettings>();

	bHeadOrientation = DefaultSettings->bHeadOrientation;
	bHeadTranslation = DefaultSettings->bHeadTranslation;
	MonitorImage = DefaultSettings->MonitorImage;
}

#if WITH_EDITOR
void UMetaHumanVideoBaseLiveLinkSubjectSettings::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	if (FProperty* Property = InPropertyChangedEvent.Property)
	{
		const FName PropertyName = *Property->GetName();

		const bool bHeadOrientationChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bHeadOrientation);
		const bool bHeadTranslationChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bHeadTranslation);
		const bool bHeadStabilizationChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bHeadStabilization);
		const bool bMonitorImageChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, MonitorImage);
		const bool bRotationChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, Rotation);

		FMetaHumanVideoBaseLiveLinkSubject* VideoSubject = (FMetaHumanVideoBaseLiveLinkSubject*) Subject;

		if (bHeadOrientationChanged)
		{
			VideoSubject->SetHeadOrientation(bHeadOrientation);
		}
		else if (bHeadTranslationChanged)
		{
			VideoSubject->SetHeadTranslation(bHeadTranslation);
		}
		else if (bHeadStabilizationChanged)
		{
			VideoSubject->SetHeadStabilization(bHeadStabilization);
		}
		else if (bMonitorImageChanged)
		{
			VideoSubject->SetMonitorImage(MonitorImage);
		}
		else if (bRotationChanged)
		{
			VideoSubject->SetRotation(Rotation);
		}
	}
}
#endif

void UMetaHumanVideoBaseLiveLinkSubjectSettings::CaptureNeutralHeadTranslation()
{
	FMetaHumanVideoBaseLiveLinkSubject* VideoSubject = (FMetaHumanVideoBaseLiveLinkSubject*) Subject;
	VideoSubject->MarkNeutralFrame();

	Super::CaptureNeutralHeadTranslation();
}
