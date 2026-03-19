// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanWardrobeItem.h"

#include "MetaHumanItemEditorPipeline.h"
#include "MetaHumanItemPipeline.h"

#include "UObject/Package.h"

#if WITH_EDITOR
void UMetaHumanWardrobeItem::SetPipeline(TNotNull<UMetaHumanItemPipeline*> InPipeline)
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
}

const UMetaHumanItemEditorPipeline* UMetaHumanWardrobeItem::GetEditorPipeline() const
{
	return Pipeline ? Pipeline->GetEditorPipeline() : nullptr;
}

const UMetaHumanCharacterEditorPipeline* UMetaHumanWardrobeItem::GetPaletteEditorPipeline() const
{
	return GetEditorPipeline();
}
#endif // WITH_EDITOR

TObjectPtr<const UMetaHumanItemPipeline> UMetaHumanWardrobeItem::GetPipeline() const
{
	return Pipeline;
}

const UMetaHumanCharacterPipeline* UMetaHumanWardrobeItem::GetPalettePipeline() const
{
	return GetPipeline();
}

bool UMetaHumanWardrobeItem::IsExternal() const
{
	return GetOuter()->IsA<UPackage>();
}

#if WITH_EDITOR
void UMetaHumanWardrobeItem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaHumanWardrobeItem, Pipeline))
	{
		if (Pipeline)
		{
			SetPipeline(Pipeline);
		}
	}
}
#endif // WITH_EDITOR
