// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "Tests/MetaHumanCharacterTestPipeline.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"

struct FNameLexicalNotLess
{
	bool operator()(const FName& A, const FName& B) const
	{
		return A.Compare(B) >= 0;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMetaHumanCharacterPipelineSpecificationTest, 
	"MetaHuman.Creator.Pipeline.Specification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FMetaHumanCharacterPipelineSpecificationTest::RunTest(const FString& InParams)
{
	// Test basic functionality of virtual slots and supported asset types
	{
		UMetaHumanCharacterPipelineSpecification* Spec = NewObject<UMetaHumanCharacterPipelineSpecification>();

		{
			FMetaHumanCharacterPipelineSlot& Slot = Spec->Slots.Add("VirtualA");
			// This slot can support a different type from A, as long as it inherits from one of 
			// A's supported types.
			Slot.SupportedPrincipalAssetTypes.Add(USkeletalMesh::StaticClass());
			Slot.TargetSlot = "A";

			UTEST_TRUE("A slot with a target slot is virtual", Slot.IsVirtual());
		}

		UTEST_FALSE("The slot targeted by a virtual slot must exist", Spec->IsValid());

		{
			FMetaHumanCharacterPipelineSlot& Slot = Spec->Slots.Add("A");
			Slot.SupportedPrincipalAssetTypes.Add(USkinnedAsset::StaticClass());
			Slot.SupportedPrincipalAssetTypes.Add(UStaticMesh::StaticClass());

			UTEST_FALSE("A slot without a target slot is real", Slot.IsVirtual());
		}
		
		UTEST_TRUE("A virtual slot can support a principal asset of a derived type of the target slot", Spec->IsValid());

		{
			FMetaHumanCharacterPipelineSlot& Slot = Spec->Slots.Add("VirtualVirtualA");
			Slot.SupportedPrincipalAssetTypes.Add(USkeletalMesh::StaticClass());
			Slot.TargetSlot = "VirtualA";

			UTEST_TRUE("A virtual slot can target a virtual slot", Spec->IsValid());

			Slot.SupportedPrincipalAssetTypes.Reset();
			Slot.SupportedPrincipalAssetTypes.Add(USkinnedAsset::StaticClass());

			UTEST_FALSE("A virtual slot may only support principal assets that are supported by the target slot, "
				"even if the underlying real slot could support other types", Spec->IsValid());
		}
	}

	// Test multiple selection rules
	{
		UMetaHumanCharacterPipelineSpecification* Spec = NewObject<UMetaHumanCharacterPipelineSpecification>();

		{
			FMetaHumanCharacterPipelineSlot& SlotA = Spec->Slots.Add("A");
			SlotA.SupportedPrincipalAssetTypes.Add(USkinnedAsset::StaticClass());
			SlotA.bAllowsMultipleSelection = false;
		}

		{
			FMetaHumanCharacterPipelineSlot& SlotVirtualA = Spec->Slots.Add("VirtualA");
			SlotVirtualA.SupportedPrincipalAssetTypes.Add(USkinnedAsset::StaticClass());
			SlotVirtualA.TargetSlot = "A";
			SlotVirtualA.bAllowsMultipleSelection = false;

			UTEST_TRUE("A virtual slot may target a slot if neither of them allow multiple selection", Spec->IsValid());

			SlotVirtualA.bAllowsMultipleSelection = true;

			UTEST_FALSE("A virtual slot may not allow multiple selection if its target doesn't", Spec->IsValid());

			Spec->Slots["A"].bAllowsMultipleSelection = true;

			UTEST_TRUE("A virtual slot may target a slot if both of them allow multiple selection", Spec->IsValid());
						
			SlotVirtualA.bAllowsMultipleSelection = false;

			UTEST_TRUE("A virtual slot that doesn't allow multiple selection may target a slot that does", Spec->IsValid());
		}
	}

	// Test cycle detection
	{
		UMetaHumanCharacterPipelineSpecification* Spec = NewObject<UMetaHumanCharacterPipelineSpecification>();
		
		{
			FMetaHumanCharacterPipelineSlot& SlotA = Spec->Slots.Add("A");
			SlotA.SupportedPrincipalAssetTypes.Add(USkinnedAsset::StaticClass());
			SlotA.TargetSlot = "A";

			UTEST_FALSE("A slot may not target itself", Spec->IsValid());

			SlotA.TargetSlot = NAME_None;
		}

		{
			FMetaHumanCharacterPipelineSlot& SlotVirtualA = Spec->Slots.Add("VirtualA");
			SlotVirtualA.SupportedPrincipalAssetTypes.Add(USkinnedAsset::StaticClass());
			SlotVirtualA.TargetSlot = "A";
			Spec->Slots["A"].TargetSlot = "VirtualA";

			UTEST_FALSE("A chain of virtual slots must terminate in a real slot", Spec->IsValid());

			Spec->Slots["A"].TargetSlot = NAME_None;
		}

		{
			FMetaHumanCharacterPipelineSlot& SlotB = Spec->Slots.Add("B");
			SlotB.SupportedPrincipalAssetTypes.Add(USkeletalMesh::StaticClass());
			SlotB.TargetSlot = "A";
		}

		UTEST_FALSE("Multiple virtual slots may not target the same real slot if it doesn't allow multiple selection", Spec->IsValid());

		Spec->Slots["A"].bAllowsMultipleSelection = true;

		UTEST_TRUE("Multiple virtual slots may target the same real slot", Spec->IsValid());

		{
			FMetaHumanCharacterPipelineSlot& SlotC = Spec->Slots.Add("C");
			SlotC.SupportedPrincipalAssetTypes.Add(USkeletalMesh::StaticClass());
			SlotC.TargetSlot = "B";
		}

		{
			FMetaHumanCharacterPipelineSlot& SlotD = Spec->Slots.Add("D");
			SlotD.SupportedPrincipalAssetTypes.Add(USkeletalMesh::StaticClass());
			SlotD.TargetSlot = "C";
		}

		UTEST_TRUE("Longer virtual slot chains are allowed", Spec->IsValid());

		Spec->Slots["A"].TargetSlot = "D";

		UTEST_FALSE("Cycles are detected in longer virtual slot chains", Spec->IsValid());

		Spec->Slots["A"].TargetSlot = "C";

		UTEST_FALSE("Cycles are detected in part of a longer virtual slot chain", Spec->IsValid());
		
		Spec->Slots.KeySort(FNameLexicalLess());

		UTEST_FALSE("Cycles are detected when slots are in ascending alphabetical order", Spec->IsValid());

		Spec->Slots.KeySort(FNameLexicalNotLess());

		UTEST_FALSE("Cycles are detected when slots are in descending alphabetical order", Spec->IsValid());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMetaHumanCharacterCollectionTest, 
	"MetaHuman.Creator.Collection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FMetaHumanCharacterCollectionTest::RunTest(const FString& InParams)
{
	const FName SlotA("A");

	UMetaHumanCollection* Collection = NewObject<UMetaHumanCollection>();

	UMetaHumanCharacterPipelineSpecification* Spec = NewObject<UMetaHumanCharacterPipelineSpecification>();
	{
		FMetaHumanCharacterPipelineSlot& Slot = Spec->Slots.Add(SlotA);
		Slot.SupportedPrincipalAssetTypes.Add(USkeletalMesh::StaticClass());
	}
	
	UMetaHumanCharacterTestPipeline* Pipeline = NewObject<UMetaHumanCharacterTestPipeline>();
	Pipeline->SetSpecification(Spec);
	Collection->SetPipeline(Pipeline);

	const USkeletalMesh* Asset = NewObject<USkeletalMesh>();
	const FAssetData AssetData(Asset);

	UTEST_TRUE("Test pipeline accepts an asset of the correct type", Pipeline->GetEditorPipeline()->IsPrincipalAssetClassCompatibleWithSlot(SlotA, Asset->GetClass()));

	FMetaHumanPaletteItemKey AssetItemKey;
	UTEST_TRUE("An item may be added to a collection from its principal asset", Collection->TryAddItemFromPrincipalAsset(SlotA, Asset, AssetItemKey));
	FMetaHumanCharacterPaletteItem Item;
	UTEST_TRUE("An item that exists can be found from its key", Collection->TryFindItem(AssetItemKey, Item));
	UTEST_SAME_PTR("Test pipeline sets the principal asset correctly on a new item", Item.LoadPrincipalAssetSynchronous(), CastChecked<UObject>(Asset));
	UTEST_EQUAL("Test pipeline sets the slot name correctly on a new item", Item.SlotName, SlotA);

	UTEST_FALSE("The exact same item may not be added twice", Collection->TryAddItem(Item));

	const FName FirstGeneratedVariation = Collection->GenerateUniqueVariationName(Item.GetItemKey());
	UTEST_NOT_EQUAL("A variation change is suggested when adding the same item twice", FirstGeneratedVariation, Item.Variation);

	// Note that this doesn't modify the item already added to the collection, because TryAddItem
	// takes a copy of the item.
	Item.Variation = FirstGeneratedVariation;

	UTEST_TRUE("The same item may be added with a variation change", Collection->TryAddItem(Item));

	const FName SecondGeneratedVariation = Collection->GenerateUniqueVariationName(Item.GetItemKey());
	UTEST_NOT_EQUAL("The third matching item to be added is given a different variation from the original item", SecondGeneratedVariation, Item.Variation);
	UTEST_NOT_EQUAL("The third matching item to be added is given a different variation from the second item", SecondGeneratedVariation, FirstGeneratedVariation);

	Item.Variation = SecondGeneratedVariation;

	UTEST_TRUE("The third matching item may be added with a variation change", Collection->TryAddItem(Item));

	Collection->RemoveAllItemsForSlot(SlotA);
	UTEST_EQUAL("Removing all items for the only slot removes all items in the collection", Collection->GetItems().Num(), 0);

	Item.Variation = NAME_None;
	UTEST_TRUE("Add item with no variation", Collection->TryAddItem(Item));

	Item.Variation.SetNumber(1);
	UTEST_TRUE("Add item with variation number 1", Collection->TryAddItem(Item));

	Item.Variation.SetNumber(2);
	UTEST_TRUE("Add item with variation number 2", Collection->TryAddItem(Item));

	Item.Variation.SetNumber(MAX_int32);
	UTEST_TRUE("Add item with variation number MAX_int32", Collection->TryAddItem(Item));
	
	const FName MaxIntGeneratedVariation = Collection->GenerateUniqueVariationName(Item.GetItemKey());
	Item.Variation = MaxIntGeneratedVariation;
	UTEST_FALSE("The variation generator can successfully generate a unique item variation starting from MAX_int32", Collection->ContainsItem(Item.GetItemKey()));
	UTEST_TRUE("An item can be added to a collection that doesn't already contain it", Collection->TryAddItem(Item));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
