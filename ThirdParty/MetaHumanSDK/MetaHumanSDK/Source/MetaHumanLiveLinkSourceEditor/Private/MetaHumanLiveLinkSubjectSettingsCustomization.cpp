// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanLiveLinkSubjectSettingsCustomization.h"

#include "MetaHumanLiveLinkSubjectSettings.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MetaHumanLiveLinkSourceEditor"



TSharedRef<IDetailCustomization> FMetaHumanLiveLinkSubjectSettingsCustomization::MakeInstance()
{
	return MakeShared<FMetaHumanLiveLinkSubjectSettingsCustomization>();
}

void FMetaHumanLiveLinkSubjectSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	InDetailBuilder.GetObjectsBeingCustomized(Objects);

	check(Objects.Num() == 1);
	UMetaHumanLiveLinkSubjectSettings* Settings = Cast<UMetaHumanLiveLinkSubjectSettings>(Objects[0]);

	if (!Settings->bIsLiveProcessing)
	{
		return;
	}

	ButtonTextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("ButtonText");
	ButtonTextStyle.SetFont(IDetailLayoutBuilder::GetDetailFont());

	TSharedRef<IPropertyHandle> CaptureNeutralsProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanLiveLinkSubjectSettings, CaptureNeutralsProperty));
	IDetailPropertyRow* CaptureNeutralsRow = InDetailBuilder.EditDefaultProperty(CaptureNeutralsProperty);
	check(CaptureNeutralsRow);

	CaptureNeutralsRow->CustomWidget()
		.NameContent()
		[
			SNew(STextBlock)
		]
		.ValueContent()
		[
			SNew(SButton)
			.TextStyle(&ButtonTextStyle)
			.ToolTipText(LOCTEXT("CaptureNeutralsTooltip", "Capture a frame with both a neutral facial expression and neutral head position"))
			.Text_Lambda([Settings]()
			{
				int32 MaxFrameCountdown = FMath::Max(Settings->CaptureNeutralFrameCountdown, Settings->CaptureNeutralHeadTranslationCountdown);

				if (MaxFrameCountdown == -1)
				{
					return LOCTEXT("CaptureNeutrals", "Capture Neutrals");
				}
				else
				{
					return FText::FromString(FString::Printf(TEXT("Hold neutral pose %i"), MaxFrameCountdown));
				}
			})
			.OnClicked_Lambda([Settings]()
			{
				Settings->CaptureNeutrals();
				return FReply::Handled();
			})
		];

	TSharedRef<IPropertyHandle> CaptureNeutralFrameProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanLiveLinkSubjectSettings, CaptureNeutralFrameCountdown));
	IDetailPropertyRow* CaptureNeutralFrameRow = InDetailBuilder.EditDefaultProperty(CaptureNeutralFrameProperty);
	check(CaptureNeutralFrameRow);

	CaptureNeutralFrameRow->CustomWidget()
		.NameContent()
		[
			SNew(STextBlock)
		]
		.ValueContent()
		[
			SNew(SButton)
			.TextStyle(&ButtonTextStyle)
			.ToolTipText(LOCTEXT("CaptureNeutralTooltip", "Capture a frame with a neutral facial expression"))
			.Text_Lambda([Settings]()
			{
				if (Settings->CaptureNeutralFrameCountdown == -1)
				{ 
					return LOCTEXT("CaptureNeutral", "Capture Neutral");
				}
				else
				{
					return FText::FromString(FString::Printf(TEXT("Hold neutral pose %i"), Settings->CaptureNeutralFrameCountdown));
				}
			})
			.OnClicked_Lambda([Settings]()
			{
				Settings->CaptureNeutralFrame();
				return FReply::Handled();
			})
		];

	TSharedRef<IPropertyHandle> CaptureNeutralHeadTranslationProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanLiveLinkSubjectSettings, CaptureNeutralHeadTranslationCountdown));
	IDetailPropertyRow* CaptureNeutralHeadTranslationRow = InDetailBuilder.EditDefaultProperty(CaptureNeutralHeadTranslationProperty);
	check(CaptureNeutralHeadTranslationRow);

	CaptureNeutralHeadTranslationRow->CustomWidget()
		.NameContent()
		[
			SNew(STextBlock)
		]
		.ValueContent()
		[
			SNew(SButton)
			.TextStyle(&ButtonTextStyle)
			.ToolTipText(LOCTEXT("CaptureNeutralHeadTranslationTooltip", "Capture a frame where the head is located in a neutral position"))
			.Text_Lambda([Settings]()
			{
				if (Settings->CaptureNeutralHeadTranslationCountdown == -1)
				{ 
					return LOCTEXT("CaptureNeutralHeadTranslation", "Capture Neutral");
				}
				else
				{
					return FText::FromString(FString::Printf(TEXT("Hold neutral pose %i"), Settings->CaptureNeutralHeadTranslationCountdown));
				}
			})
			.OnClicked_Lambda([Settings]()
			{
				Settings->CaptureNeutralHeadTranslation();
				return FReply::Handled();
			})
		];
}

#undef LOCTEXT_NAMESPACE
