// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaletteEditor/MetaHumanCharacterPaletteEditorToolkit.h"
#include "PaletteEditor/MetaHumanCharacterPaletteEditorViewportClient.h"
#include "PaletteEditor/SMetaHumanCharacterPaletteEditorViewport.h"
#include "PaletteEditor/MetaHumanCharacterPaletteAssetEditor.h"
#include "PaletteEditor/MetaHumanCharacterPaletteEditorCommands.h"
#include "Framework/Docking/LayoutExtender.h"
#include "MetaHumanCharacterActorInterface.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterPaletteItemWrapper.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "AdvancedPreviewScene.h"
#include "AssetEditorModeManager.h"
#include "EditorViewportTabContent.h"
#include "Selection.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "ToolMenus.h"
#include "Editor/EditorEngine.h"
#include "Widgets/SCharacterPartsView.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterPaletteEditor"

static const FName PartsViewTabID("PartsView");
static const FName ItemDetailsTabID("ItemDetails");

FMetaHumanCharacterPaletteEditorToolkit::FMetaHumanCharacterPaletteEditorToolkit(UMetaHumanCharacterPaletteAssetEditor* InOwningAssetEditor)
	: FBaseAssetToolkit{ InOwningAssetEditor }
{
	ItemWrapper = TStrongObjectPtr(NewObject<UMetaHumanCharacterPaletteItemWrapper>());

	StandaloneDefaultLayout = FTabManager::NewLayout(TEXT("MetaHumanCharacterPaletteEditorLayout_3"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(PartsViewTabID, ETabState::OpenedTab)
					->SetExtensionId("PartsView")
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->AddTab(ViewportTabID, ETabState::OpenedTab)
					->SetExtensionId(TEXT("ViewportArea"))
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab(ItemDetailsTabID, ETabState::OpenedTab)
						->SetExtensionId(TEXT("ItemDetailsArea"))
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab(DetailsTabID, ETabState::OpenedTab)
						->SetExtensionId(TEXT("DetailsArea"))
					)
				)
			)
		);

	LayoutExtender = MakeShared<FLayoutExtender>();

	PreviewScene = MakeUnique<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues{});
}

UMetaHumanCharacterPaletteAssetEditor* FMetaHumanCharacterPaletteEditorToolkit::GetCharacterEditor()
{
	return Cast<UMetaHumanCharacterPaletteAssetEditor>(OwningAssetEditor);
}

void FMetaHumanCharacterPaletteEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FBaseAssetToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(PartsViewTabID, FOnSpawnTab::CreateSP(this, &FMetaHumanCharacterPaletteEditorToolkit::SpawnTab_PartsView))
		.SetDisplayName(LOCTEXT("PartsViewTab", "Character Parts"));

	if (GetCharacterEditor()->IsPaletteEditable())
	{
		// This tab edits build parameters of a part, so is only visible if the Character is editable
		InTabManager->RegisterTabSpawner(ItemDetailsTabID, FOnSpawnTab::CreateSP(this, &FMetaHumanCharacterPaletteEditorToolkit::SpawnTab_ItemDetails))
			.SetDisplayName(LOCTEXT("ItemDetailsTab", "Item Details"));
	}
}

void FMetaHumanCharacterPaletteEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FBaseAssetToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(PartsViewTabID);
	if (GetCharacterEditor()->IsPaletteEditable())
	{
		InTabManager->UnregisterTabSpawner(ItemDetailsTabID);
	}
}

TSharedRef<SDockTab> FMetaHumanCharacterPaletteEditorToolkit::SpawnTab_PartsView(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PartsViewTab", "Character Parts"))
		.ToolTipText(LOCTEXT("PartsViewTabTooltip", "Shows the parts currently imported into this Character"))
		[
			SAssignNew(PartsViewWidget, SCharacterPartsView)
			.CharacterPalette(GetCharacterEditor()->GetMetaHumanCollection())
			.OnSelectionChanged(this, &FMetaHumanCharacterPaletteEditorToolkit::OnPartsViewSelectionChanged)
			.OnMouseButtonDoubleClick(this, &FMetaHumanCharacterPaletteEditorToolkit::OnPartsViewDoubleClick)
			.IsPaletteEditable(GetCharacterEditor()->IsPaletteEditable())
		];
}

TSharedRef<SDockTab> FMetaHumanCharacterPaletteEditorToolkit::SpawnTab_ItemDetails(const FSpawnTabArgs& Args)
{
	if (!ItemDetailsView.IsValid())
	{
		ItemDetailsView = CreateDetailsView();
		ItemDetailsView->OnFinishedChangingProperties().AddSP(this, &FMetaHumanCharacterPaletteEditorToolkit::OnFinishedChangingItemProperties);
	}

	return SNew(SDockTab)
		.Label(LOCTEXT("ItemDetailsTab", "Item Details"))
		.ToolTipText(LOCTEXT("ItemDetailsTabTooltip", "The details of the currently selected item in the Character Parts view"))
		[
			ItemDetailsView.ToSharedRef()
		];
}

void FMetaHumanCharacterPaletteEditorToolkit::OnPartsViewSelectionChanged(TSharedPtr<FMetaHumanCharacterPaletteItem> NewSelectedItem, ESelectInfo::Type SelectInfo)
{
	if (NewSelectedItem.IsValid())
	{
		ItemWrapper->Item = *NewSelectedItem;

		if (ItemDetailsView)
		{
			const bool bForceRefresh = true;
			ItemDetailsView->SetObject(ItemWrapper.Get(), bForceRefresh);
		}
	}
	else
	{
		if (ItemDetailsView)
		{
			ItemDetailsView->SetObject(nullptr);
		}
	}

	CurrentlySelectedItem = NewSelectedItem;
	CurrentlySelectedItemKey = NewSelectedItem->GetItemKey();
}

void FMetaHumanCharacterPaletteEditorToolkit::OnPartsViewDoubleClick(TSharedPtr<FMetaHumanCharacterPaletteItem> Item)
{
	if (!Item.IsValid())
	{
		return;
	}

	const FName ParameterName = Item->SlotName;
	if (ParameterName == NAME_None)
	{
		return;
	}

	GetCharacterEditor()->GetCharacterInstance()->SetSingleSlotSelection(ParameterName, Item->GetItemKey());
	UpdatePreviewActor();
}

void FMetaHumanCharacterPaletteEditorToolkit::OnFinishedChangingItemProperties(const FPropertyChangedEvent& Event)
{
	if (!CurrentlySelectedItem.IsValid()
		|| !GetCharacterEditor()->IsPaletteEditable())
	{
		return;
	}

	if (ItemWrapper->Item.GetItemKey() != CurrentlySelectedItemKey
		&& GetCharacterEditor()->GetMetaHumanCollection()->ContainsItem(ItemWrapper->Item.GetItemKey()))
	{
		// The user has modified the properties of the item such that its key conflicts with 
		// another item.
		//
		// For now we adjust the item's Variation to make it unique. In future, we may pop up a 
		// dialog to explain the problem.
		ItemWrapper->Item.Variation = GetCharacterEditor()->GetMetaHumanCollection()->GenerateUniqueVariationName(ItemWrapper->Item.GetItemKey());
		check(!GetCharacterEditor()->GetMetaHumanCollection()->ContainsItem(ItemWrapper->Item.GetItemKey()));
	}

	// Save off the key so that we can update CurrentlySelectedItemKey before calling 
	// WriteItemToCharacterPalette, in case that call triggers another function on this object.
	//
	// This ensures that CurrentlySelectedItem and CurrentlySelectedItemKey are guaranteed to be in
	// sync when any function is called.
	const FMetaHumanPaletteItemKey OldItemKey = CurrentlySelectedItemKey;

	// Copy property values from wrapper object back to actual selected item
	*CurrentlySelectedItem = ItemWrapper->Item;
	CurrentlySelectedItemKey = ItemWrapper->Item.GetItemKey();

	// Commit the change back to the UMetaHumanCharacterPalette asset as well
	PartsViewWidget->WriteItemToCharacterPalette(OldItemKey, CurrentlySelectedItem.ToSharedRef());
}

AssetEditorViewportFactoryFunction FMetaHumanCharacterPaletteEditorToolkit::GetViewportDelegate()
{
	AssetEditorViewportFactoryFunction ViewportDelegateFunction = [this](FAssetEditorViewportConstructionArgs InArgs)
	{
		return SNew(SMetaHumanCharacterPaletteEditorViewport, InArgs)
			.EditorViewportClient(ViewportClient);
	};

	return ViewportDelegateFunction;
}

TSharedPtr<FEditorViewportClient> FMetaHumanCharacterPaletteEditorToolkit::CreateEditorViewportClient() const
{
	return MakeShared<FMetaHumanCharacterPaletteViewportClient>(EditorModeManager.Get(), PreviewScene.Get());
}

void FMetaHumanCharacterPaletteEditorToolkit::PostInitAssetEditor()
{
	if (GetCharacterEditor()->IsPaletteEditable())
	{
		ToolkitCommands->MapAction(FMetaHumanCharacterPaletteEditorCommands::Get().Build,
								FExecuteAction::CreateSP(this, &FMetaHumanCharacterPaletteEditorToolkit::Build));

		FName ParentToolbarName;
		const FName ToolbarName = GetToolMenuToolbarName(ParentToolbarName);
		UToolMenu* AssetToolbar = UToolMenus::Get()->ExtendMenu(ToolbarName);
		FToolMenuSection& Section = AssetToolbar->FindOrAddSection(TEXT("Asset"));
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(FMetaHumanCharacterPaletteEditorCommands::Get().Build));
	}

	// TODO: Currently we do this even when editing an instance to ensure the Palette is built.
	// In future, we should only build here if the Palette is not already built.
	Build();

	// We need the viewport client to start out focused, or else it won't get ticked until
	// we click inside it. This makes sure streaming of assets will actually finish before
	// the user clicks on the viewport
	ViewportClient->ReceivedFocus(ViewportClient->Viewport);

	// TODO: hard-coded values to set the camera in a sensible initial location.
	// This should really be handled by a focus viewport to selection
	ViewportClient->SetViewLocation(FVector{ 0, 80, 143 });
	ViewportClient->SetViewRotation(FRotator{ 0, -90, 0 });
	ViewportClient->SetLookAtLocation(FVector{ 0, 0, 143 });
	ViewportClient->ViewFOV = 18.001738f; // Same FoV used in MHC

	// Enable the orbit camera by default
	ViewportClient->ToggleOrbitCamera(true);
}

void FMetaHumanCharacterPaletteEditorToolkit::Build()
{
	UMetaHumanCollection* Collection = GetCharacterEditor()->GetMetaHumanCollection();
	if (!Collection->GetEditorPipeline())
	{
		if (PreviewActor.IsValid())
		{
			PreviewActor->Destroy();
			PreviewActor = nullptr;
		}

		return;
	}

	Collection->Build(
		FInstancedStruct(),
		EMetaHumanCharacterPaletteBuildQuality::Production,
		GetTargetPlatformManagerRef().GetRunningTargetPlatform(),
		UMetaHumanCollection::FOnBuildComplete::CreateLambda(
			[Toolkit = SharedThis(this)](EMetaHumanBuildStatus Status)
			{
				if (Status == EMetaHumanBuildStatus::Succeeded)
				{
					Toolkit->UpdatePreviewActor();
				}
				else
				{
					if (Toolkit->PreviewActor.IsValid())
					{
						Toolkit->PreviewActor->Destroy();
						Toolkit->PreviewActor = nullptr;
					}
				}
			}));
}

void FMetaHumanCharacterPaletteEditorToolkit::UpdatePreviewActor()
{
	UMetaHumanCharacterInstance* Instance = GetCharacterEditor()->GetCharacterInstance();

	Instance->Assemble(EMetaHumanCharacterPaletteBuildQuality::Preview, FMetaHumanCharacterAssembledNative::CreateSP(this, &FMetaHumanCharacterPaletteEditorToolkit::OnMetaHumanCharacterAssembled));
}

void FMetaHumanCharacterPaletteEditorToolkit::OnMetaHumanCharacterAssembled(EMetaHumanCharacterAssemblyResult Status)
{
	if (PreviewActor.IsValid())
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}

	if (Status != EMetaHumanCharacterAssemblyResult::Succeeded)
	{
		return;
	}

	UMetaHumanCollection* Collection = GetCharacterEditor()->GetMetaHumanCollection();
	if (!Collection
		|| !Collection->GetPipeline()
		|| !Collection->GetPipeline()->GetActorClass()
		|| !Collection->GetPipeline()->GetActorClass()->ImplementsInterface(UMetaHumanCharacterActorInterface::StaticClass()))
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewActor = TWeakObjectPtr(PreviewScene->GetWorld()->SpawnActor<AActor>(Collection->GetPipeline()->GetActorClass(), SpawnParameters));
	if (!PreviewActor.IsValid())
	{
		// Failed to spawn
		return;
	}

	check(PreviewActor->Implements<UMetaHumanCharacterActorInterface>());

	IMetaHumanCharacterActorInterface::Execute_SetCharacterInstance(PreviewActor.Get(), GetCharacterEditor()->GetCharacterInstance());
}

#undef LOCTEXT_NAMESPACE
