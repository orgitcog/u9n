// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCalibrationGeneratorWindow.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "SSimpleComboButton.h"
#include "Editor.h"

#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCalibrationGeneratorWindow"

void SMetaHumanCalibrationGeneratorWindow::Construct(const FArguments& InArgs)
{
	CaptureData = InArgs._CaptureData;
	check(CaptureData);

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ENameAreaSettings::HideNameArea;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;
	DetailsViewArgs.bShowPropertyMatrixButton = false;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("SMetaHumanCalibrationGeneratorWindow_Title", "Choose Options for Calibration Generation"))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.ClientSize(FVector2D(450, 450))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+SScrollBox::Slot()
				[
					DetailsView->AsShared()
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.AutoWidth()
					[
						SNew(SButton)
						.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
						.Text(LOCTEXT("ContinueButton", "Continue"))
						.HAlign(HAlign_Center)
						.OnClicked_Lambda([this, InArgs]()
						{
							RequestDestroyWindow();
							UserResponse = true;
							return FReply::Handled(); 
						})
					]
					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.AutoWidth()
					[
						SNew(SButton)
						.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
						.Text(LOCTEXT("AbortButton", "Abort"))
						.HAlign(HAlign_Center)
						.OnClicked_Lambda([this]()
						{ 
							RequestDestroyWindow();
							UserResponse = false;
							return FReply::Handled(); 
						})
					]
				]
			]
		]);
}

TOptional<TStrongObjectPtr<UMetaHumanCalibrationGeneratorOptions>> SMetaHumanCalibrationGeneratorWindow::ShowModal()
{
	TStrongObjectPtr<UMetaHumanCalibrationGeneratorOptions> Options(NewObject<UMetaHumanCalibrationGeneratorOptions>());

	Options->PackagePath.Path = GetDefaultPackagePath();

	DetailsView->SetObject(Options.Get(), true);

	GEditor->EditorAddModalWindow(SharedThis(this));

	if (UserResponse)
	{
		return Options;
	}

	return {};
}

FString SMetaHumanCalibrationGeneratorWindow::GetDefaultPackagePath()
{
	const FString PackagePath = FPaths::GetPath(CaptureData->GetOuter()->GetName());

	return PackagePath;
}

#undef LOCTEXT_NAMESPACE