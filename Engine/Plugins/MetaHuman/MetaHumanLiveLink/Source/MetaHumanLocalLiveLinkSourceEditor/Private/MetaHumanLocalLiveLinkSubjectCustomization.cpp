// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanLocalLiveLinkSubjectCustomization.h"
#include "MetaHumanLocalLiveLinkSubjectSettings.h"
#include "MetaHumanLocalLiveLinkSubjectMonitorWidget.h"
#include "MetaHumanLocalLiveLinkSubject.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "EditorFontGlyphs.h"
#include "Widgets/Text/STextBlock.h"

#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "MetaHumanLocalLiveLinkSource"



TSharedRef<IDetailCustomization> FMetaHumanLocalLiveLinkSubjectCustomization::MakeInstance()
{
	return MakeShared<FMetaHumanLocalLiveLinkSubjectCustomization>();
}

void FMetaHumanLocalLiveLinkSubjectCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	InDetailBuilder.GetObjectsBeingCustomized(Objects);

	check(Objects.Num() == 1);
	UMetaHumanLocalLiveLinkSubjectSettings* Settings = Cast<UMetaHumanLocalLiveLinkSubjectSettings>(Objects[0]);

	if (!Settings->bIsLiveProcessing)
	{
		return;
	}

	ButtonTextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("ButtonText");
	ButtonTextStyle.SetFont(IDetailLayoutBuilder::GetDetailFont());

	// Only create the monitor widget if derived class of the settings has not already created it
	TArray<FName> CategoryNames;
	InDetailBuilder.GetCategoryNames(CategoryNames);
	if (!CategoryNames.Contains("Monitor") && !CategoryNames.Contains("Image") && !CategoryNames.Contains("Audio"))
	{
		IDetailCategoryBuilder& MonitorCategory = InDetailBuilder.EditCategory("Monitor", LOCTEXT("Monitor", "Monitor"), ECategoryPriority::Important);

		MonitorCategory.AddCustomRow(LOCTEXT("Monitor", "Monitor"))
			.WholeRowContent()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SMetaHumanLocalLiveLinkSubjectMonitorWidget, Settings)
				]
			];
	}

	TSharedRef<IPropertyHandle> RemoveProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanLocalLiveLinkSubjectSettings, Remove));
	IDetailPropertyRow* RemoveRow = InDetailBuilder.EditDefaultProperty(RemoveProperty);
	check(RemoveRow);

	RemoveRow->CustomWidget()
		.NameContent()
		[
			SNew(STextBlock)
		]
		.ValueContent()
		[
			SNew(SButton)
			.Text(LOCTEXT("RemoveSubject", "Remove Subject"))
			.TextStyle(&ButtonTextStyle)
			.OnClicked_Lambda([Settings]()
			{
				Settings->Subject->RemoveSubject();

				return FReply::Handled();
			})
		];

	IDetailCategoryBuilder& InfoCategory = InDetailBuilder.EditCategory("Information", LOCTEXT("Information", "Information"), ECategoryPriority::Important);

	TSharedRef<IPropertyHandle> StateProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanLocalLiveLinkSubjectSettings, State));
	IDetailPropertyRow* StateRow = InDetailBuilder.EditDefaultProperty(StateProperty);
	check(StateRow);

	TSharedPtr<SWidget> NameWidget, ValueWidget;
	StateRow->GetDefaultWidgets(NameWidget, ValueWidget);

	StateRow->CustomWidget()
		.NameContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(0, 4, 0 ,0)
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("FontAwesome.8"))
				.Text(FEditorFontGlyphs::Circle)
				.ColorAndOpacity_Lambda([Settings]()
				{
					return Settings->StateLED;
				})
			]
			+SHorizontalBox::Slot()
			.Padding(5, 0, 0 ,0)
			.AutoWidth()
			[
				NameWidget.ToSharedRef()
			]
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text_Lambda([Settings]()
			{
				return FText::FromString(Settings->State);
			})
		];

	TSharedRef<IPropertyHandle> StateLEDProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanLocalLiveLinkSubjectSettings, StateLED));
	IDetailPropertyRow* StateLEDRow = InDetailBuilder.EditDefaultProperty(StateLEDProperty);
	check(StateLEDRow);

	StateLEDRow->Visibility(EVisibility::Hidden);

	TSharedRef<IPropertyHandle> FrameProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanLocalLiveLinkSubjectSettings, Frame));
	IDetailPropertyRow* FrameRow = InDetailBuilder.EditDefaultProperty(FrameProperty);
	check(FrameRow);

	FrameRow->GetDefaultWidgets(NameWidget, ValueWidget);

	FrameRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text_Lambda([Settings]()
			{
				return FText::FromString(Settings->Frame);
			})
		];

	TSharedRef<IPropertyHandle> FPSProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanLocalLiveLinkSubjectSettings, FPS));
	IDetailPropertyRow* FPSRow = InDetailBuilder.EditDefaultProperty(FPSProperty);
	check(FPSRow);

	FPSRow->GetDefaultWidgets(NameWidget, ValueWidget);

	FPSRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text_Lambda([Settings]()
			{
				return FText::FromString(Settings->FPS);
			})
		];

	TSharedRef<IPropertyHandle> TimecodeProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanLocalLiveLinkSubjectSettings, Timecode));
	IDetailPropertyRow* TimecodeRow = InDetailBuilder.EditDefaultProperty(TimecodeProperty);
	check(TimecodeRow);

	TimecodeRow->GetDefaultWidgets(NameWidget, ValueWidget);

	TimecodeRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text_Lambda([Settings]()
			{
				return FText::FromString(Settings->Timecode);
			})
		];
}

#undef LOCTEXT_NAMESPACE
