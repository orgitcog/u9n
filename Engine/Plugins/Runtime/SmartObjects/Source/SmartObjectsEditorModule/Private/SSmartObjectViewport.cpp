// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSmartObjectViewport.h"

#include "EditorViewportCommands.h"
#include "PreviewProfileController.h"
#include "SmartObjectAssetEditorViewportClient.h"
#include "SSmartObjectViewportToolbar.h"
#include "SmartObjectAssetToolkit.h"
#include "Framework/Application/SlateApplication.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "SmartObjectViewport"

void SSmartObjectViewport::Construct(const FArguments& InArgs)
{
	ViewportClient = InArgs._EditorViewportClient;
	PreviewScene = InArgs._PreviewScene;
	AssetEditorToolkitPtr = InArgs._AssetEditorToolkit;

	SEditorViewport::Construct(
		SEditorViewport::FArguments().IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
	);
}

void SSmartObjectViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	// We don't support scaling widget mode (see  FSmartObjectAssetEditorViewportClient::CanSetWidgetMode)
	// so we also disable scale grid snap
	CommandList->MapAction(
		FEditorViewportCommands::Get().ScaleGridSnap,
		FExecuteAction(),
		FCanExecuteAction::CreateLambda([] { return false; }),
		FIsActionChecked::CreateLambda([] { return false; }));
}

TSharedRef<FEditorViewportClient> SSmartObjectViewport::MakeEditorViewportClient()
{
	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SSmartObjectViewport::MakeViewportToolbar()
{
	return SAssignNew(ViewportToolbar, SSmartObjectViewportToolBar, SharedThis(this));
}

TSharedPtr<SWidget> SSmartObjectViewport::BuildViewportToolbar()
{
	// Register the viewport toolbar if another viewport hasn't already (it's shared).
	const FName ViewportToolbarName = "SmartObjectEditor.ViewportToolbar";

	if (!UToolMenus::Get()->IsMenuRegistered(ViewportToolbarName))
	{
		UToolMenu* const ViewportToolbarMenu = UToolMenus::Get()->RegisterMenu(
			ViewportToolbarName, NAME_None /* parent */, EMultiBoxType::SlimHorizontalToolBar
		);

		ViewportToolbarMenu->StyleName = "ViewportToolbar";

		// Add the left-aligned part of the viewport toolbar.
		{
			FToolMenuSection& LeftSection = ViewportToolbarMenu->AddSection("Left");
			LeftSection.AddEntry(UE::UnrealEd::CreateTransformsSubmenu());
			LeftSection.AddEntry(UE::UnrealEd::CreateSnappingSubmenu());
		}

		// Add the right-aligned part of the viewport toolbar.
		{
			FToolMenuSection& RightSection = ViewportToolbarMenu->AddSection("Right");
			RightSection.Alignment = EToolMenuSectionAlign::Last;

			// Add the "Camera" submenu.
			RightSection.AddEntry(UE::UnrealEd::CreateCameraSubmenu(UE::UnrealEd::FViewportCameraMenuOptions().ShowAll()));

			// Add the "View Modes" sub menu.
			{
				// Stay backward-compatible with the old viewport toolbar.
				{
					const FName ParentSubmenuName = "UnrealEd.ViewportToolbar.View";
					// Create our parent menu.
					if (!UToolMenus::Get()->IsMenuRegistered(ParentSubmenuName))
					{
						UToolMenus::Get()->RegisterMenu(ParentSubmenuName);
					}

					// Register our ToolMenu here first, before we create the submenu, so we can set our parent.
					UToolMenus::Get()->RegisterMenu("SmartObjectEditor.ViewportToolbar.ViewModes", ParentSubmenuName);
				}

				RightSection.AddEntry(UE::UnrealEd::CreateViewModesSubmenu());
			}

			RightSection.AddEntry(UE::UnrealEd::CreateDefaultShowSubmenu());
			RightSection.AddEntry(UE::UnrealEd::CreatePerformanceAndScalabilitySubmenu());
			RightSection.AddEntry(UE::UnrealEd::CreateAssetViewerProfileSubmenu());
		}
	}

	FToolMenuContext ViewportToolbarContext;
	{
		ViewportToolbarContext.AppendCommandList(GetCommandList());

		// Add the UnrealEd viewport toolbar context.
		{
			UUnrealEdViewportToolbarContext* const ContextObject = NewObject<UUnrealEdViewportToolbarContext>();
			ContextObject->Viewport = SharedThis(this);

			// No support for multiple coordinate systems
			ContextObject->bShowCoordinateSystemControls = false;
			// No surface snap
			ContextObject->bShowSurfaceSnap = false;

			// Hook up our toolbar's filter for supported view modes.
			ContextObject->IsViewModeSupported = UE::UnrealEd::IsViewModeSupportedDelegate::CreateLambda(
				[](EViewModeIndex ViewModeIndex) -> bool
				{
					// This code is taken from SViewportToolBar::IsViewModeSupported
					// SSCSEditorViewportToolBar does not override it, so we just take it as-is
					// TODO: maybe create a private function for it, or move IsViewModeSupported to SEditorViewport

					switch (ViewModeIndex)
					{
					case VMI_PrimitiveDistanceAccuracy:
					case VMI_MaterialTextureScaleAccuracy:
					case VMI_RequiredTextureResolution:
						return false;
					default:
						return true;
					}
				}
			);

			ViewportToolbarContext.AddObject(ContextObject);
		}
	}

	return UToolMenus::Get()->GenerateWidget(ViewportToolbarName, ViewportToolbarContext);
}

TSharedPtr<IPreviewProfileController> SSmartObjectViewport::CreatePreviewProfileController()
{
	return MakeShared<FPreviewProfileController>();
}

TSharedRef<SEditorViewport> SSmartObjectViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SSmartObjectViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SSmartObjectViewport::OnFloatingButtonClicked()
{
}

#undef LOCTEXT_NAMESPACE
