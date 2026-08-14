// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanGenerateDepthWindow.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "SSimpleComboButton.h"
#include "Editor.h"
#include "SWarningOrErrorBox.h"

#define LOCTEXT_NAMESPACE "SMetaHumanGenerateDepthWindow"

void SMetaHumanGenerateDepthWindow::Construct(const FArguments& InArgs)
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
		.Title(LOCTEXT("SMetaHumanGenerateDepthWindow_Title", "Choose Options for Depth Generation"))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.ClientSize(FVector2D(450, 450))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				GenerateWarningMessageIfNeeded()
			]
			+SVerticalBox::Slot()
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
			]
		]);
}

TOptional<TStrongObjectPtr<UMetaHumanGenerateDepthWindowOptions>> SMetaHumanGenerateDepthWindow::ShowModal()
{
	TStrongObjectPtr<UMetaHumanGenerateDepthWindowOptions> Options(NewObject<UMetaHumanGenerateDepthWindowOptions>());

	Options->AssetName = GetDirectoryName();
	Options->PackagePath.Path = GetDefaultPackagePath();
	Options->ImageSequenceRootPath = GetDefaultStoragePath();
	Options->ReferenceCameraCalibration = GetDefaultCameraCalibration();

	DetailsView->SetObject(Options.Get(), true);

	GEditor->EditorAddModalWindow(SharedThis(this));

	if (UserResponse)
	{
		return Options;
	}

	return {};
}

FString SMetaHumanGenerateDepthWindow::GetDefaultPackagePath()
{
	const FString PackagePath = FPaths::GetPath(CaptureData->GetOuter()->GetName());

	return PackagePath;
}

FDirectoryPath SMetaHumanGenerateDepthWindow::GetDefaultStoragePath()
{
	FPackagePath CaptureDataPackagePath = CaptureData->GetPackage()->GetLoadedPath();

	FString DepthDirectory = FPaths::GetPath(FPaths::ConvertRelativePathToFull(CaptureDataPackagePath.GetLocalFullPath())) / GetDirectoryName();

	return { DepthDirectory };
}

FString SMetaHumanGenerateDepthWindow::GetDirectoryName()
{
	return CaptureData->GetName() + TEXT("_DepthSequence");
}

TObjectPtr<UCameraCalibration> SMetaHumanGenerateDepthWindow::GetDefaultCameraCalibration()
{
	if (CaptureData->CameraCalibrations.IsEmpty())
	{
		return nullptr;
	}

	return IsValid(CaptureData->CameraCalibrations[0]) ? CaptureData->CameraCalibrations[0] : nullptr;
}

TSharedRef<SWidget> SMetaHumanGenerateDepthWindow::GenerateWarningMessageIfNeeded() const
{
	if (CaptureData->DepthSequences.IsEmpty())
	{
		return SNullWidget::NullWidget;
	}

	FText DepthSequenceWarningMessage = 
		LOCTEXT("DepthSequenceReplace", "The Generate Depth process will replace the existing depth sequence in the Capture Data.");

	TSharedRef<SWidget> MessageBox =
		SNew(SWarningOrErrorBox)
		.MessageStyle(EMessageStyle::Warning)
		.Padding(10.f) // Default is 16.f
		.Message(DepthSequenceWarningMessage);

	return MessageBox;
}

#undef LOCTEXT_NAMESPACE