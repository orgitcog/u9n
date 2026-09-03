// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSCSEditorViewport.h"

#include "BlueprintEditor.h"
#include "BlueprintEditorCommands.h"
#include "BlueprintEditorSettings.h"
#include "BlueprintEditorTabs.h"
#include "Containers/EnumAsByte.h"
#include "CoreGlobals.h"
#include "Delegates/Delegate.h"
#include "Editor/EditorEngine.h"
#include "EditorViewportClient.h"
#include "EditorViewportCommands.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GenericPlatform/ICursor.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Text.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "PreviewScene.h"
#include "RHIDefinitions.h"
#include "SCSEditorViewportClient.h"
#include "SEditorViewportToolBarMenu.h"
#include "SSubobjectEditor.h"
#include "STransformViewportToolbar.h"
#include "SViewportToolBar.h"
#include "Slate/SceneViewport.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "ToolMenus.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"

class FDragDropEvent;
class SDockTab;
class SWidget;
struct FGeometry;

#define LOCTEXT_NAMESPACE "SSCSEditorViewportToolBar"

namespace UE::SCSEditor::Private
{

void CreateCameraSpeedMenu(UToolMenu* InMenu, TWeakPtr<FSCSEditorViewportClient> InWeakViewportClient)
{
	FToolMenuSection& PositioningSection =
		InMenu->FindOrAddSection("Positioning", LOCTEXT("PositioningLabel", "Positioning"));

	PositioningSection.AddEntry(UE::UnrealEd::CreateCameraSpeedMenu());
}

bool IsViewModeSupported(EViewModeIndex InViewModeIndex)
{
	switch (InViewModeIndex)
	{
	case VMI_Unlit:
	case VMI_Lit:
	case VMI_BrushWireframe:
	case VMI_CollisionVisibility:
		return true;
	default:
		return false;
	}
}

bool DoesViewModeMenuShowSection(UE::UnrealEd::EHidableViewModeMenuSections)
{
	return false;
}

} // namespace UE::SCSEditor::Private

/*-----------------------------------------------------------------------------
   SSCSEditorViewportToolBar
-----------------------------------------------------------------------------*/

class SSCSEditorViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS( SSCSEditorViewportToolBar ){}
		SLATE_ARGUMENT(TWeakPtr<SSCSEditorViewport>, EditorViewport)
	SLATE_END_ARGS()

	/** Constructs this widget with the given parameters */
	void Construct(const FArguments& InArgs)
	{
		EditorViewport = InArgs._EditorViewport;

		const FMargin ToolbarSlotPadding(4.0f, 1.0f);

		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.Background"))
			.Cursor(EMouseCursor::Default)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(ToolbarSlotPadding)
				[
					SNew(SEditorViewportToolbarMenu)
					.ParentToolBar(SharedThis(this))
					.Cursor(EMouseCursor::Default)
					.Image("EditorViewportToolBar.OptionsDropdown")
					.OnGetMenuContent(this, &SSCSEditorViewportToolBar::GeneratePreviewMenu)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(ToolbarSlotPadding)
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Label(this, &SSCSEditorViewportToolBar::GetCameraMenuLabel)
					.OnGetMenuContent(this, &SSCSEditorViewportToolBar::GenerateCameraMenu)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(ToolbarSlotPadding)
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Cursor( EMouseCursor::Default )
					.Label(this, &SSCSEditorViewportToolBar::GetViewMenuLabel)
					.OnGetMenuContent(this, &SSCSEditorViewportToolBar::GenerateViewMenu)
				]
				+ SHorizontalBox::Slot()
				.Padding(ToolbarSlotPadding)
				.HAlign( HAlign_Right )
				[
					SNew(STransformViewportToolBar)
					.Viewport(EditorViewport.Pin().ToSharedRef())
					.CommandList(EditorViewport.Pin()->GetCommandList())
				]
			]
		];

		SViewportToolBar::Construct(SViewportToolBar::FArguments());
	}

	/** Creates the preview menu */
	TSharedRef<SWidget> GeneratePreviewMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid()? EditorViewport.Pin()->GetCommandList(): NULL;

		const bool bInShouldCloseWindowAfterMenuSelection = true;

		FMenuBuilder PreviewOptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);
		{
			PreviewOptionsMenuBuilder.BeginSection("BlueprintEditorPreviewOptions", NSLOCTEXT("BlueprintEditor", "PreviewOptionsMenuHeader", "Preview Viewport Options"));
			{
				PreviewOptionsMenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().ResetCamera);
				PreviewOptionsMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().ToggleRealTime);
				PreviewOptionsMenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().ShowFloor);
				PreviewOptionsMenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().ShowGrid);
			}
			PreviewOptionsMenuBuilder.EndSection();
		}

		return PreviewOptionsMenuBuilder.MakeWidget();
	}

	FText GetCameraMenuLabel() const
	{
		if(EditorViewport.IsValid())
		{
			return UE::UnrealEd::GetCameraSubmenuLabelFromViewportType(
				EditorViewport.Pin()->GetViewportClient()->GetViewportType()
			);
		}

		return NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Default", "Camera");
	}

	TSharedRef<SWidget> GenerateCameraMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid()? EditorViewport.Pin()->GetCommandList(): nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder CameraMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);

		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

		CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", NSLOCTEXT("BlueprintEditor", "CameraTypeHeader_Ortho", "Orthographic"));
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
		CameraMenuBuilder.EndSection();

		return CameraMenuBuilder.MakeWidget();
	}

	FText GetViewMenuLabel() const
	{
		FText Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Default", "View");

		if (EditorViewport.IsValid())
		{
			switch (EditorViewport.Pin()->GetViewportClient()->GetViewMode())
			{
			case VMI_Lit:
				Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Lit", "Lit");
				break;

			case VMI_Unlit:
				Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Unlit", "Unlit");
				break;

			case VMI_BrushWireframe:
				Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Wireframe", "Wireframe");
				break;

			case VMI_CollisionVisibility:
				Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_CollisionVisibility", "Collision Visibility");
				break;
			}
		}

		return Label;
	}

	TSharedRef<SWidget> GenerateViewMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid() ? EditorViewport.Pin()->GetCommandList() : nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder ViewMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);

		ViewMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().LitMode, NAME_None, NSLOCTEXT("BlueprintEditor", "LitModeMenuOption", "Lit"));
		ViewMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().UnlitMode, NAME_None, NSLOCTEXT("BlueprintEditor", "UnlitModeMenuOption", "Unlit"));
		ViewMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().WireframeMode, NAME_None, NSLOCTEXT("BlueprintEditor", "WireframeModeMenuOption", "Wireframe"));
		ViewMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().CollisionVisibility, NAME_None, NSLOCTEXT("BlueprintEditor", "CollisionVisibilityMenuOption", "Visibility Collision"));

		return ViewMenuBuilder.MakeWidget();
	}

private:
	/** Reference to the parent viewport */
	TWeakPtr<SSCSEditorViewport> EditorViewport;
};


/*-----------------------------------------------------------------------------
   SSCSEditorViewport
-----------------------------------------------------------------------------*/

void SSCSEditorViewport::Construct(const FArguments& InArgs)
{
	bIsActiveTimerRegistered = false;

	// Save off the Blueprint editor reference, we'll need this later
	BlueprintEditorPtr = InArgs._BlueprintEditor;

	SEditorViewport::Construct( SEditorViewport::FArguments() );

	// Restore last used feature level
	if (ViewportClient.IsValid())
	{
		UWorld* World = ViewportClient->GetPreviewScene()->GetWorld();
		if (World != nullptr)
		{
			World->ChangeFeatureLevel(GWorld->GetFeatureLevel());
		}
	}

	// Use a delegate to inform the attached world of feature level changes.
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	PreviewFeatureLevelChangedHandle = Editor->OnPreviewFeatureLevelChanged().AddLambda([this](ERHIFeatureLevel::Type NewFeatureLevel)
		{
			if(ViewportClient.IsValid())
			{
				UWorld* World = ViewportClient->GetPreviewScene()->GetWorld();
				if (World != nullptr)
				{
					World->ChangeFeatureLevel(NewFeatureLevel);

					// Refresh the preview scene. Don't change the camera.
					RequestRefresh(false);
				}
			}
		});

	// Refresh the preview scene
	RequestRefresh(true);
}

SSCSEditorViewport::~SSCSEditorViewport()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	Editor->OnPreviewFeatureLevelChanged().Remove(PreviewFeatureLevelChangedHandle);

	if(ViewportClient.IsValid())
	{
		// Reset this to ensure it's no longer in use after destruction
		ViewportClient->Viewport = NULL;
	}
}

bool SSCSEditorViewport::IsVisible() const
{
	// We consider the viewport to be visible if the reference is valid
	return ViewportWidget.IsValid() && SEditorViewport::IsVisible();
}

TSharedRef<FEditorViewportClient> SSCSEditorViewport::MakeEditorViewportClient()
{
	FPreviewScene* PreviewScene = BlueprintEditorPtr.Pin()->GetPreviewScene();

	// Construct a new viewport client instance.
	ViewportClient = MakeShareable(new FSCSEditorViewportClient(BlueprintEditorPtr, PreviewScene, SharedThis(this)));
	ViewportClient->SetRealtime(true);
	ViewportClient->bSetListenerPosition = false;
	ViewportClient->VisibilityDelegate.BindSP(this, &SSCSEditorViewport::IsVisible);

	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SSCSEditorViewport::MakeViewportToolbar()
{
	TSharedRef<SSCSEditorViewportToolBar> OldViewportToolbar =
		SNew(SSCSEditorViewportToolBar)
			.EditorViewport(SharedThis(this))
			.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());

	// clang-format off
	return 
		SNew(SVerticalBox)
		.Visibility( EVisibility::SelfHitTestInvisible )
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 1.0f, 0, 0)
		.VAlign(VAlign_Top)
		[
			OldViewportToolbar
		];
	// clang-format on
}

TSharedPtr<SWidget> SSCSEditorViewport::BuildViewportToolbar()
{
	// Register the viewport toolbar if another viewport hasn't already (it's shared).
	const FName ViewportToolbarName = "SCSEditor.ViewportToolbar";
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
			{
				RightSection.AddEntry(UE::UnrealEd::CreateCameraSubmenu(UE::UnrealEd::FViewportCameraMenuOptions().ShowCameraMovement()));
				
				const FName SubmenuName = UToolMenus::JoinMenuPaths(ViewportToolbarName, "Camera");
                UToolMenu* const Submenu = UToolMenus::Get()->ExtendMenu(SubmenuName);
                FToolMenuSection& MovementSection = Submenu->FindOrAddSection("Movement");
                MovementSection.AddMenuEntry(FBlueprintEditorCommands::Get().ResetCamera);
			}

			// TODO: Filter this menu with IsViewModeSupportedDelegate (see further down in this file) and remove the
			// "Exposure" section.

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
					UToolMenus::Get()->RegisterMenu("SCSEditor.ViewportToolbar.ViewModes", ParentSubmenuName);
				}

				RightSection.AddEntry(UE::UnrealEd::CreateViewModesSubmenu());
			}

			// Add the "Show" submenu.
			{
				RightSection.AddEntry(UE::UnrealEd::CreateShowSubmenu(FNewToolMenuDelegate::CreateLambda(
					[](UToolMenu* Submenu) -> void
					{
						FToolMenuSection& UnnamedSection = Submenu->FindOrAddSection(NAME_None);
						UnnamedSection.AddMenuEntry(FBlueprintEditorCommands::Get().ShowFloor);
						UnnamedSection.AddMenuEntry(FBlueprintEditorCommands::Get().ShowGrid);
					})
				));
			}

			// Add the "Performance & Scalability" submenu.
			{
				FToolMenuEntry PerfSubmenu = FToolMenuEntry::InitSubMenu(
					"PerformanceAndScalability",
					LOCTEXT("PerformanceAndScalabilityLabel", "Performance and Scalability"),
					LOCTEXT("PerformanceAndScalabilityTooltip", "Performance and Scalability tools tied to this viewport"),
					FNewToolMenuDelegate::CreateLambda(
						[](UToolMenu* Submenu) -> void
						{
							FToolMenuSection& UnnamedSection = Submenu->FindOrAddSection(NAME_None);
							UnnamedSection.AddEntry(UE::UnrealEd::CreateToggleRealtimeEntry());

							if (const UUnrealEdViewportToolbarContext* Context =
									Submenu->FindContext<UUnrealEdViewportToolbarContext>())
							{
								UnnamedSection.AddEntry(UE::UnrealEd::CreateRemoveRealtimeOverrideEntry(Context->Viewport));
							}
						}
					)
				);
				PerfSubmenu.Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Scalability");
				PerfSubmenu.ToolBarData.LabelOverride = FText();
				PerfSubmenu.ToolBarData.ResizeParams.ClippingPriority = 800;
				RightSection.AddEntry(PerfSubmenu);
			}
		}
	}

	FToolMenuContext ViewportToolbarContext;
	{
		ViewportToolbarContext.AppendCommandList(GetCommandList());

		// Add the UnrealEd viewport toolbar context.
		{
			UUnrealEdViewportToolbarContext* const ContextObject = NewObject<UUnrealEdViewportToolbarContext>();
			ContextObject->Viewport = SharedThis(this);

			// Setup the callback to filter available view modes
			ContextObject->IsViewModeSupported =
				UE::UnrealEd::IsViewModeSupportedDelegate::CreateStatic(&UE::SCSEditor::Private::IsViewModeSupported);

			// Setup the callback to hide/show specific sections
			ContextObject->DoesViewModeMenuShowSection = UE::UnrealEd::DoesViewModeMenuShowSectionDelegate::CreateStatic(
				&UE::SCSEditor::Private::DoesViewModeMenuShowSection
			);

			ViewportToolbarContext.AddObject(ContextObject);
		}
	}

	return UToolMenus::Get()->GenerateWidget(ViewportToolbarName, ViewportToolbarContext);
}

void SSCSEditorViewport::PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);

	// add the feature level display widget
	Overlay->AddSlot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Right)
		.Padding(5.0f)
		[
			BuildFeatureLevelWidget()
		];
}

void SSCSEditorViewport::BindCommands()
{
	FSCSEditorViewportCommands::Register(); // make sure the viewport specific commands have been registered

	TSharedPtr<FBlueprintEditor> BlueprintEditor = BlueprintEditorPtr.Pin();
	TSharedPtr<SSubobjectEditor> SubobjectEditorPtr = BlueprintEditor->GetSubobjectEditor();
	SSubobjectEditor* SubobjectEditorWidget = SubobjectEditorPtr.Get(); 
	
	// for mac, we have to bind a command that would override the BP-Editor's 
	// "NavigateToParentBackspace" command, because the delete key is the 
	// backspace key for that platform (and "NavigateToParentBackspace" does not 
	// make sense in the viewport window... it blocks the generic delete command)
	// 
	// NOTE: this needs to come before we map any other actions (so it is 
	// prioritized first)

	if(SubobjectEditorWidget)
	{
		CommandList->MapAction(
		    FSCSEditorViewportCommands::Get().DeleteComponent,
		    FExecuteAction::CreateSP(SubobjectEditorWidget, &SSubobjectEditor::OnDeleteNodes),
		    FCanExecuteAction::CreateSP(SubobjectEditorWidget, &SSubobjectEditor::CanDeleteNodes)
		);
		
		CommandList->Append(SubobjectEditorWidget->GetCommandList().ToSharedRef());
	}
	
	CommandList->Append(BlueprintEditor->GetToolkitCommands());
	SEditorViewport::BindCommands();

	const FBlueprintEditorCommands& Commands = FBlueprintEditorCommands::Get();

	BlueprintEditorPtr.Pin()->GetToolkitCommands()->MapAction(
		Commands.EnableSimulation,
		FExecuteAction::CreateSP(this, &SSCSEditorViewport::ToggleIsSimulateEnabled),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::GetIsSimulateEnabled),
		FIsActionButtonVisible::CreateSP(this, &SSCSEditorViewport::ShouldShowViewportCommands));

	// Toggle camera lock on/off
	CommandList->MapAction(
		Commands.ResetCamera,
		FExecuteAction::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::ResetCamera));

	CommandList->MapAction(
		Commands.ShowFloor,
		FExecuteAction::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::ToggleShowFloor),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::GetShowFloor));

	CommandList->MapAction(
		Commands.ShowGrid,
		FExecuteAction::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::ToggleShowGrid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::GetShowGrid));
}

void SSCSEditorViewport::Invalidate()
{
	ViewportClient->Invalidate();
}

void SSCSEditorViewport::ToggleIsSimulateEnabled()
{
	// Make the viewport visible if the simulation is starting.
	if ( !ViewportClient->GetIsSimulateEnabled() )
	{
		if ( GetDefault<UBlueprintEditorSettings>()->bShowViewportOnSimulate )
		{
			BlueprintEditorPtr.Pin()->GetTabManager()->TryInvokeTab(FBlueprintEditorTabs::SCSViewportID);
		}
	}

	ViewportClient->ToggleIsSimulateEnabled();
}

void SSCSEditorViewport::EnablePreview(bool bEnable)
{
	const FText SystemDisplayName = NSLOCTEXT("BlueprintEditor", "RealtimeOverrideMessage_Blueprints", "the active blueprint mode");
	if(bEnable)
	{
		// Restore the previously-saved realtime setting
		ViewportClient->RemoveRealtimeOverride(SystemDisplayName);
	}
	else
	{
		// Disable and store the current realtime setting. This will bypass real-time rendering in the preview viewport (see UEditorEngine::UpdateSingleViewportClient).
		const bool bShouldBeRealtime = false;
		ViewportClient->AddRealtimeOverride(bShouldBeRealtime, SystemDisplayName);
	}
}

void SSCSEditorViewport::RequestRefresh(bool bResetCamera, bool bRefreshNow)
{
	if(bRefreshNow)
	{
		if(ViewportClient.IsValid())
		{
			ViewportClient->InvalidatePreview(bResetCamera);
		}
	}
	else
	{
		// Defer the update until the next tick. This way we don't accidentally spawn the preview actor in the middle of a transaction, for example.
		if (!bIsActiveTimerRegistered)
		{
			bIsActiveTimerRegistered = true;
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SSCSEditorViewport::DeferredUpdatePreview, bResetCamera));
		}
	}
}

void SSCSEditorViewport::OnComponentSelectionChanged()
{
	// When the component selection changes, make sure to invalidate hit proxies to sync with the current selection
	SceneViewport->Invalidate();
}

void SSCSEditorViewport::OnFocusViewportToSelection()
{
	ViewportClient->FocusViewportToSelection();
}

bool SSCSEditorViewport::ShouldShowViewportCommands() const
{
	// Hide if actively debugging
	return !GIntraFrameDebuggingGameThread;
}

bool SSCSEditorViewport::GetIsSimulateEnabled()
{
	return ViewportClient->GetIsSimulateEnabled();
}

void SSCSEditorViewport::SetOwnerTab(TSharedRef<SDockTab> Tab)
{
	OwnerTab = Tab;
}

TSharedPtr<SDockTab> SSCSEditorViewport::GetOwnerTab() const
{
	return OwnerTab.Pin();
}

FReply SSCSEditorViewport::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<SSubobjectEditor> SubobjectEditor = BlueprintEditorPtr.Pin()->GetSubobjectEditor();

	return SubobjectEditor->TryHandleAssetDragDropOperation(DragDropEvent);
}

EActiveTimerReturnType SSCSEditorViewport::DeferredUpdatePreview(double InCurrentTime, float InDeltaTime, bool bResetCamera)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->InvalidatePreview(bResetCamera);
	}

	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

#undef LOCTEXT_NAMESPACE
