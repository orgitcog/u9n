// Copyright Epic Games, Inc. All Rights Reserved.

#include "StaticMeshEditorViewportToolbarSections.h"

#include "SStaticMeshEditorViewport.h"
#include "StaticMeshEditorActions.h"
#include "StaticMeshViewportLODCommands.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"
#include "ViewportToolbar/UnrealEdViewportToolbarContext.h"

#define LOCTEXT_NAMESPACE "StaticMeshEditorViewportToolbarSections"

FText UE::StaticMeshEditor::GetLODMenuLabel(const TSharedPtr<SStaticMeshEditorViewport>& InStaticMeshEditorViewport)
{
	FText Label = LOCTEXT("LODMenu_AutoLabel", "LOD Auto");

	if (InStaticMeshEditorViewport)
	{
		const int32 LODSelectionType = InStaticMeshEditorViewport->GetCurrentLOD();

		if (LODSelectionType >= 0)
		{
			FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODSelectionType);
			Label = FText::FromString(TitleLabel);
		}
	}

	return Label;
}

FToolMenuEntry UE::StaticMeshEditor::CreateLODSubmenu()
{
	FToolMenuEntry Entry = FToolMenuEntry::InitDynamicEntry(
		"DynamicLODOptions",
		FNewToolMenuSectionDelegate::CreateLambda(
			[](FToolMenuSection& InDynamicSection) -> void
			{
				if (UUnrealEdViewportToolbarContext* const EditorViewportContext = InDynamicSection.FindContext<UUnrealEdViewportToolbarContext>())
				{
					TWeakPtr<SStaticMeshEditorViewport> StaticMeshEditorViewport = StaticCastWeakPtr<SStaticMeshEditorViewport>(EditorViewportContext->Viewport);
					FToolMenuEntry& Entry = InDynamicSection.AddEntry(UE::UnrealEd::CreatePreviewLODSelectionSubmenu(StaticMeshEditorViewport));
					Entry.ToolBarData.ResizeParams.ClippingPriority = 800;
				}
			}
		)
	);
	return Entry;
}

FToolMenuEntry UE::StaticMeshEditor::CreateShowSubmenu()
{
	return UE::UnrealEd::CreateShowSubmenu(FNewToolMenuDelegate::CreateLambda(&FillShowSubmenu));
}

TSharedRef<SWidget> UE::StaticMeshEditor::GenerateLODMenuWidget(const TSharedPtr<SStaticMeshEditorViewport>& InStaticMeshEditorViewport
)
{
	if (!InStaticMeshEditorViewport)
	{
		return SNullWidget::NullWidget;
	}

	// We generate a menu via UToolMenus, so we can use FillShowSubmenu call from both old and new toolbar
	FName OldShowMenuName = "StaticMeshEditor.OldViewportToolbar.LODMenu";

	if (!UToolMenus::Get()->IsMenuRegistered(OldShowMenuName))
	{
		if (UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(OldShowMenuName, NAME_None, EMultiBoxType::Menu, false))
		{
			Menu->AddDynamicSection(
				"BaseSection",
				FNewToolMenuDelegate::CreateLambda(
					[](UToolMenu* InMenu)
					{
						if (UUnrealEdViewportToolbarContext* const EditorViewportContext = InMenu->FindContext<UUnrealEdViewportToolbarContext>())
						{
							UE::UnrealEd::FillPreviewLODSelectionSubmenu(InMenu, StaticCastWeakPtr<SStaticMeshEditorViewport>(EditorViewportContext->Viewport));
						}
					}
				)
			);
		}
	}

	FToolMenuContext MenuContext;
	{
		MenuContext.AppendCommandList(InStaticMeshEditorViewport->GetCommandList());

		// Add the UnrealEd viewport toolbar context.
		{
			UUnrealEdViewportToolbarContext* const ContextObject =
				UE::UnrealEd::CreateViewportToolbarDefaultContext(InStaticMeshEditorViewport);

			MenuContext.AddObject(ContextObject);
		}
	}

	return UToolMenus::Get()->GenerateWidget(OldShowMenuName, MenuContext);
}

TSharedRef<SWidget> UE::StaticMeshEditor::GenerateShowMenuWidget(const TSharedPtr<SStaticMeshEditorViewport>& InStaticMeshEditorViewport
)
{
	if (!InStaticMeshEditorViewport)
	{
		return SNullWidget::NullWidget;
	}

	InStaticMeshEditorViewport->OnFloatingButtonClicked();

	// We generate a menu via UToolMenus, so we can use FillShowSubmenu call from both old and new toolbar
	FName OldShowMenuName = "StaticMesh.OldViewportToolbar.Show";

	if (!UToolMenus::Get()->IsMenuRegistered(OldShowMenuName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(OldShowMenuName, NAME_None, EMultiBoxType::Menu, false);
		Menu->AddDynamicSection(
			"BaseSection",
			FNewToolMenuDelegate::CreateLambda(
				[](UToolMenu* InMenu)
				{
					constexpr bool bShowGridToggle = true;
					FillShowSubmenu(InMenu);
				}
			)
		);
	}

	FToolMenuContext MenuContext;
	{
		MenuContext.AppendCommandList(InStaticMeshEditorViewport->GetCommandList());

		// Add the UnrealEd viewport toolbar context.
		{
			UUnrealEdViewportToolbarContext* const ContextObject =
				UE::UnrealEd::CreateViewportToolbarDefaultContext(InStaticMeshEditorViewport);

			MenuContext.AddObject(ContextObject);
		}
	}

	return UToolMenus::Get()->GenerateWidget(OldShowMenuName, MenuContext);
}

void UE::StaticMeshEditor::FillShowSubmenu(UToolMenu* InMenu)
{
	if (UUnrealEdViewportToolbarContext* const EditorViewportContext =
			InMenu->FindContext<UUnrealEdViewportToolbarContext>())
	{
		if (TSharedPtr<SStaticMeshEditorViewport> StaticMeshEditorViewport =
				StaticCastSharedPtr<SStaticMeshEditorViewport>(EditorViewportContext->Viewport.Pin()))
		{
			FToolMenuSection& UnnamedSection = InMenu->FindOrAddSection(NAME_None);
			const FStaticMeshEditorCommands& Commands = FStaticMeshEditorCommands::Get();

			UnnamedSection.AddMenuEntry(Commands.SetShowNaniteFallback);
			UnnamedSection.AddMenuEntry(Commands.SetShowDistanceField);
			UnnamedSection.AddMenuEntry(Commands.SetShowRayTracingFallback);

			FToolMenuSection& MeshComponentsSection =
				InMenu->FindOrAddSection("MeshComponents", LOCTEXT("MeshComponments", "Mesh Components"));

			MeshComponentsSection.AddMenuEntry(Commands.SetShowSockets);
			MeshComponentsSection.AddMenuEntry(Commands.SetShowVertices);
			MeshComponentsSection.AddMenuEntry(Commands.SetShowVertexColor);
			MeshComponentsSection.AddMenuEntry(Commands.SetShowNormals);
			MeshComponentsSection.AddMenuEntry(Commands.SetShowTangents);
			MeshComponentsSection.AddMenuEntry(Commands.SetShowBinormals);

			MeshComponentsSection.AddSeparator(NAME_None);

			MeshComponentsSection.AddMenuEntry(Commands.SetShowPivot);
			MeshComponentsSection.AddMenuEntry(Commands.SetShowGrid);

			MeshComponentsSection.AddMenuEntry(Commands.SetShowBounds);
			MeshComponentsSection.AddMenuEntry(Commands.SetShowSimpleCollision);
			MeshComponentsSection.AddMenuEntry(Commands.SetShowComplexCollision);
			MeshComponentsSection.AddMenuEntry(Commands.SetShowPhysicalMaterialMasks);
		}
	}
}

#undef LOCTEXT_NAMESPACE
