// Copyright Epic Games, Inc. All Rights Reserved.

#include "DNAImportDialogWidget.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Text/TextLayout.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"

#include "MetaHumanCharacter.h"


void SDNAImportDialogWidget::Construct(const FArguments& InArgs)
{
	MeshTypeOptions = { MakeShared<FString>(TEXT("Face")), MakeShared<FString>(TEXT("Body")) };
	TextureSelectionOptions.Add(MakeShared<EMetaHumanCharacterSkinPreviewMaterial>(EMetaHumanCharacterSkinPreviewMaterial::Default));
	TextureSelectionOptions.Add(MakeShared<EMetaHumanCharacterSkinPreviewMaterial>(EMetaHumanCharacterSkinPreviewMaterial::Clay));
	CurrentTextureSelection = TextureSelectionOptions[0];
	SelectedMeshType = "Face";

	
	SWindow::Construct(SWindow::FArguments().Title(FText::FromString(TEXT("DNA Import")))
			.ClientSize(FVector2D(600, 220))
			.SupportsMinimize(false)
			.SupportsMaximize(false));

	
	SetContent(
		SNew(SVerticalBox)

		// File path row
		+ SVerticalBox::Slot().AutoHeight().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SAssignNew(FilePathBox, SEditableTextBox)
				.HintText(NSLOCTEXT("DNAImport", "FilePathHint", "Select .dna file"))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0)
			[
				SNew(SButton)
				.Text(FText::FromString("Browse"))
				.OnClicked_Lambda([this]()
				{
					OnBrowseButtonClicked();
					return FReply::Handled();
				})
			]
		]

		// Name row
		+ SVerticalBox::Slot().AutoHeight().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString("Name:"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5, 0)
			[
				SAssignNew(NameBox, SEditableTextBox)
			]
		]

		// Path row
		+ SVerticalBox::Slot().AutoHeight().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString("Path:"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5, 0)
			[
				SAssignNew(PathBox, SEditableTextBox)
				.Text(FText::FromString("/Engine/ImportedMesh"))
			]
		]

		// Combo boxes
		+ SVerticalBox::Slot().AutoHeight().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f)
			[
				SAssignNew(MeshTypeComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&MeshTypeOptions)
				.OnGenerateWidget(this, &SDNAImportDialogWidget::MakeComboWidget)
				.OnSelectionChanged(this, &SDNAImportDialogWidget::OnMeshTypeChanged)
				.InitiallySelectedItem(MeshTypeOptions[0])
				[
					SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(SelectedMeshType); })
				]
			]
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(5, 0)
			[
				SAssignNew(TextureSelectionComboBox, SComboBox<TSharedPtr<EMetaHumanCharacterSkinPreviewMaterial>>)
				.OptionsSource(&TextureSelectionOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<EMetaHumanCharacterSkinPreviewMaterial> InItem)
				{
					FString Label = StaticEnum<EMetaHumanCharacterSkinPreviewMaterial>()->GetDisplayNameTextByValue((int64)*InItem).ToString();
					return SNew(STextBlock).Text(FText::FromString(Label));
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<EMetaHumanCharacterSkinPreviewMaterial> NewSelection, ESelectInfo::Type)
				{
					if (NewSelection.IsValid())
					{
						CurrentTextureSelection = NewSelection;
					}
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return FText::FromString(StaticEnum<EMetaHumanCharacterSkinPreviewMaterial>()->GetDisplayNameTextByValue((int64)*CurrentTextureSelection).ToString());
					})
				]
			]
		]

		// Import button
		+ SVerticalBox::Slot().AutoHeight().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().HAlign(HAlign_Left)
			[
				SNew(SButton)
				.Text(FText::FromString("Import"))
				.OnClicked(this, &SDNAImportDialogWidget::OnImportClicked)
			]
		]
	);
}

void SDNAImportDialogWidget::OnBrowseButtonClicked() const
{
	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	TArray<FString> OutFiles;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	if (DesktopPlatform && DesktopPlatform->OpenFileDialog(ParentWindowHandle, TEXT("Select DNA File"), TEXT(""), TEXT(""),
	                                                       TEXT("DNA files (*.dna)|*.dna"), EFileDialogFlags::None, OutFiles))
	{
		if (OutFiles.Num() > 0)
		{
			FilePathBox->SetText(FText::FromString(OutFiles[0]));
			NameBox->SetText(FText::FromString(FPaths::GetBaseFilename(OutFiles[0])));
		}
	}
}

TSharedRef<SWidget> SDNAImportDialogWidget::MakeComboWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(FText::FromString(*InItem));
}

void SDNAImportDialogWidget::OnMeshTypeChanged(TSharedPtr<FString> InNewSelection, ESelectInfo::Type)
{
	if (InNewSelection.IsValid())
	{
		SelectedMeshType = *InNewSelection;
	}
}

FReply SDNAImportDialogWidget::OnImportClicked()
{
	RequestDestroyWindow();
	
	return FReply::Handled();
}

FString SDNAImportDialogWidget::GetFilePath() const
{
	return FilePathBox.IsValid() ? FilePathBox->GetText().ToString() : FString();
}

FString SDNAImportDialogWidget::GetImportName() const
{
	return NameBox.IsValid() ? NameBox->GetText().ToString() : FString();
}

FString SDNAImportDialogWidget::GetImportPath() const
{
	return PathBox.IsValid() ? PathBox->GetText().ToString() : FString();
}

FString SDNAImportDialogWidget::GetMeshType() const
{
	return SelectedMeshType;
}

TSharedPtr<EMetaHumanCharacterSkinPreviewMaterial> SDNAImportDialogWidget::GetSelectedMaterial()
{
	return CurrentTextureSelection;
}
