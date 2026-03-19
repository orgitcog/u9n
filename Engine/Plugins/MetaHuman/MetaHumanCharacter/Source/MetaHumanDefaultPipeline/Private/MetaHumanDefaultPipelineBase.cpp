// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDefaultPipelineBase.h"

#include "Item/MetaHumanGroomPipeline.h"
#include "Item/MetaHumanSkeletalMeshPipeline.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanCollection.h"

#include "ChaosClothAsset/ClothAssetBase.h"
#include "Engine/SkeletalMesh.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "Algo/Transform.h"

#define LOCTEXT_NAMESPACE "MetaHumanDefaultPipelineBase"

UMetaHumanDefaultPipelineBase::UMetaHumanDefaultPipelineBase()
{
	// Initialize the specification
	{
		Specification = CreateDefaultSubobject<UMetaHumanCharacterPipelineSpecification>("Specification");
		Specification->AssemblyOutputStruct = FMetaHumanDefaultAssemblyOutput::StaticStruct();

		// Grooms
		{
			{
				FMetaHumanCharacterPipelineSlot& Slot = Specification->Slots.FindOrAdd("Hair");
				Slot.SupportedPrincipalAssetTypes.Add(UGroomBindingAsset::StaticClass());
				Slot.BuildOutputStruct = FMetaHumanGroomPipelineBuildOutput::StaticStruct();
			}

			{
				FMetaHumanCharacterPipelineSlot& Slot = Specification->Slots.FindOrAdd("Eyebrows");
				Slot.SupportedPrincipalAssetTypes.Add(UGroomBindingAsset::StaticClass());
				Slot.BuildOutputStruct = FMetaHumanGroomPipelineBuildOutput::StaticStruct();
			}

			{
				FMetaHumanCharacterPipelineSlot& Slot = Specification->Slots.FindOrAdd("Beard");
				Slot.SupportedPrincipalAssetTypes.Add(UGroomBindingAsset::StaticClass());
				Slot.BuildOutputStruct = FMetaHumanGroomPipelineBuildOutput::StaticStruct();
			}
		
			{
				FMetaHumanCharacterPipelineSlot& Slot = Specification->Slots.FindOrAdd("Mustache");
				Slot.SupportedPrincipalAssetTypes.Add(UGroomBindingAsset::StaticClass());
				Slot.BuildOutputStruct = FMetaHumanGroomPipelineBuildOutput::StaticStruct();
			}
		
			{
				FMetaHumanCharacterPipelineSlot& Slot = Specification->Slots.FindOrAdd("Eyelashes");
				Slot.SupportedPrincipalAssetTypes.Add(UGroomBindingAsset::StaticClass());
				Slot.BuildOutputStruct = FMetaHumanGroomPipelineBuildOutput::StaticStruct();
			}
				
			{
				FMetaHumanCharacterPipelineSlot& Slot = Specification->Slots.FindOrAdd("Peachfuzz");
				Slot.SupportedPrincipalAssetTypes.Add(UGroomBindingAsset::StaticClass());
				Slot.BuildOutputStruct = FMetaHumanGroomPipelineBuildOutput::StaticStruct();
			}
		}

		// Outfits
		{
			{
				FMetaHumanCharacterPipelineSlot& Slot = Specification->Slots.FindOrAdd("Outfits");
				Slot.SupportedPrincipalAssetTypes.Add(UChaosOutfitAsset::StaticClass());
				// This is hidden for now, since the UI doesn't support multi-select. We may expose it later.
				Slot.bVisibleToUser = false;
				Slot.bAllowsMultipleSelection = true;
			}

			{
				FMetaHumanCharacterPipelineSlot& Slot = Specification->Slots.FindOrAdd("Top Garment");
				Slot.SupportedPrincipalAssetTypes.Add(UChaosOutfitAsset::StaticClass());
				Slot.TargetSlot = "Outfits";
			}

			{
				FMetaHumanCharacterPipelineSlot& Slot = Specification->Slots.FindOrAdd("Bottom Garment");
				Slot.SupportedPrincipalAssetTypes.Add(UChaosOutfitAsset::StaticClass());
				Slot.TargetSlot = "Outfits";
			}
		}

		// Skeletal meshes
		{
			{
				FMetaHumanCharacterPipelineSlot& Slot = Specification->Slots.FindOrAdd("SkeletalMesh");
				Slot.SupportedPrincipalAssetTypes.Add(USkeletalMesh::StaticClass());
				Slot.BuildOutputStruct = FMetaHumanMeshPartOutput::StaticStruct();
				Slot.bAllowsMultipleSelection = true;
			}
		}

		// Character
		{
			FMetaHumanCharacterPipelineSlot& Slot = Specification->Slots.FindOrAdd(UE::MetaHuman::CharacterPipelineSlots::Character);
			Slot.SupportedPrincipalAssetTypes.Add(UMetaHumanCharacter::StaticClass());
		}
	}
}

void UMetaHumanDefaultPipelineBase::AssembleCollection(
	TNotNull<const UMetaHumanCollection*> Collection,
	EMetaHumanCharacterPaletteBuildQuality Quality,
	const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections,
	const FInstancedStruct& AssemblyInput,
	TNotNull<UObject*> OuterForGeneratedObjects,
	const FOnAssemblyComplete& OnComplete) const
{
	if (!Collection->GetBuiltData(Quality).IsValid())
	{
		OnComplete.ExecuteIfBound(FMetaHumanAssemblyOutput());
		return;
	}

	const FMetaHumanCollectionBuiltData& BuildOutput = Collection->GetBuiltData(Quality);

	FMetaHumanAssemblyOutput AssemblyOutput;
	FMetaHumanDefaultAssemblyOutput& AssemblyStruct = AssemblyOutput.PipelineAssemblyOutput.InitializeAs<FMetaHumanDefaultAssemblyOutput>();

	// Character slot
	FMetaHumanPaletteItemKey SelectedCharacterItem;
	{
		const FName SlotName = UE::MetaHuman::CharacterPipelineSlots::Character;
		
		if (UMetaHumanCharacterInstance::TryGetAnySlotSelection(SlotSelections, SlotName, SelectedCharacterItem))
		{
			const FMetaHumanPipelineBuiltData* BuildOutputForSlot = BuildOutput.PaletteBuiltData.ItemBuiltData.Find(FMetaHumanPaletteItemPath(SelectedCharacterItem));

			if (BuildOutputForSlot
				&& BuildOutputForSlot->BuildOutput.GetPtr<FMetaHumanCharacterPartOutput>())
			{
				const FMetaHumanCharacterPartOutput& PartOutput = BuildOutputForSlot->BuildOutput.Get<FMetaHumanCharacterPartOutput>();

				AssemblyStruct.FaceMesh = PartOutput.GeneratedAssets.FaceMesh;
				AssemblyStruct.BodyMesh = PartOutput.GeneratedAssets.BodyMesh;
				AssemblyOutput.Metadata.Append(PartOutput.GeneratedAssets.Metadata);
			}
		}
	}

	// Same as AssembleMeshPart but for grooms
	auto AssembleGroomPart = [Collection, &BuildOutput, &SlotSelections, &AssemblyStruct, &AssemblyOutput, OuterForGeneratedObjects](
		const FName SlotName,
		FMetaHumanGroomPipelineAssemblyOutput FMetaHumanDefaultAssemblyOutput::* AssemblyOutputMember)
	{
		// Don't call this if there's no FaceMesh
		check(AssemblyStruct.FaceMesh);

		FMetaHumanPaletteItemKey ItemKey;
		if (UMetaHumanCharacterInstance::TryGetAnySlotSelection(SlotSelections, SlotName, ItemKey))
		{
			const FMetaHumanPaletteItemPath ItemPath(ItemKey);
			if (BuildOutput.PaletteBuiltData.ItemBuiltData.Contains(ItemPath))
			{
				const UMetaHumanItemPipeline* ItemPipeline = nullptr;
				if (!Collection->TryResolveItemPipeline(ItemPath, ItemPipeline))
				{
					ItemPipeline = GetDefault<UMetaHumanGroomPipeline>();
				}

				FInstancedStruct ItemAssemblyInput;
				FMetaHumanGroomPipelineAssemblyInput& GroomAssemblyInput = ItemAssemblyInput.InitializeAs<FMetaHumanGroomPipelineAssemblyInput>();
				GroomAssemblyInput.TargetMesh = AssemblyStruct.FaceMesh;

				// TODO: Check that slot and item struct types match

				FMetaHumanAssemblyOutput ItemAssemblyOutput;
				ItemPipeline->AssembleItemSynchronous(
					ItemPath,
					// Sub-item selections not supported yet
					TArray<FMetaHumanPipelineSlotSelectionData>(),
					BuildOutput.PaletteBuiltData,
					ItemAssemblyInput,
					OuterForGeneratedObjects,
					ItemAssemblyOutput);

				if (const FMetaHumanGroomPipelineAssemblyOutput* GroomAssemblyOutput = ItemAssemblyOutput.PipelineAssemblyOutput.GetPtr<FMetaHumanGroomPipelineAssemblyOutput>())
				{
					AssemblyStruct.*AssemblyOutputMember = *GroomAssemblyOutput;
					
					AssemblyOutput.Metadata.Append(MoveTemp(ItemAssemblyOutput.Metadata));
					AssemblyOutput.InstanceParameters.Append(MoveTemp(ItemAssemblyOutput.InstanceParameters));
				}
			}
		}
	};

	if (AssemblyStruct.FaceMesh)
	{
		AssembleGroomPart(TEXT("Hair"), &FMetaHumanDefaultAssemblyOutput::Hair);
		AssembleGroomPart(TEXT("Eyebrows"), &FMetaHumanDefaultAssemblyOutput::Eyebrows);
		AssembleGroomPart(TEXT("Beard"), &FMetaHumanDefaultAssemblyOutput::Beard);
		AssembleGroomPart(TEXT("Mustache"), &FMetaHumanDefaultAssemblyOutput::Mustache);
		AssembleGroomPart(TEXT("Eyelashes"), &FMetaHumanDefaultAssemblyOutput::Eyelashes);
		AssembleGroomPart(TEXT("Peachfuzz"), &FMetaHumanDefaultAssemblyOutput::Peachfuzz);
	}

	// Finds all item paths for the given slot name
	auto GetItemPaths = [this, &SlotSelections](const FName& SlotName)
	{
		TArray<FMetaHumanPaletteItemPath> ItemPaths;

		if (const FMetaHumanCharacterPipelineSlot* FoundSlot = Specification->Slots.Find(SlotName))
		{
			if (FoundSlot->bAllowsMultipleSelection)
			{
				Algo::TransformIf(
					SlotSelections,
					ItemPaths,
					[SlotName](const FMetaHumanPipelineSlotSelectionData& Selection)
					{
						return Selection.Selection.SlotName == SlotName;
					},
					[](const FMetaHumanPipelineSlotSelectionData& Selection)
					{
						return FMetaHumanPaletteItemPath(Selection.Selection.SelectedItem);
					});
			}
			else
			{
				FMetaHumanPaletteItemKey ItemKey;

				if (UMetaHumanCharacterInstance::TryGetAnySlotSelection(SlotSelections, SlotName, ItemKey))
				{
					ItemPaths.Add(FMetaHumanPaletteItemPath(ItemKey));
				}
			}
		}
		return ItemPaths;
	};

	// Handle Outfits slot
	{
		const TArray<FMetaHumanPaletteItemPath> ItemPaths = GetItemPaths("Outfits");

		for (const FMetaHumanPaletteItemPath& ItemPath : ItemPaths)
		{
			const FMetaHumanPipelineBuiltData* BuildOutputForSlot = BuildOutput.PaletteBuiltData.ItemBuiltData.Find(ItemPath);

			if (BuildOutputForSlot
				&& BuildOutputForSlot->BuildOutput.GetPtr<FMetaHumanOutfitPipelineBuildOutput>())
			{
				const UMetaHumanItemPipeline* ItemPipeline = nullptr;
				if (!Collection->TryResolveItemPipeline(ItemPath, ItemPipeline))
				{
					ItemPipeline = GetDefault<UMetaHumanOutfitPipeline>();
				}

				FInstancedStruct ItemAssemblyInput;
				FMetaHumanOutfitPipelineAssemblyInput& OutfitAssemblyInput = ItemAssemblyInput.InitializeAs<FMetaHumanOutfitPipelineAssemblyInput>();
				OutfitAssemblyInput.SelectedCharacter = SelectedCharacterItem;

				// TODO: Check that slot and item struct types match

				FMetaHumanAssemblyOutput ItemAssemblyOutput;
				ItemPipeline->AssembleItemSynchronous(
					ItemPath,
					// Sub-item selections not supported yet
					TArray<FMetaHumanPipelineSlotSelectionData>(),
					BuildOutput.PaletteBuiltData,
					ItemAssemblyInput,
					OuterForGeneratedObjects,
					ItemAssemblyOutput);

				if (const FMetaHumanOutfitPipelineAssemblyOutput* OutfitAssemblyOutput = ItemAssemblyOutput.PipelineAssemblyOutput.GetPtr<FMetaHumanOutfitPipelineAssemblyOutput>())
				{
					AssemblyStruct.ClothData.Add(*OutfitAssemblyOutput);

					AssemblyOutput.Metadata.Append(MoveTemp(ItemAssemblyOutput.Metadata));
					AssemblyOutput.InstanceParameters.Append(MoveTemp(ItemAssemblyOutput.InstanceParameters));
				}
			}
		}
	}

	// Assemble Skeletal Mesh clothing
	{
		const TArray<FMetaHumanPaletteItemPath> ItemPaths = GetItemPaths("SkeletalMesh");

		for (const FMetaHumanPaletteItemPath& ItemPath : ItemPaths)
		{
			const FMetaHumanPipelineBuiltData* BuildOutputForSlot = BuildOutput.PaletteBuiltData.ItemBuiltData.Find(ItemPath);

			if (!BuildOutputForSlot)
			{
				return;
			}

			if (const FMetaHumanMeshPartOutput* MeshPartOutput = BuildOutputForSlot->BuildOutput.GetPtr<FMetaHumanMeshPartOutput>())
			{
				if (!MeshPartOutput->Mesh)
				{
					return;
				}

				const UMetaHumanItemPipeline* ItemPipeline = nullptr;
				if (!Collection->TryResolveItemPipeline(ItemPath, ItemPipeline))
				{
					ItemPipeline = GetDefault<UMetaHumanSkeletalMeshPipeline>();
				}

				FInstancedStruct ItemAssemblyInput;
				FMetaHumanSkeletalMeshPipelineAssemblyInput& SkeletalMeshAssemblyInput = ItemAssemblyInput.InitializeAs<FMetaHumanSkeletalMeshPipelineAssemblyInput>();
				SkeletalMeshAssemblyInput.TargetMesh = MeshPartOutput->Mesh;

				// TODO: Check that slot and item struct types match

				FMetaHumanAssemblyOutput ItemAssemblyOutput;
				ItemPipeline->AssembleItemSynchronous(
					ItemPath,
					// Sub-item selections not supported yet
					TArray<FMetaHumanPipelineSlotSelectionData>(),
					BuildOutput.PaletteBuiltData,
					ItemAssemblyInput,
					OuterForGeneratedObjects,
					ItemAssemblyOutput);


				if (const FMetaHumanSkeletalMeshPipelineAssemblyOutput* SkeletalMeshAssemblyOutput = ItemAssemblyOutput.PipelineAssemblyOutput.GetPtr<FMetaHumanSkeletalMeshPipelineAssemblyOutput>())
				{
					AssemblyOutput.Metadata.Append(MoveTemp(ItemAssemblyOutput.Metadata));
					AssemblyOutput.InstanceParameters.Append(MoveTemp(ItemAssemblyOutput.InstanceParameters));

					AssemblyStruct.SkeletalMeshData.Add(*SkeletalMeshAssemblyOutput);
				}
			}
		}
	}

	OnComplete.ExecuteIfBound(MoveTemp(AssemblyOutput));
}

const UMetaHumanItemPipeline* UMetaHumanDefaultPipelineBase::GetFallbackItemPipelineForAssetType(const TSoftClassPtr<UObject>& InAssetClass) const
{
	if (const TSubclassOf<UMetaHumanItemPipeline>* FoundPipelineClass = DefaultAssetPipelines.Find(InAssetClass))
	{
		if (*FoundPipelineClass)
		{
			return Cast<UMetaHumanItemPipeline>(FoundPipelineClass->GetDefaultObject());
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
