// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosOutfitAsset/OutfitAssetFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ChaosOutfitAsset/OutfitAsset.h"
#include "Dataflow/DataflowObject.h"
#include "Dialog/SMessageDialog.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OutfitAssetFactory)

#define LOCTEXT_NAMESPACE "OutfitAssetFactory"

UChaosOutfitAssetFactory::UChaosOutfitAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bEditorImport = true;
	bEditAfterNew = true;
	SupportedClass = UChaosOutfitAsset::StaticClass();
}

UObject* UChaosOutfitAssetFactory::FactoryCreateNew(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, UObject* /*Context*/, FFeedbackContext* /*Warn*/)
{
	UChaosOutfitAsset* const OutfitAsset = NewObject<UChaosOutfitAsset>(Parent, Class, Name, Flags | RF_Transactional | RF_Public | RF_Standalone);
	OutfitAsset->MarkPackageDirty();

	// Create a new Dataflow asset
	const FString DataflowPath = FPackageName::GetLongPackagePath(OutfitAsset->GetOutermost()->GetName());
	const FString OutfitAssetName = OutfitAsset->GetName();
	FString DataflowName = FString(TEXT("DF_")) + (OutfitAssetName.StartsWith(TEXT("OA_")) ? OutfitAssetName.RightChop(3) : OutfitAssetName);
	FString DataflowPackageName = FPaths::Combine(DataflowPath, DataflowName);
	if (FindPackage(nullptr, *DataflowPackageName))
	{
		// If a Dataflow asset already exists with this name, make a unique name from it to avoid clobbering it
		MakeUniqueObjectName(nullptr, UPackage::StaticClass(), FName(DataflowPackageName)).ToString(DataflowPackageName);
		DataflowName = FPaths::GetBaseFilename(DataflowPackageName);
	}
	UPackage* const DataflowPackage = CreatePackage(*DataflowPackageName);

	// Select the template
	TSharedRef<SMessageDialog> SelectTemplateMessageDialog = SNew(SMessageDialog)
		.Title(FText(LOCTEXT("SelectTemplateTitle", "Select an Outfit Asset Template")))
		.Message(LOCTEXT("SelectTemplateMessage", "Select a template for this newly created Outfit Asset:"))
		.Buttons(
			{
				SMessageDialog::FButton(LOCTEXT("SelectNoTemplate", "No Dataflow"))
					.SetToolTipText(LOCTEXT("SelectNoTemplateTooltip", "Don't add a Dataflow to this Outfit Asset. Useful when a Dataflow already exists."))
					.SetPrimary(false),
				SMessageDialog::FButton(LOCTEXT("SelectEmptyTemplate", "Empty Dataflow"))
					.SetToolTipText(LOCTEXT("SelectEmptyTemplateTooltip", "Add an empty Dataflow with an Outfit Asset Terminal node."))
					.SetPrimary(false),
				SMessageDialog::FButton(LOCTEXT("SelectSimpleOutfitTemplate", "Simple Outfit"))
					.SetToolTipText(LOCTEXT("SelectSimpleOutfitTemplateTooltip", "Add a Dataflow with a simple Cloth Asset aggregator graph. Allows to simulate multiple Cloth Assets from the same ChaosClothComponent."))
					.SetPrimary(true),
				SMessageDialog::FButton(LOCTEXT("SelectResizableOutfitTemplate", "Resizable Outfit"))
					.SetToolTipText(LOCTEXT("SelectResizableOutfitTemplateTooltip", "Add a Dataflow that builds a single resizable garment from multiple Cloth Assets. Body sizes will have to be provided in addition to the mutiple Cloth Assets."))
					.SetPrimary(false),
				SMessageDialog::FButton(LOCTEXT("SelectResizingGraph", "Resizing Graph"))
					.SetToolTipText(LOCTEXT("SelectResizingGraphTooltip", "Resizing graph to test a resizable Outfit Asset."))
					.SetPrimary(false),
			});

	FString OutfitAssetTemplate;
	switch (SelectTemplateMessageDialog->ShowModal())
	{
	default:
	case 0:  // No Dataflow
		break;
	case 1:  // Empty Dataflow
		OutfitAssetTemplate = TEXT("/ChaosOutfitAsset/EmptyOutfitAssetTemplate.EmptyOutfitAssetTemplate");
		break;
	case 2:  // Simple Outfit
		OutfitAssetTemplate = TEXT("/ChaosOutfitAsset/OutfitAssetTemplate.OutfitAssetTemplate");
		break;
	case 3:  // Resizable Outfit
		OutfitAssetTemplate = TEXT("/ChaosOutfitAsset/MakeResizableOutfitTemplate.MakeResizableOutfitTemplate");
		break;
	case 4:  // Resizing Graph
		OutfitAssetTemplate = TEXT("/ChaosOutfitAsset/ResizeOutfitTemplate.ResizeOutfitTemplate");
		break;
	}

	// Load the cloth template into the new Dataflow asset
	UDataflow* const Template = OutfitAssetTemplate.IsEmpty() ? nullptr: LoadObject<UDataflow>(DataflowPackage, *OutfitAssetTemplate);
	if (UDataflow* const Dataflow = Template ? DuplicateObject(Template, DataflowPackage, FName(DataflowName)) : nullptr)
	{
		Dataflow->MarkPackageDirty();

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(Dataflow);

		// Set the Dataflow to the cloth asset
		OutfitAsset->SetDataflow(Dataflow);
	}

	return OutfitAsset;
}

FString UChaosOutfitAssetFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("OA_NewOutfitAsset"));
}

#undef LOCTEXT_NAMESPACE
