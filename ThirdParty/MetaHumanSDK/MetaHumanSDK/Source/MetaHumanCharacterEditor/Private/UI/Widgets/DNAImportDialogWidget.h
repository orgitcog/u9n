// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"

enum class EMetaHumanCharacterSkinPreviewMaterial : uint8;

class SDNAImportDialogWidget : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SDNAImportDialogWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void OnBrowseButtonClicked() const;
	void OnMeshTypeChanged(TSharedPtr<FString> InNewSelection, ESelectInfo::Type);

	FReply OnImportClicked();

	FString GetFilePath() const;
	FString GetImportName() const;
	FString GetImportPath() const;
	FString GetMeshType() const;
	TSharedPtr<EMetaHumanCharacterSkinPreviewMaterial> GetSelectedMaterial();

private:

	TSharedRef<SWidget> MakeComboWidget(TSharedPtr<FString> InItem);

	TSharedPtr<SEditableTextBox> FilePathBox;
	TSharedPtr<SEditableTextBox> NameBox;
	TSharedPtr<SEditableTextBox> PathBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> MeshTypeComboBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TextureTypeComboBox;
	TSharedPtr<SComboBox<TSharedPtr<EMetaHumanCharacterSkinPreviewMaterial>>> TextureSelectionComboBox;
	
	TArray<TSharedPtr<EMetaHumanCharacterSkinPreviewMaterial>> TextureSelectionOptions;
	TArray<TSharedPtr<FString>> MeshTypeOptions;
	FString SelectedMeshType;
	TSharedPtr<EMetaHumanCharacterSkinPreviewMaterial> CurrentTextureSelection;
	
};
