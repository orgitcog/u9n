// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextRigVMAssetEditorDataCustomization.h"

#include "AnimNextRigVMAsset.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "AnimNextRigVMAssetEditorDataCustomization"

namespace UE::AnimNext::Editor
{

void FAnimNextRigVMAssetEditorDataCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedRef<IPropertyHandle> ExternalPackagesProperty = DetailBuilder.GetProperty(UAnimNextRigVMAssetEditorData::GetUsesExternalPackagesPropertyName());
	
	TArray<TWeakObjectPtr<UObject>> Objects = DetailBuilder.GetSelectedObjects();
	if(Objects.Num() != 1)
	{
		ExternalPackagesProperty->MarkHiddenByCustomization();
		return;
	}

	UAnimNextRigVMAsset* Asset = CastChecked<UAnimNextRigVMAsset>(Objects[0]);
	UAnimNextRigVMAssetEditorData* EditorData = UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(Asset);
	if(EditorData == nullptr)
	{
		ExternalPackagesProperty->MarkHiddenByCustomization();
		return;
	}

	IDetailPropertyRow* PropertyRow = DetailBuilder.EditDefaultProperty(ExternalPackagesProperty);
	if(PropertyRow == nullptr)
	{
		ExternalPackagesProperty->MarkHiddenByCustomization();
		return;
	}

	PropertyRow->CustomWidget()
	.NameContent()
	[
		ExternalPackagesProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SButton)
		.Text_Lambda([EditorData]()
		{
			return EditorData->IsUsingExternalPackages() ?
				LOCTEXT("DisableExternalPackagesLabel", "Use Single Package") :
				LOCTEXT("EnableExternalPackagesLabel", "Use External Packages");
		})
		.ToolTipText_Lambda([EditorData]()
		{
			return EditorData->IsUsingExternalPackages() ?
				LOCTEXT("DisableExternalPackagesTooltip", "Set this asset to use a single package.\nThis will remove any external packages for existing entries, remove them from version control if enabled and save all packages.\nWarning: This operation cannot be undone, so a connection to version control is recommended.") :
				LOCTEXT("EnableExternalPackagesTooltip", "Set this asset to use external packaging for its entries (graphs, variables etc.)\nThis will create the external packages for all entries, add them to version control if enabled and save all packages.\nWarning: This operation cannot be undone, so a connection to version control is recommended.");
		})
		.OnClicked_Lambda([Asset, EditorData]() mutable
		{
			UAnimNextRigVMAssetEditorData::SetUseExternalPackages(TArrayView<UAnimNextRigVMAsset*>(&Asset, 1), !EditorData->IsUsingExternalPackages());
			return FReply::Handled();
		})
	]
	.ResetToDefaultContent()
	[
		SNullWidget::NullWidget
	];
}

}

#undef LOCTEXT_NAMESPACE
