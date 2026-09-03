// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCollection.h"

#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterPaletteLog.h"
#include "MetaHumanCharacterPaletteProjectSettings.h"
#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "MetaHumanCollectionPipeline.h"
#include "MetaHumanItemPipeline.h"
#include "MetaHumanWardrobeItem.h"

#include "Logging/StructuredLog.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

bool FMetaHumanCollectionBuiltData::IsValid() const
{
	return PaletteBuiltData.ItemBuiltData.Num() > 0;
}

UMetaHumanCollection::UMetaHumanCollection()
{
	DefaultInstance = CreateDefaultSubobject<UMetaHumanCharacterInstance>(TEXT("DefaultInstance"));
	// Allow the Default Instance to be referenced from other packages, such as actors in a level
	DefaultInstance->SetFlags(RF_Public);
	DefaultInstance->SetMetaHumanCollection(this);
}


#if WITH_EDITOR
void UMetaHumanCollection::Build(
	const FInstancedStruct& BuildInput,
	EMetaHumanCharacterPaletteBuildQuality Quality,
	ITargetPlatform* TargetPlatform,
	const FOnBuildComplete& OnComplete,
	const TArray<FMetaHumanPinnedSlotSelection>& PinnedSlotSelections,
	const TArray<FMetaHumanPaletteItemPath>& ItemsToExclude)
{
	if (!Pipeline
		|| !Pipeline->GetEditorPipeline())
	{
		OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed);
		return;
	}

	TArray<FMetaHumanPaletteItemPath> LocalItemsToExclude;
	LocalItemsToExclude.Reserve(ItemsToExclude.Num());

	// Any invalid pinned slot selections detected below will be treated as a build failure, 
	// because they could have significant downstream effects that are hard to detect later, e.g. a
	// large amount of content being unintentionally built.
	{
		for (int32 Index = 0; Index < PinnedSlotSelections.Num(); Index++)
		{
			const FMetaHumanPinnedSlotSelection& PinnedSelection = PinnedSlotSelections[Index];
			if (PinnedSelection.Selection.SlotName == NAME_None)
			{
				OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed);
				return;
			}

			// Find out if this pinned slot has already been processed
			{
				bool bAlreadyProcessed = false;
				for (int32 CompareIndex = Index - 1; CompareIndex >= 0; CompareIndex--)
				{
					const FMetaHumanPinnedSlotSelection& CompareSelection = PinnedSlotSelections[CompareIndex];
					if (CompareSelection.Selection.ParentItemPath == PinnedSelection.Selection.ParentItemPath
						&& CompareSelection.Selection.SlotName == PinnedSelection.Selection.SlotName)
					{
						bAlreadyProcessed = true;
						break;
					}
				}

				if (bAlreadyProcessed)
				{
					continue;
				}
			}

			const UMetaHumanCharacterPalette* ContainingPalette = nullptr;
			{
				if (PinnedSelection.Selection.ParentItemPath.IsEmpty())
				{
					ContainingPalette = this;
				}
				else
				{
					// TODO: Support nested items. These are currently not possible to create, but we want to support them in future.
					OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed);
					return;
				}
			}

			for (const FMetaHumanCharacterPaletteItem& Item : ContainingPalette->GetItems())
			{
				if (Item.SlotName != PinnedSelection.Selection.SlotName)
				{
					continue;
				}

				const FMetaHumanPaletteItemKey ItemKey = Item.GetItemKey();
				
				if (!PinnedSlotSelections.ContainsByPredicate(
					[&ItemKey, &PinnedSelection](const FMetaHumanPinnedSlotSelection& OtherPinnedSelection)
					{
						return OtherPinnedSelection.Selection.ParentItemPath == PinnedSelection.Selection.ParentItemPath
							&& OtherPinnedSelection.Selection.SlotName == PinnedSelection.Selection.SlotName
							&& OtherPinnedSelection.Selection.SelectedItem == ItemKey;
					}))
				{
					// This item is in the same slot as the pinned item, but is not itself pinned

					// Since each pinned slot is only processed once and each item can only be in one slot,
					// there should be no duplicates in this list.
					LocalItemsToExclude.Emplace(PinnedSelection.Selection.ParentItemPath, ItemKey);
				}
			}
		}
	}

	if (LocalItemsToExclude.Num() > 0)
	{
		for (const FMetaHumanPaletteItemPath& Item : ItemsToExclude)
		{
			LocalItemsToExclude.AddUnique(Item);
		}
	}
	else
	{
		LocalItemsToExclude = ItemsToExclude;
	}

	LocalItemsToExclude.Sort();

	TArray<FMetaHumanPinnedSlotSelection> SortedPinnedSlotSelections = PinnedSlotSelections;
	SortedPinnedSlotSelections.Sort([](const FMetaHumanPinnedSlotSelection& A, const FMetaHumanPinnedSlotSelection& B) { return A.Selection < B.Selection; });

	UObject* OuterForGeneratedObjects = this;
	// we use the transient package for the outer in preview builds, to avoid dirtying the collection
	if (Quality == EMetaHumanCharacterPaletteBuildQuality::Preview)
	{
		OuterForGeneratedObjects = GetTransientPackage();
	}


	Pipeline->GetEditorPipeline()->BuildCollection(
		this,
		OuterForGeneratedObjects,
		SortedPinnedSlotSelections,
		LocalItemsToExclude,
		BuildInput,
		Quality,
		TargetPlatform,
		UMetaHumanCollectionEditorPipeline::FOnBuildComplete::CreateWeakLambda(this, 
			[this, OnComplete, TargetPlatform, Quality, SortedPinnedSlotSelections](EMetaHumanBuildStatus Status, TSharedPtr<FMetaHumanCollectionBuiltData> BuiltData)
			{
				if (BuiltData.IsValid())
				{
					// Overwrite these to ensure they're set to the values that were passed into 
					// BuildCollection.
					BuiltData->Quality = Quality;
					// Note that SortedPinnedSlotSelections may reference UObjects, but is not 
					// visible to the GC while stored in the lambda capture. This will need to be 
					// addressed when we make building properly async.
					BuiltData->SortedPinnedSlotSelections = SortedPinnedSlotSelections;

					SetBuiltData(Quality, MoveTemp(*BuiltData));
				}

				OnComplete.ExecuteIfBound(Status);

				if (Status == EMetaHumanBuildStatus::Succeeded)
				{
					OnPaletteBuilt.Broadcast(Quality);
				}
			}));
}

void UMetaHumanCollection::UnpackAssets(const FOnMetaHumanCharacterAssetsUnpacked& OnComplete)
{
	if (!Pipeline
		|| !Pipeline->GetEditorPipeline())
	{
		OnComplete.ExecuteIfBound(EMetaHumanCharacterAssetsUnpackResult::Failed);
		return;
	}

	Pipeline->GetEditorPipeline()->UnpackCollectionAssets(this, ProductionBuiltData, UMetaHumanCharacterEditorPipeline::FOnUnpackComplete::CreateWeakLambda(
		this,
		[OnComplete, this](EMetaHumanBuildStatus Result)
		{
			if (Result == EMetaHumanBuildStatus::Failed)
			{
				OnComplete.ExecuteIfBound(EMetaHumanCharacterAssetsUnpackResult::Failed);
				return;
			}

			bIsUnpacked = true;

			OnComplete.ExecuteIfBound(EMetaHumanCharacterAssetsUnpackResult::Succeeded);
		}));
}

void UMetaHumanCollection::SetDefaultPipeline()
{
	// If this is a blueprint class, the project code should load it at startup to avoid a hitch here.
	TSubclassOf<UMetaHumanCollectionPipeline> PipelineClass = GetDefault<UMetaHumanCharacterPaletteProjectSettings>()->DefaultCharacterPipelineClass.LoadSynchronous();

	if (PipelineClass)
	{
		SetPipeline(NewObject<UMetaHumanCollectionPipeline>(this, PipelineClass));
	}
	else
	{
		UE_LOGFMT(LogMetaHumanCharacterPalette, Error,
			"Failed to load DefaultCharacterPipelineClass: {DefaultClass}", 
			GetDefault<UMetaHumanCharacterPaletteProjectSettings>()->DefaultCharacterPipelineClass.ToString());
	}
}

void UMetaHumanCollection::SetPipeline(TNotNull<UMetaHumanCollectionPipeline*> InPipeline)
{		
	Pipeline = InPipeline;

	// It's not always possible for a pipeline to initialize its own editor pipeline when it's 
	// constructed, e.g. if it's in an editor module that the runtime pipeline can't depend on,
	// so we create a default editor pipeline here if one isn't already set.
	//
	// We could require callers to do this instead, but that is more error prone and doesn't have
	// any benefits other than being conceptually more correct.
	if (!Pipeline->GetEditorPipeline())
	{
		Pipeline->SetDefaultEditorPipeline();
	}

	// TODO: Delete any items belonging to slots that don't exist on the new pipeline

	OnPipelineChanged.Broadcast();
}

void UMetaHumanCollection::SetPipelineFromClass(TSubclassOf<UMetaHumanCollectionPipeline> InPipelineClass)
{
	if (InPipelineClass)
	{
		SetPipeline(NewObject<UMetaHumanCollectionPipeline>(this, InPipelineClass));
	}
}

const UMetaHumanCollectionEditorPipeline* UMetaHumanCollection::GetEditorPipeline() const
{
	return Pipeline ? Pipeline->GetEditorPipeline() : nullptr;
}

const UMetaHumanCharacterEditorPipeline* UMetaHumanCollection::GetPaletteEditorPipeline() const
{
	return GetEditorPipeline();
}

#endif // WITH_EDITOR

UMetaHumanCollectionPipeline* UMetaHumanCollection::GetMutablePipeline()
{
	return Pipeline;
}
	
const UMetaHumanCollectionPipeline* UMetaHumanCollection::GetPipeline() const
{
	return Pipeline;
}

const UMetaHumanCharacterPipeline* UMetaHumanCollection::GetPalettePipeline() const
{
	return GetPipeline();
}

const FMetaHumanCollectionBuiltData& UMetaHumanCollection::GetBuiltData(EMetaHumanCharacterPaletteBuildQuality Quality) const
{
#if WITH_EDITORONLY_DATA
	switch (Quality)
	{
		case EMetaHumanCharacterPaletteBuildQuality::Production:
			return ProductionBuiltData;

		case EMetaHumanCharacterPaletteBuildQuality::Preview:
			return PreviewBuiltData;

		default:
			checkNoEntry();
	}
#else
	check(Quality == EMetaHumanCharacterPaletteBuildQuality::Production);
#endif

	return ProductionBuiltData;
}

TNotNull<UMetaHumanCharacterInstance*> UMetaHumanCollection::GetMutableDefaultInstance()
{
	return DefaultInstance;
}

TNotNull<const UMetaHumanCharacterInstance*> UMetaHumanCollection::GetDefaultInstance() const
{
	return DefaultInstance;
}

#if WITH_EDITORONLY_DATA
void UMetaHumanCollection::SetBuiltData(EMetaHumanCharacterPaletteBuildQuality Quality, FMetaHumanCollectionBuiltData&& Data)
{
	switch (Quality)
	{
		case EMetaHumanCharacterPaletteBuildQuality::Production:
			ProductionBuiltData = MoveTemp(Data);
			break;

		case EMetaHumanCharacterPaletteBuildQuality::Preview:
			PreviewBuiltData = MoveTemp(Data);
			break;

		default:
			checkNoEntry();
	}
}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR

void UMetaHumanCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaHumanCollection, Pipeline))
	{
		SetPipeline(Pipeline);
	}
}

#endif // WITH_EDITOR

TArray<FMetaHumanPipelineSlotSelectionData> UMetaHumanCollection::PropagateVirtualSlotSelections(const TArray<FMetaHumanPipelineSlotSelectionData>& Selections) const
{
	TArray<FMetaHumanPipelineSlotSelectionData> Result;
	Result.Reserve(Selections.Num());

	for (const FMetaHumanPipelineSlotSelectionData& SelectionData : Selections)
	{
		const UMetaHumanCharacterPalette* ContainingPalette = nullptr;
		FMetaHumanCharacterPaletteItem Item;
		if (!TryResolveItem(SelectionData.Selection.GetSelectedItemPath(), ContainingPalette, Item))
		{
			// This selection will be dropped from the result and only the valid selections will be returned
			continue;
		}

		if (!Item.WardrobeItem
			|| !Item.WardrobeItem->PrincipalAsset)
		{
			// Drop the selection if the item isn't valid
			continue;
		}

		const UMetaHumanCharacterPipeline* ParentPipeline = ContainingPalette->GetPalettePipeline();
		TNotNull<const UMetaHumanCharacterPipelineSpecification*> PipelineSpec = ParentPipeline->GetSpecification();

		TOptional<FName> ResolvedSlotName = PipelineSpec->ResolveRealSlotName(SelectionData.Selection.SlotName);
		if (!ResolvedSlotName.IsSet())
		{
			UE_LOGFMT(LogMetaHumanCharacterPalette, Error, "Failed to resolve virtual slot {VirtualSlot} to a real slot on specification {PipelineSpec}",
				SelectionData.Selection.SlotName.ToString(), PipelineSpec->GetPathName());

			continue;
		}

		FMetaHumanPipelineSlotSelectionData& NewSelection = Result.Add_GetRef(SelectionData);
		NewSelection.Selection.SlotName = ResolvedSlotName.GetValue();
	}

	return Result;
}

#if WITH_EDITORONLY_DATA

FString UMetaHumanCollection::GetUnpackFolder() const
{
	switch (UnpackPathMode)
	{
		case EMetaHumanCharacterUnpackPathMode::SubfolderNamedForPalette:
		{
			return GetPackage()->GetName();
		}
		case EMetaHumanCharacterUnpackPathMode::Relative:
		{
			FString UnpackFolder = FPackageName::GetLongPackagePath(GetPackage()->GetName());

			if (UnpackFolderPath.Len() > 0)
			{
				UnpackFolder /= UnpackFolderPath;
			}

			return UnpackFolder;
		}
		case EMetaHumanCharacterUnpackPathMode::Absolute:
		{
			return UnpackFolderPath;
		}
		default:
		{
			checkNoEntry();
			return FString(TEXT(""));
		}
	}
}

#endif // WITH_EDITORONLY_DATA
