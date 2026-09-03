// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAudioBaseLiveLinkSubjectCustomization.h"
#include "MetaHumanAudioBaseLiveLinkSubjectSettings.h"
#include "MetaHumanAudioBaseLiveLinkSubjectMonitorWidget.h"
#include "MetaHumanLocalLiveLinkSubjectMonitorWidget.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "MetaHumanAudioBaseLiveLinkSource"



TSharedRef<IDetailCustomization> FMetaHumanAudioBaseLiveLinkSubjectCustomization::MakeInstance()
{
	return MakeShared<FMetaHumanAudioBaseLiveLinkSubjectCustomization>();
}

void FMetaHumanAudioBaseLiveLinkSubjectCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	InDetailBuilder.GetObjectsBeingCustomized(Objects);

	check(Objects.Num() == 1);
	UMetaHumanAudioBaseLiveLinkSubjectSettings* Settings = Cast<UMetaHumanAudioBaseLiveLinkSubjectSettings>(Objects[0]);

	if (!Settings->bIsLiveProcessing)
	{
		return;
	}

	IDetailCategoryBuilder& MonitorCategory = InDetailBuilder.EditCategory("Audio", LOCTEXT("Audio", "Audio"), ECategoryPriority::Important);

	TSharedPtr<SMetaHumanLocalLiveLinkSubjectMonitorWidget> LocalLiveLinkSubjectMonitorWidget = SNew(SMetaHumanLocalLiveLinkSubjectMonitorWidget, Settings);

	TSharedRef<IPropertyHandle> LevelProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanAudioBaseLiveLinkSubjectSettings, Level));
	IDetailPropertyRow* LevelRow = InDetailBuilder.EditDefaultProperty(LevelProperty);
	check(LevelRow);

	TSharedPtr<SWidget> NameWidget, ValueWidget;
	LevelRow->GetDefaultWidgets(NameWidget, ValueWidget, false);

	LevelRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SMetaHumanAudioBaseLiveLinkSubjectMonitorWidget, Settings)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				LocalLiveLinkSubjectMonitorWidget.ToSharedRef()
			]
		];

	// Hide the unused calibration, smoothing and head translation
	IDetailCategoryBuilder& ControlsCategory = InDetailBuilder.EditCategory("Controls", LOCTEXT("Controls", "Controls"));
	ControlsCategory.SetCategoryVisibility(false);
}

#undef LOCTEXT_NAMESPACE
