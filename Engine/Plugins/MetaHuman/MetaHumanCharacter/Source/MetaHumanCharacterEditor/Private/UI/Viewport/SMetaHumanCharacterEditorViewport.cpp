// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorViewport.h"

#include "MetaHumanCharacterEditorViewportClient.h"
#include "MetaHumanCharacterEditorLog.h"
#include "SMetaHumanCharacterEditorViewportToolBar.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorActor.h"
#include "MetaHumanCharacterEditorStyle.h"

#include "EditorViewportCommands.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSlider.h"
#include "SViewportToolBar.h"
#include "SEditorViewportToolBarButton.h"
#include "UI/Viewport/SMetaHumanCharacterEditorViewportAnimationBar.h"
#include "UI/Widgets/SMetaHumanCharacterEditorTileView.h"
#include "ViewportToolbar/UnrealEdViewportToolbarContext.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditorViewport"

TSharedPtr<SWidget> SMetaHumanCharacterEditorViewport::BuildViewportToolbar()
{
	// Register the viewport toolbar if another viewport hasn't already (it's shared).
	const FName ViewportToolbarName = "MetaHumanCharacterEditorViewport.ViewportToolbar";

	if (!UToolMenus::Get()->IsMenuRegistered(ViewportToolbarName))
	{
		UToolMenu* const ViewportToolbarMenu = UToolMenus::Get()->RegisterMenu(
			ViewportToolbarName, NAME_None /* parent */, EMultiBoxType::SlimHorizontalToolBar
		);

		//ViewportToolbarMenu->SetStyleSet(&FLevelEditorStyle::Get());
		ViewportToolbarMenu->StyleName = "ViewportToolbar";
		ViewportToolbarMenu->bSeparateSections = false;

		// Add the left-aligned part of the viewport toolbar.
		{
			FToolMenuSection& LeftSection = ViewportToolbarMenu->AddSection("Left");
		}

		// Add the right-aligned part of the viewport toolbar.
		{
			FToolMenuSection& RightSection = ViewportToolbarMenu->AddSection("Right");
			RightSection.Alignment = EToolMenuSectionAlign::Last;
			RightSection.AddEntry(CreatePreviewMaterialSubmenu());
			RightSection.AddEntry(CreateEnvironmentSubmenu());
			RightSection.AddEntry(CreateCameraSelectionSubmenu());
			RightSection.AddEntry(CreateLODSubmenu());
			RightSection.AddEntry(CreateRenderingQualitySubmenu());
			RightSection.AddEntry(CreateViewportOverlayToggle());
		}
	}

	FToolMenuContext ViewportToolbarContext;
	{
		ViewportToolbarContext.AppendCommandList(GetCommandList());

		// Add the UnrealEd viewport toolbar context.
		{
			UUnrealEdViewportToolbarContext* const ContextObject = NewObject<UUnrealEdViewportToolbarContext>();
			ContextObject->Viewport = SharedThis(this);
			ViewportToolbarContext.AddObject(ContextObject);
		}
	}

	return UToolMenus::Get()->GenerateWidget(ViewportToolbarName, ViewportToolbarContext);
}

TSharedRef<FMetaHumanCharacterViewportClient> SMetaHumanCharacterEditorViewport::GetMetaHumanCharacterEditorViewportClient() const
{
	return StaticCastSharedPtr<FMetaHumanCharacterViewportClient>(Client).ToSharedRef();
}

#undef LOCTEXT_NAMESPACE
