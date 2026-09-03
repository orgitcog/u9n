// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanMediaSourceCustomization.h"
#include "MetaHumanVideoLiveLinkSourceCustomization.h"
#include "MetaHumanLocalLiveLinkSourceSettings.h"
#include "MetaHumanVideoLiveLinkSubjectSettings.h"
#include "MetaHumanAudioLiveLinkSubjectSettings.h"
#include "MetaHumanMediaSourceWidget.h"
#include "MetaHumanLiveLinkSourceStyle.h"

#include "UObject/Package.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "MetaHumanMediaSourceCustomization"



void FMetaHumanMediaSourceCustomization::Setup(IDetailLayoutBuilder& InDetailBuilder, bool bInIsVideo)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	InDetailBuilder.GetObjectsBeingCustomized(Objects);

	check(Objects.Num() == 1);
	UMetaHumanLocalLiveLinkSourceSettings* Settings = Cast<UMetaHumanLocalLiveLinkSourceSettings>(Objects[0]);

	// Return if some derived class of the settings has already applied customization
	TArray<FName> CategoryNames;
	InDetailBuilder.GetCategoryNames(CategoryNames);
	if (CategoryNames.Contains("Create"))
	{
		return;
	}

	MediaSource = SNew(SMetaHumanMediaSourceWidget, bInIsVideo ? SMetaHumanMediaSourceWidget::EMediaType::Video : SMetaHumanMediaSourceWidget::EMediaType::Audio);

	SubjectName = SNew(SEditableTextBox).Font(IDetailLayoutBuilder::GetDetailFont());

	ButtonTextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("ButtonText");
	ButtonTextStyle.SetFont(IDetailLayoutBuilder::GetDetailFont());

	constexpr bool bIsAdvanced = true;

	IDetailCategoryBuilder& CreateCategory = InDetailBuilder.EditCategory("Create", LOCTEXT("Create", "Create"), ECategoryPriority::Important);

	TSharedPtr<SHorizontalBox> DeviceWidget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			MediaSource->GetWidget(bInIsVideo ? SMetaHumanMediaSourceWidget::EWidgetType::VideoDevice : SMetaHumanMediaSourceWidget::EWidgetType::AudioDevice).ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RefreshTooltip", "Refresh the list of devices"))
			.OnClicked_Lambda([this]()
			{
				MediaSource->Repopulate();

				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(FMetaHumanLiveLinkSourceStyle::Get().GetBrush("Refresh"))
			]
		];

	if (bInIsVideo)
	{
		AddRow(CreateCategory, LOCTEXT("VideoDevice", "Video Device"), DeviceWidget);
		AddRow(CreateCategory, LOCTEXT("VideoTrack", "Video Track"), MediaSource->GetWidget(SMetaHumanMediaSourceWidget::EWidgetType::VideoTrack), bIsAdvanced);
		AddRow(CreateCategory, LOCTEXT("VideoTrackFormat", "Video Format"), MediaSource->GetWidget(SMetaHumanMediaSourceWidget::EWidgetType::VideoTrackFormat));
	}
	else
	{
		AddRow(CreateCategory, LOCTEXT("AudioDevice", "Audio Device"), DeviceWidget);
		AddRow(CreateCategory, LOCTEXT("AudioTrack", "Audio Track"), MediaSource->GetWidget(SMetaHumanMediaSourceWidget::EWidgetType::AudioTrack), bIsAdvanced);
		AddRow(CreateCategory, LOCTEXT("AudioTrackFormat", "Audio Format"), MediaSource->GetWidget(SMetaHumanMediaSourceWidget::EWidgetType::AudioTrackFormat));
	}

	AddRow(CreateCategory, LOCTEXT("Filtered", "Filter Format List"), MediaSource->GetWidget(SMetaHumanMediaSourceWidget::EWidgetType::Filtered), bIsAdvanced,
		LOCTEXT("FilteredTooltip", "Filter the formats to show only the most relevant ones"));
	AddRow(CreateCategory, LOCTEXT("StartTimeout", "Start Timeout"), MediaSource->GetWidget(SMetaHumanMediaSourceWidget::EWidgetType::StartTimeout), bIsAdvanced,
		LOCTEXT("StartTimeoutTooltip", "Timeout for waiting for media to open"));
	AddRow(CreateCategory, LOCTEXT("FormatWaitTime", "Format Wait Time"), MediaSource->GetWidget(SMetaHumanMediaSourceWidget::EWidgetType::FormatWaitTime), bIsAdvanced,
		LOCTEXT("FormatWaitTimeTooltip", "Time to wait for format changes to take effect"));
	AddRow(CreateCategory, LOCTEXT("SampleTimeout", "Sample Timeout"), MediaSource->GetWidget(SMetaHumanMediaSourceWidget::EWidgetType::SampleTimeout), bIsAdvanced,
		LOCTEXT("SampleTimeoutTooltip", "Timeout for waiting on first media sample to arrive"));

	IDetailCategoryBuilder& NoCategory = InDetailBuilder.EditCategory("nocategory", LOCTEXT("nocategory", "nocategory"), ECategoryPriority::Important);

	AddRow(NoCategory, LOCTEXT("SubjectName", "Subject Name"), SubjectName);

	NoCategory.AddCustomRow(LOCTEXT("Connect", "Connect"))
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.Text(LOCTEXT("Connect", "Connect"))
				.TextStyle(&ButtonTextStyle)
				.HAlign(HAlign_Center)
				.IsEnabled_Lambda([this]()
				{
					return MediaSource->CanCreate();
				})
				.OnClicked_Lambda([this, Settings, bInIsVideo]()
				{
					UMetaHumanLocalLiveLinkSubjectSettings* SubjectSettings = nullptr;
					FString DeviceName;

					if (bInIsVideo)
					{ 
						UMetaHumanVideoLiveLinkSubjectSettings* VideoSubjectSettings = NewObject<UMetaHumanVideoLiveLinkSubjectSettings>(GetTransientPackage());
						VideoSubjectSettings->MediaSourceCreateParams = MediaSource->GetCreateParams();
						SubjectSettings = VideoSubjectSettings;
						DeviceName = VideoSubjectSettings->MediaSourceCreateParams.VideoName;
					}
					else
					{
						UMetaHumanAudioLiveLinkSubjectSettings* AudioSubjectSettings = NewObject<UMetaHumanAudioLiveLinkSubjectSettings>(GetTransientPackage());
						AudioSubjectSettings->MediaSourceCreateParams = MediaSource->GetCreateParams();
						SubjectSettings = AudioSubjectSettings;
						DeviceName = AudioSubjectSettings->MediaSourceCreateParams.AudioName;
					}

					SubjectSettings->Setup();

					Settings->RequestSubjectCreation(SubjectName->GetText().IsEmpty() ? DeviceName : SubjectName->GetText().ToString(), SubjectSettings);

					return FReply::Handled();
				})
			]
		];
}

void FMetaHumanMediaSourceCustomization::AddRow(IDetailCategoryBuilder& InCategoryBuilder, FText InText, TSharedPtr<SWidget> InWidget, bool bInIsAdvanced, FText InToolTip)
{
	InCategoryBuilder.AddCustomRow(InText, bInIsAdvanced)
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(InToolTip)
			.Text(InText)
		]
		.ValueContent()
		[
			InWidget.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE
