// Copyright Epic Games, Inc. All Rights Reserved.

#include "Library/LevelEditorViewportGroup.h"

#include "ImageViewers/LevelEditorViewportImageViewer.h"
#include "LevelEditor.h"
#include "Library/MediaViewerLibraryItem.h"
#include "Modules/ModuleManager.h"
#include "Slate/SceneViewport.h"
#include "SLevelViewport.h"

#define LOCTEXT_NAMESPACE "LevelEditorViewportGroup"

namespace UE::MediaViewer::Private
{

FLevelEditorViewportGroup::FLevelEditorViewportGroup(const TSharedRef<IMediaViewerLibrary>& InLibrary)
	: FLevelEditorViewportGroup(InLibrary, FGuid::NewGuid())
{
}

FLevelEditorViewportGroup::FLevelEditorViewportGroup(const TSharedRef<IMediaViewerLibrary>& InLibrary,
	const FGuid& InGuid)
	: FMediaViewerLibraryDynamicGroup(
		InLibrary,
		InGuid,
		LOCTEXT("LevelEditorViewports", "Editor Viewports"),
		LOCTEXT("LevelEditorViewportsTooltip", "The viewports available in the Level Editor."),
		FGenerateItems::CreateStatic(&FLevelEditorViewportGroup::GetLevelEditorViewportItems)
	)
{
}

TArray<TSharedRef<FMediaViewerLibraryItem>> FLevelEditorViewportGroup::GetLevelEditorViewportItems()
{
	TArray<TSharedRef<FMediaViewerLibraryItem>> LevelEditorViewportItems;

	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor");

	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	if (!LevelEditor.IsValid())
	{
		return LevelEditorViewportItems;
	}

	TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
	
	TSet<FGuid> AddedViewportIds;
	AddedViewportIds.Reserve(Viewports.Num());

	for (int32 Index = 0; Index < Viewports.Num(); ++Index)
	{
		TSharedPtr<FSceneViewport> ActiveViewport = Viewports[Index]->GetSharedActiveViewport();

		if (!ActiveViewport.IsValid())
		{
			continue;
		}

		const FIntPoint Size = ActiveViewport->GetSize();

		if (Size.X < 2 || Size.Y < 2)
		{
			continue;
		}

		const FString ConfigKey = Viewports[Index]->GetConfigKey().ToString();

		const FGuid ViewportId = FLevelEditorViewportImageViewer::FItem::GetIdForViewport(
			ConfigKey,
			/* Create id if invalid */ false
		);

		bool bAlreadyInSet = false;
		AddedViewportIds.Add(ViewportId, &bAlreadyInSet);

		if (bAlreadyInSet)
		{
			continue;
		}

		if (ViewportId.IsValid())
		{
			LevelEditorViewportItems.Add(MakeShared<FLevelEditorViewportImageViewer::FItem>(ViewportId, ConfigKey));
		}
	}

	return LevelEditorViewportItems;
}

} // UE::MediaViewer::Private

#undef LOCTEXT_NAMESPACE
