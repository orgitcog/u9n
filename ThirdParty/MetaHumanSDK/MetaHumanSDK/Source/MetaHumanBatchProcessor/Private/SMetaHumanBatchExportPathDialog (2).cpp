// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanBatchExportPathDialog.h"
#include "Editor.h"
#include "EditorAnimUtils.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "MetaHumanBatchExportPathDialog"

void SMetaHumanBatchExportPathDialog::Construct(const FArguments& InArgs)
{
	CanProcessConditional = InArgs._CanProcessConditional;
	NameRule = InArgs._NameRule;
	check(NameRule);
	if(NameRule->FolderPath.IsEmpty())
	{
		if (InArgs._DefaultFolder.IsEmpty())
		{
			NameRule->FolderPath = "/Game";
		}
		else
		{
			NameRule->FolderPath = InArgs._DefaultFolder;
		}
	}

	// path picker
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = NameRule->FolderPath;
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateLambda([this](const FString& NewPath){
		NameRule->FolderPath = NewPath;
		UpdateCanProcess();
	});
	PathPickerConfig.bAddDefaultPath = true;
	PathPickerConfig.bShowViewOptions = true;

	const FText TitleText = FText::Format(LOCTEXT("MetaHumanBatchExport_Title", "{0} Output Paths"), FText::FromString(InArgs._AssetTypeName));
	const int32 WindowHeight = 650;

	SWindow::Construct(SWindow::FArguments()
	.Title(TitleText)
	.SupportsMinimize(false)
	.SupportsMaximize(false)
	.IsTopmostWindow(true)
	.ClientSize(FVector2D(350, WindowHeight))
	[
		SNew(SVerticalBox)		
		+ SVerticalBox::Slot()
		.Padding(2)
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.FillHeight(1)
			.Padding(3)
			[
				ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
			]
			
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2, 3)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MetaHumanBatchExport_Folder", "Export Path: "))
					.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				]

				+SHorizontalBox::Slot()
				.FillWidth(1)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return FText::FromString(NameRule->FolderPath);
					})
				]
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("MetaHumanBatchExport_RenameLabel", "Name New {0} Assets"), FText::FromString(InArgs._AssetTypeName)))
					.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(2, 1)
					[
						SNew(STextBlock).Text(LOCTEXT("MetaHumanBatchExport_Prefix", "Prefix"))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
							.Text_Lambda([this]()
							{
								return FText::FromString(NameRule->Prefix);
							})
							.OnTextChanged_Lambda([this](const FText& InText)
							{
								NameRule->Prefix = ConvertToCleanString(InText);
								UpdateCanProcess();
								UpdateExampleText();
							})
							.MinDesiredWidth(100)
							.IsReadOnly(false)
							.RevertTextOnEscape(true)
							.HintText(FText::FromString(InArgs._PrefixHint))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(2, 1)
					[
						SNew(STextBlock).Text(LOCTEXT("MetaHumanBatchExport_Suffix", "Suffix"))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
							.Text_Lambda([this]()
							{
								return FText::FromString(NameRule->Suffix);
							})
							.OnTextChanged_Lambda([this](const FText& InText)
							{
								NameRule->Suffix = ConvertToCleanString(InText);
								UpdateCanProcess();
								UpdateExampleText();	
							})
							.MinDesiredWidth(100)
							.IsReadOnly(false)
							.RevertTextOnEscape(true)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(2, 1)
					[
						SNew(STextBlock).Text(LOCTEXT("MetaHumanBatchExport_Search", "Search "))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
							.Text_Lambda([this]()
							{
								return FText::FromString(NameRule->ReplaceFrom);
							})
							.OnTextChanged_Lambda([this](const FText& InText)
							{
								NameRule->ReplaceFrom = ConvertToCleanString(InText);
								UpdateCanProcess();
								UpdateExampleText();
							})
							.MinDesiredWidth(100)
							.IsReadOnly(false)
							.RevertTextOnEscape(true)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(2, 1)
					[
						SNew(STextBlock).Text(LOCTEXT("MetaHumanBatchExport_Replace", "Replace "))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
							.Text_Lambda([this]()
							{
								return FText::FromString(NameRule->ReplaceTo);
							})
							.OnTextChanged_Lambda([this](const FText& InText)
							{
								NameRule->ReplaceTo = ConvertToCleanString(InText);
								UpdateCanProcess();
								UpdateExampleText();	
							})
							.MinDesiredWidth(100)
							.IsReadOnly(false)
							.RevertTextOnEscape(true)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 3)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(5, 5)
					[
						SNew(STextBlock)
						.Text_Lambda([this](){return ExampleText; })
						.Font(FAppStyle::GetFontStyle("Persona.RetargetManager.ItalicFont"))
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(5, 2)
					[
						SNew(STextBlock)
						.Text_Lambda([this](){return CanProcessText; })
						.Font(FAppStyle::GetFontStyle("Persona.RetargetManager.ItalicFont"))
					]
				]
			]
		]
		
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(5)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))

			+SUniformGridPanel::Slot(0, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("Export", "Export"))
				.OnClicked(this, &SMetaHumanBatchExportPathDialog::OnButtonClick, EAppReturnType::Ok)
				.IsEnabled(this, &SMetaHumanBatchExportPathDialog::CanProcess)
			]

			+SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked(this, &SMetaHumanBatchExportPathDialog::OnButtonClick, EAppReturnType::Cancel)
			]
		]
	]);

	UpdateCanProcess();
	UpdateExampleText();
}

FReply SMetaHumanBatchExportPathDialog::OnButtonClick(EAppReturnType::Type InButtonID)
{
	UserResponse = InButtonID;
	RequestDestroyWindow();
	return FReply::Handled();
}

bool SMetaHumanBatchExportPathDialog::CanProcess() const
{
	return bCanProcess;
}

void SMetaHumanBatchExportPathDialog::UpdateCanProcess()
{
	if (CanProcessConditional.IsBound())
	{
		FCanProcessResult CanProcessResult = CanProcessConditional.Get();
		bCanProcess = CanProcessResult.bCanProcess;
		CanProcessText = CanProcessResult.CanProcessText;
	}
}

EAppReturnType::Type SMetaHumanBatchExportPathDialog::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

void SMetaHumanBatchExportPathDialog::UpdateExampleText()
{
	const FString ReplaceFrom = FString::Printf(TEXT("Old Name : ***%s***"), *(NameRule->ReplaceFrom));
	const FString ReplaceTo = FString::Printf(TEXT("New Name : %s***%s***%s"), *(NameRule->Prefix), *(NameRule->ReplaceTo), *(NameRule->Suffix));
	ExampleText = FText::FromString(FString::Printf(TEXT("%s\n%s"), *ReplaceFrom, *ReplaceTo));
}

FText SMetaHumanBatchExportPathDialog::GetFolderPath() const
{
	return FText::FromString(NameRule->FolderPath);
}

FString SMetaHumanBatchExportPathDialog::ConvertToCleanString(const FText& InToClean)
{
	static TSet<TCHAR> IllegalChars = {' ','$', '&', '^', '/', '\\', '#', '@', '!', '*', '_', '(', ')'};
	
	FString StrToClean = InToClean.ToString();
	for (TCHAR& Char : StrToClean)
	{
		if (IllegalChars.Contains(Char))
		{
			Char = TEXT('_'); // Replace illegal char with underscore
		}
	}

	return MoveTemp(StrToClean);
}

#undef LOCTEXT_NAMESPACE
