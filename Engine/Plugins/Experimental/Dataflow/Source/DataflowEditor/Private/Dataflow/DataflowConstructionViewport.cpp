// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowConstructionViewport.h"

#include "DataflowSceneProfileIndexStorage.h"
#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowConstructionViewportClient.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "EditorModeManager.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowConstructionViewportToolbar.h"
#include "Dataflow/DataflowConstructionScene.h"
#include "Dataflow/DataflowEditorPreviewSceneBase.h"
#include "Dataflow/DataflowRenderingViewMode.h"
#include "Dataflow/DataflowSimulationPanel.h"
#include "ToolMenus.h"
#include "Dataflow/DataflowConstructionVisualization.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"
#include "Widgets/Text/SRichTextBlock.h"

#define LOCTEXT_NAMESPACE "SDataflowConstructionViewport"


SDataflowConstructionViewport::SDataflowConstructionViewport()
{
}

void SDataflowConstructionViewport::Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs)
{
	SAssetEditorViewport::FArguments ParentArgs;
	ParentArgs._EditorViewportClient = InArgs._ViewportClient;
	SAssetEditorViewport::Construct(ParentArgs, InViewportConstructionArgs);
	Client->VisibilityDelegate.BindSP(this, &SDataflowConstructionViewport::IsVisible);
}

FDataflowConstructionScene* SDataflowConstructionViewport::GetConstructionScene() const
{
	const TSharedPtr<FDataflowConstructionViewportClient> DataflowClient = StaticCastSharedPtr<FDataflowConstructionViewportClient>(Client);
	if (DataflowClient)
	{
		if (const TSharedPtr<FDataflowEditorToolkit> Toolkit = DataflowClient->GetDataflowEditorToolkit().Pin())
		{
			return Toolkit->GetConstructionScene();
		}
	}
	return nullptr;
}

TSharedPtr<SWidget> SDataflowConstructionViewport::MakeViewportToolbar()
{
	return SNew(SDataflowConstructionViewportSelectionToolBar, SharedThis(this))
		.CommandList(CommandList);
}

TSharedPtr<SWidget> SDataflowConstructionViewport::BuildViewportToolbar()
{
	const FName ToolbarName = "Dataflow.ConstructionViewportToolbar";
	
	if (!UToolMenus::Get()->IsMenuRegistered(ToolbarName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(ToolbarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
		Menu->StyleName = "ViewportToolbar";
		
		Menu->AddSection("Left");
		
		{
			FToolMenuSection& RightSection = Menu->AddSection("Right");
			RightSection.Alignment = EToolMenuSectionAlign::Last;
			
			RightSection.AddEntry(UE::UnrealEd::CreateViewModesSubmenu());
			
			{
				RightSection.AddDynamicEntry("DynamicShowAndCamera", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& Section)
				{
					UUnrealEdViewportToolbarContext* Context = Section.FindContext<UUnrealEdViewportToolbarContext>();
					if (!Context)
					{
						return;
					}
					
					TSharedPtr<SDataflowConstructionViewport> ConstructionViewport = StaticCastSharedPtr<SDataflowConstructionViewport>(Context->Viewport.Pin());
					if (!ConstructionViewport)
					{
						return;
					}
				
					{
						// Show Menu
						Section.AddEntry(UE::UnrealEd::CreateShowSubmenu(
							FNewMenuDelegate::CreateLambda([WeakViewport = ConstructionViewport.ToWeakPtr()](FMenuBuilder& Menu)
							{
								if (const TSharedPtr<SDataflowConstructionViewport> PinnedEditorViewport = WeakViewport.Pin())
								{
									if (const TSharedPtr<FEditorViewportClient> ViewportClient = PinnedEditorViewport->GetViewportClient())
									{
										for (const TPair<FName, TUniquePtr<UE::Dataflow::IDataflowConstructionVisualization>>& Visualization : UE::Dataflow::FDataflowConstructionVisualizationRegistry::GetInstance().GetVisualizations())
										{
											Visualization.Value->ExtendViewportShowMenu(StaticCastSharedPtr<FDataflowConstructionViewportClient>(ViewportClient), Menu);
										}
									}
								}
							})
						));
					}
					
					{
						
						// Camera / View Options
						TAttribute<FText> Label = TAttribute<FText>::CreateLambda([WeakViewport = ConstructionViewport.ToWeakPtr()]
						{
							if (const TSharedPtr<SDataflowConstructionViewport> Viewport = WeakViewport.Pin())
							{
								if (UDataflowEditorMode* EditorMode = Viewport->GetEdMode())
								{
									return EditorMode->GetConstructionViewMode()->GetButtonText();
								}
							}
							
							return LOCTEXT("DataflowConstructionViewMenuTitle_Default", "View");
						});
						
						Section.AddSubMenu(
							"Camera",
							Label,
							LOCTEXT("CameraSubMenuTooltip", "Display options for the construction viewport."),
							FNewToolMenuDelegate::CreateLambda([WeakViewport = ConstructionViewport.ToWeakPtr()](UToolMenu* Menu)
							{
								FToolMenuSection& SimulationSection = Menu->AddSection("Simulation", LOCTEXT("SimulationSection", "Simulation"));
							
								if (const TSharedPtr<SDataflowConstructionViewport> Viewport = WeakViewport.Pin())
								{
									if (UDataflowEditorMode* EditorMode = Viewport->GetEdMode())
									{
										for (const TPair<FName, TSharedPtr<FUICommandInfo>>& NameAndCommand : FDataflowEditorCommandsImpl::Get().SetConstructionViewModeCommands)
										{
											if (EditorMode->CanChangeConstructionViewModeTo(NameAndCommand.Key))
											{
												SimulationSection.AddMenuEntry(NameAndCommand.Value);
											}
										}
									}
								}
							}),
							false,
							FSlateIcon(FAppStyle::Get().GetStyleSetName(), "EditorViewport.Perspective")
						);
					}
				}));
			}
		}
	}
	
	FToolMenuContext Context;
	{
		Context.AppendCommandList(GetCommandList());
		Context.AddExtender(GetExtenders());
		
		UUnrealEdViewportToolbarContext* ContextObject = UE::UnrealEd::CreateViewportToolbarDefaultContext(SharedThis(this));
		Context.AddObject(ContextObject);
	}
	
	return UToolMenus::Get()->GenerateWidget(ToolbarName, Context);
}

TSharedPtr<IPreviewProfileController> SDataflowConstructionViewport::CreatePreviewProfileController()
{
	TSharedPtr<FDataflowConstructionSceneProfileIndexStorage> ProfileIndexStorage = MakeShared<FDataflowConstructionSceneProfileIndexStorage>(GetConstructionScene());
	return MakeShared<FDataflowPreviewProfileController>(ProfileIndexStorage);
}

void SDataflowConstructionViewport::OnFocusViewportToSelection()
{
	const UDataflowEditorMode* const DataflowEdMode = GetEdMode();

	if (DataflowEdMode)
	{
		const FBox BoundingBox = DataflowEdMode->SelectionBoundingBox();
		if (BoundingBox.IsValid && !(BoundingBox.Min == FVector::Zero() && BoundingBox.Max == FVector::Zero()))
		{
			Client->FocusViewportOnBox(BoundingBox);
		}
	}
}

UDataflowEditorMode* SDataflowConstructionViewport::GetEdMode() const
{
	if (const FEditorModeTools* const EditorModeTools = Client->GetModeTools())
	{
		if (UDataflowEditorMode* const DataflowEdMode = Cast<UDataflowEditorMode>(EditorModeTools->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)))
		{
			return DataflowEdMode;
		}
	}
	return nullptr;
}

void SDataflowConstructionViewport::BindCommands()
{
	SAssetEditorViewport::BindCommands();

	const FDataflowEditorCommandsImpl& CommandInfos = FDataflowEditorCommands::Get();

	for (const TPair<FName, TSharedPtr<FUICommandInfo>>& SetViewModeCommand : CommandInfos.SetConstructionViewModeCommands)
	{
		CommandList->MapAction(
			SetViewModeCommand.Value,
			FExecuteAction::CreateLambda([this, ViewModeName = SetViewModeCommand.Key]()
			{
				if (UDataflowEditorMode* const EdMode = GetEdMode())
				{
					EdMode->SetConstructionViewMode(ViewModeName);
				}
			}),
			FCanExecuteAction::CreateLambda([this, ViewModeName = SetViewModeCommand.Key]()
			{ 
				if (const UDataflowEditorMode* const EdMode = GetEdMode())
				{
					return EdMode->CanChangeConstructionViewModeTo(ViewModeName);
				}
				return false; 
			}),
			FIsActionChecked::CreateLambda([this, ViewModeName = SetViewModeCommand.Key]()
			{
				if (const UDataflowEditorMode* const EdMode = GetEdMode())
				{
					return EdMode->GetConstructionViewMode()->GetName() == ViewModeName;
				}
				return false;
			})
		);
	}
}

bool SDataflowConstructionViewport::IsVisible() const
{
	// Intentionally not calling SEditorViewport::IsVisible because it will return false if our simulation is more than 250ms.
	return ViewportWidget.IsValid();
}

TSharedRef<class SEditorViewport> SDataflowConstructionViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SDataflowConstructionViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SDataflowConstructionViewport::OnFloatingButtonClicked()
{
}

void SDataflowConstructionViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);

	Overlay->AddSlot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.Padding(TAttribute<FMargin>(this, &SDataflowConstructionViewport::GetOverlayMargin))
		[
			SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("FloatingBorder"))
				.Padding(4.f)
				[
					SNew(SRichTextBlock)
					.Text(this, &SDataflowConstructionViewport::GetOverlayText)
				]
		];

	// this widget will display the current viewed feature level
	Overlay->AddSlot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Right)
		.Padding(5.0f)
		[
			BuildFeatureLevelWidget()
		];
}

FMargin SDataflowConstructionViewport::GetOverlayMargin() const
{
	return UE::UnrealEd::ShowOldViewportToolbars()
		? FMargin(6.0f, 36.0f, 6.0f, 6.0f) 
		: FMargin(6.0f, 6.0f, 6.0f, 6.0f);
}

FText SDataflowConstructionViewport::GetOverlayText() const
{
	const TSharedPtr<FDataflowConstructionViewportClient> DataflowClient = StaticCastSharedPtr<FDataflowConstructionViewportClient>(Client);
	if (DataflowClient)
	{
		return FText::FromString(DataflowClient->GetOverlayString());
	}

	return FText();
}

#undef LOCTEXT_NAMESPACE
