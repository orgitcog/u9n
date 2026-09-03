// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorCostumeToolView.h"

#include "Algo/Find.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCollection.h"
#include "MetaHumanWardrobeItem.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Tools/MetaHumanCharacterEditorCostumeTools.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "SWarningOrErrorBox.h"
#include "GroomBindingAsset.h"
#include "Engine/SkeletalMesh.h"
#include "ChaosOutfitAsset/OutfitAsset.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorCostumeToolView"

namespace UE::MetaHuman::Private
{
	template<typename T>
	bool SlotSupportsType(TObjectPtr<UMetaHumanCollection> Collection, const FName& InSlotName)
	{
		if (const FMetaHumanCharacterPipelineSlot* Slot = Collection->GetPipeline()->GetSpecification()->Slots.Find(InSlotName))
		{
			return Slot->SupportsAssetType(T::StaticClass());
		}

		return false;
	}
} // UE::MetaHuman::Private

void SMetaHumanCharacterEditorCostumeToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorCostumeTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorCostumeToolView::GetToolProperties() const
{
	const UMetaHumanCharacterEditorCostumeTool* CostumeTool = Cast<UMetaHumanCharacterEditorCostumeTool>(Tool);
	return IsValid(CostumeTool) ? CostumeTool->GetCostumeToolProperties() : nullptr;
}

void SMetaHumanCharacterEditorCostumeToolView::MakeToolView()
{
	if(ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->ClearChildren();

		ToolViewScrollBox->AddSlot()
			.Padding(0.f, 4.f)
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateCostumeToolViewWarningSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateCostumeToolViewGroomsSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateCostumeToolViewOutfitClothingSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateCostumeToolViewSkeletalMeshSection()
				]
			];
	}

	MakeCostumeItemsBoxes();
}

void SMetaHumanCharacterEditorCostumeToolView::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	OnPreEditChangeProperty(PropertyAboutToChange, PropertyAboutToChange->GetName());
}

void SMetaHumanCharacterEditorCostumeToolView::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	const bool bIsInteractive = PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive;
	OnPostEditChangeProperty(PropertyThatChanged, bIsInteractive);
}

void SMetaHumanCharacterEditorCostumeToolView::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		Refresh();
	}
}

void SMetaHumanCharacterEditorCostumeToolView::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		Refresh();
	}
}

void SMetaHumanCharacterEditorCostumeToolView::Refresh()
{
	if (UMetaHumanCharacterEditorCostumeTool* CostumeTool = Cast<UMetaHumanCharacterEditorCostumeTool>(Tool))
	{
		CostumeTool->UpdateCostumeItems();

		if (UMetaHumanCharacterEditorCostumeToolProperties* CostumeToolProperties = CostumeTool->GetCostumeToolProperties())
		{
			TNotNull<UMetaHumanCharacterInstance*> Instance = CostumeToolProperties->Collection->GetMutableDefaultInstance();
			const TMap<FMetaHumanPaletteItemPath, FInstancedPropertyBag>& OverridenParams = Instance->GetOverriddenInstanceParameters();

			for (const TPair<FMetaHumanPaletteItemPath, FInstancedPropertyBag>& ParamIt : OverridenParams)
			{
				Instance->OverrideInstanceParameters(ParamIt.Key, ParamIt.Value);
			}
		}

		MakeToolView();
	}
}

TSharedRef<SWidget> SMetaHumanCharacterEditorCostumeToolView::CreateCostumeToolViewWarningSection()
{
	const TSharedRef<SWidget> WarningSectionWidget =
		SNew(SBox)
		.Padding(4.f)
		[
			SNew(SWarningOrErrorBox)
			.AutoWrapText(true)
			.MessageStyle(EMessageStyle::Warning)
			.Visibility(this, &SMetaHumanCharacterEditorCostumeToolView::GetWarningVisibility)
			.Message(LOCTEXT("CostumeToolViewWarningMessage","No items available. Please, select items from the Wardrobe to enable Costume editing."))
		];

	return WarningSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorCostumeToolView::CreateCostumeToolViewGroomsSection()
{
	UMetaHumanCharacterEditorCostumeToolProperties* CostumeToolProperties = Cast<UMetaHumanCharacterEditorCostumeToolProperties>(GetToolProperties());
	if (!CostumeToolProperties)
	{
		return SNullWidget::NullWidget;
	}

	const TSharedRef<SWidget> CostumeGroomsSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("CostumeGroomsSectionLabel", "Grooms"))
		.Visibility(this, &SMetaHumanCharacterEditorCostumeToolView::GetGroomsBoxVisibility)
		.Content()
		[
			SAssignNew(GroomsBox, SVerticalBox)
		];

	return CostumeGroomsSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorCostumeToolView::CreateCostumeToolViewOutfitClothingSection()
{
	UMetaHumanCharacterEditorCostumeToolProperties* CostumeToolProperties = Cast<UMetaHumanCharacterEditorCostumeToolProperties>(GetToolProperties());
	if (!CostumeToolProperties)
	{
		return SNullWidget::NullWidget;
	}

	const TSharedRef<SWidget> CostumeOutfitClothingSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("CostumeOutfitClothingSectionLabel", "Outfit Clothing"))
		.Visibility(this, &SMetaHumanCharacterEditorCostumeToolView::GetOutfitClothingBoxVisibility)
		.Content()
		[
			SAssignNew(OutfitClothingBox, SVerticalBox)
		];

	return CostumeOutfitClothingSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorCostumeToolView::CreateCostumeToolViewSkeletalMeshSection()
{
	UMetaHumanCharacterEditorCostumeToolProperties* CostumeToolProperties = Cast<UMetaHumanCharacterEditorCostumeToolProperties>(GetToolProperties());
	if (!CostumeToolProperties)
	{
		return SNullWidget::NullWidget;
	}

	const TSharedRef<SWidget> SkeletalMeshSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("CostumeSkelMeshClothingSectionLabel", "Skeletal Mesh"))
		.Visibility(this, &SMetaHumanCharacterEditorCostumeToolView::GetSkeletalMeshBoxVisibility)
		.Content()
		[
			SAssignNew(SkeletalMeshesBox, SVerticalBox)
		];

	return SkeletalMeshSectionWidget;
}

void SMetaHumanCharacterEditorCostumeToolView::MakeCostumeItemsBoxes()
{
	UMetaHumanCharacterEditorCostumeToolProperties* CostumeToolProperties = Cast<UMetaHumanCharacterEditorCostumeToolProperties>(GetToolProperties());
	if (!CostumeToolProperties || !GroomsBox.IsValid() || !OutfitClothingBox.IsValid())
	{
		return;
	}

	TArray<FMetaHumanCharacterEditorCostumeItem>& CostumeItems = CostumeToolProperties->CostumeItems;

	TMap<FName, TArray<FMetaHumanCharacterEditorCostumeItem*>> SlotNameToItemsMap;
	for (FMetaHumanCharacterEditorCostumeItem& Item : CostumeItems)
	{
		SlotNameToItemsMap.FindOrAdd(Item.SlotName).Add(&Item);
	}

	SlotNameToItemsMap.KeySort(FNameLexicalLess());

	for (const TPair<FName, TArray<FMetaHumanCharacterEditorCostumeItem*>>& SlotNameToItems : SlotNameToItemsMap)
	{
		const FName& SlotName = SlotNameToItems.Key;
		const TArray<FMetaHumanCharacterEditorCostumeItem*>& Items = SlotNameToItems.Value;
		if (SlotName == UE::MetaHuman::CharacterPipelineSlots::Character)
		{
			continue;
		}

		TSharedPtr<SVerticalBox> ItemsSlotBox = nullptr;
		for (FMetaHumanCharacterEditorCostumeItem* Item : Items)
		{
			if (!Item || !Item->WardrobeItem.IsValid())
			{
				continue;
			}

			FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			FDetailsViewArgs DetailsViewArgs;
			DetailsViewArgs.bUpdatesFromSelection = false;
			DetailsViewArgs.bLockable = false;
			DetailsViewArgs.bAllowSearch = false;
			DetailsViewArgs.NotifyHook = this;
			DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

			FStructureDetailsViewArgs StructureViewArgs;
			StructureViewArgs.bShowObjects = true;
			StructureViewArgs.bShowAssets = true;
			StructureViewArgs.bShowClasses = true;
			StructureViewArgs.bShowInterfaces = true;

			const TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(
				Item->InstanceParameters.GetPropertyBagStruct(), 
				Item->InstanceParameters.GetMutableValue().GetMemory());

			const TSharedRef<IStructureDetailsView> StructDetailsView = PropertyEditorModule.CreateStructureDetailView(
				DetailsViewArgs, 
				StructureViewArgs, 
				StructOnScope,
				LOCTEXT("CostumeOverridesCategoryName", "Overrides"));

			StructDetailsView->GetOnFinishedChangingPropertiesDelegate().AddWeakLambda(CostumeToolProperties,
				[CostumeToolProperties, StructOnScope, ItemPath = Item->ItemPath](const FPropertyChangedEvent& PropertyChangedEvent)
				{
					if (!CostumeToolProperties->Collection
						|| !PropertyChangedEvent.MemberProperty)
					{
						return;
					}

					// Override only the specific property that was changed, so that we're not 
					// saving default property values as overrides.
					{
						check(PropertyChangedEvent.MemberProperty->Owner == StructOnScope->GetStruct());

						FInstancedPropertyBag InstanceParameterBag;

						const EPropertyBagAlterationResult AddResult = InstanceParameterBag.AddProperty(
							PropertyChangedEvent.MemberProperty->GetFName(), 
							PropertyChangedEvent.MemberProperty);

						if (ensure(AddResult == EPropertyBagAlterationResult::Success))
						{
							const EPropertyBagResult SetResult = InstanceParameterBag.SetValue(
								PropertyChangedEvent.MemberProperty->GetFName(), 
								PropertyChangedEvent.MemberProperty,
								StructOnScope->GetStructMemory());

							if (ensure(SetResult == EPropertyBagResult::Success))
							{
								TNotNull<UMetaHumanCharacterInstance*> Instance = CostumeToolProperties->Collection->GetMutableDefaultInstance();
								
								const FScopedTransaction Transaction(LOCTEXT("ApplyOverrideInstanceParameter", "Apply Parameter"));
								Instance->Modify();
								Instance->OverrideInstanceParameters(ItemPath, InstanceParameterBag);
							}
						}
					}
				});

			if (!ItemsSlotBox.IsValid())
			{
				SAssignNew(ItemsSlotBox, SVerticalBox);
			}
			
			FText AssetName = FText::FromString(Item->WardrobeItem->PrincipalAsset.GetAssetName());
			if (!Item->WardrobeItem->ThumbnailName.IsEmpty())
			{
				AssetName = Item->WardrobeItem->ThumbnailName;
			}
			
			const FText SlotLabel = FText::Format(LOCTEXT("CostumeSlotLabelText", "{0} ({1})"), AssetName, FText::FromName(SlotName));
			ItemsSlotBox->AddSlot()
				.AutoHeight()
				.Padding(2.f, 4.f)
				[
					SNew(SMetaHumanCharacterEditorToolPanel)
					.Label(SlotLabel)
					.Content()
					[
						SNew(SBox)
						.Padding(6.f, 10.f)
						[
							StructDetailsView->GetWidget().ToSharedRef()
						]
					]
				];
		}

		if (!ItemsSlotBox.IsValid())
		{
			continue;
		}

		if (UE::MetaHuman::Private::SlotSupportsType<UGroomBindingAsset>(CostumeToolProperties->Collection, SlotName))
		{
			GroomsBox->AddSlot()
				.AutoHeight()
				.Padding(4.f, 6.f)
				[
					ItemsSlotBox.ToSharedRef()
				];
		}
		else if (UE::MetaHuman::Private::SlotSupportsType<UChaosOutfitAsset>(CostumeToolProperties->Collection, SlotName))
		{
			OutfitClothingBox->AddSlot()
				.AutoHeight()
				.Padding(4.f, 6.f)
				[
					ItemsSlotBox.ToSharedRef()
				];
		}
		else if (UE::MetaHuman::Private::SlotSupportsType<USkeletalMesh>(CostumeToolProperties->Collection, SlotName))
		{
			SkeletalMeshesBox->AddSlot()
				.AutoHeight()
				.Padding(4.f, 6.f)
				[
					ItemsSlotBox.ToSharedRef()
				];
		}
	}
}

EVisibility SMetaHumanCharacterEditorCostumeToolView::GetWarningVisibility() const
{
	const bool bIsVisible =
		GetGroomsBoxVisibility() == EVisibility::Collapsed
		&& GetOutfitClothingBoxVisibility() == EVisibility::Collapsed
		&& GetSkeletalMeshBoxVisibility() == EVisibility::Collapsed;
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMetaHumanCharacterEditorCostumeToolView::GetGroomsBoxVisibility() const
{
	const bool bIsVisible = GroomsBox.IsValid() && GroomsBox->NumSlots() > 0;
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMetaHumanCharacterEditorCostumeToolView::GetOutfitClothingBoxVisibility() const
{
	const bool bIsVisible = OutfitClothingBox.IsValid() && OutfitClothingBox->NumSlots() > 0;
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMetaHumanCharacterEditorCostumeToolView::GetSkeletalMeshBoxVisibility() const
{
	const bool bIsVisible = SkeletalMeshesBox.IsValid() && SkeletalMeshesBox->NumSlots() > 0;
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SMetaHumanCharacterEditorCostumeToolView::IsNameEnumValue(UEnum* EnumPtr, const FName& NameToCheck) const
{
	if (!EnumPtr)
	{
		return false;
	}

	const int32 NumEnums = EnumPtr->NumEnums();
	for (int32 Index = 0; Index < NumEnums - 1; ++Index)
	{
		FString EnumValueName = EnumPtr->GetDisplayNameTextByIndex(Index).ToString();
		if (EnumValueName == NameToCheck.ToString())
		{
			return true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
