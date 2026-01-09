// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanSpeechProcessingSettings.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "DetailLayoutBuilder.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "SMetaHumanSpeechToAnimProcessingSettings"

void SMetaHumanSpeechToAnimProcessingSettings::Construct(const FArguments& InArgs)
{
	check(InArgs._Settings);

	SettingsObject = InArgs._Settings;
	CanProcessConditional = InArgs._CanProcessConditional;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsView->SetObject(InArgs._Settings);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				DetailsView
			]
			// Export/Cancel buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(8)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding( FAppStyle::GetMargin("StandardDialog.ContentPadding") )
					.OnClicked(this, &SMetaHumanSpeechToAnimProcessingSettings::ProcessClicked)
					.IsEnabled(this, &SMetaHumanSpeechToAnimProcessingSettings::CanProcess)
					.Text(LOCTEXT("CreateButton", "Create"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding( FAppStyle::GetMargin("StandardDialog.ContentPadding") )
					.OnClicked(this, &SMetaHumanSpeechToAnimProcessingSettings::CancelClicked)
					.Text(LOCTEXT("CancelButton", "Cancel"))
				]
			]
		]
	];
}

EAppReturnType::Type SMetaHumanSpeechToAnimProcessingSettings::ShowModel()
{
	check(!DialogWindow.IsValid());

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("SMetaHumanSpeechToAnimProcessorSettingsTitle", "Process Audio To Animation"))
		.Type(EWindowType::Normal)
		.SizingRule(ESizingRule::Autosized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.FocusWhenFirstShown(true)
		.ActivationPolicy(EWindowActivationPolicy::FirstShown)
		[
			AsShared()
		];

	Window->SetWidgetToFocusOnActivate(AsShared());

	DialogWindow = Window;

	GEditor->EditorAddModalWindow(Window);

	return UserResponse;
}

void SMetaHumanSpeechToAnimProcessingSettings::RequestDestroyWindow()
{
	if (DialogWindow.IsValid())
	{
		DialogWindow.Pin()->RequestDestroyWindow();
		DialogWindow.Reset();
	}
}

bool SMetaHumanSpeechToAnimProcessingSettings::CanProcess() const
{
	return !CanProcessConditional.IsBound() || CanProcessConditional.Get();
}

FReply SMetaHumanSpeechToAnimProcessingSettings::ProcessClicked()
{
	UserResponse = EAppReturnType::Ok;
	RequestDestroyWindow();
	return FReply::Handled();
}

FReply SMetaHumanSpeechToAnimProcessingSettings::CancelClicked()
{
	UserResponse = EAppReturnType::Cancel;
	RequestDestroyWindow();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE