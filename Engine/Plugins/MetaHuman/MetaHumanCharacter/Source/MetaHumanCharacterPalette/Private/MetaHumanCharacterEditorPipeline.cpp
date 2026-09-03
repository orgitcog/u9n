// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorPipeline.h"

#include "MetaHumanCharacterPaletteLog.h"
#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanCharacterPipeline.h"
#include "MetaHumanWardrobeItem.h"

#include "Logging/StructuredLog.h"
#include "Misc/ScopedSlowTask.h"

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditorPipeline"

bool UMetaHumanCharacterEditorPipeline::IsPrincipalAssetClassCompatibleWithSlot(FName SlotName, TNotNull<const UClass*> AssetClass) const
{
	const FMetaHumanCharacterPipelineSlot* SlotSpec = GetRuntimeCharacterPipeline()->GetSpecification()->Slots.Find(SlotName);
	if (!SlotSpec)
	{
		// Slot not found
		return false;
	}

	return SlotSpec->SupportsAssetType(AssetClass);
}

bool UMetaHumanCharacterEditorPipeline::IsWardrobeItemCompatibleWithSlot(FName SlotName, TNotNull<const UMetaHumanWardrobeItem*> WardrobeItem) const
{
	const UObject* PrincipalAsset = WardrobeItem->PrincipalAsset.Get();

	if (!PrincipalAsset)
	{
		FScopedSlowTask Progress(0, LOCTEXT("LoadingAssets", "Loading assets..."));
		Progress.MakeDialog();

		PrincipalAsset = WardrobeItem->PrincipalAsset.LoadSynchronous();
	}

	if (!PrincipalAsset)
	{
		return false;
	}

	// TODO: Check compatibility of any pipeline set on the WardrobeItem

	return IsPrincipalAssetClassCompatibleWithSlot(SlotName, PrincipalAsset->GetClass());
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
