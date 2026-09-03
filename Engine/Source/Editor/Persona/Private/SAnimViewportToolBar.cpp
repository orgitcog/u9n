// Copyright Epic Games, Inc. All Rights Reserved.


#include "SAnimViewportToolBar.h"

#include "AnimViewportToolBarToolMenuContext.h"
#include "AnimPreviewInstance.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "EngineGlobals.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Engine.h"
#include "Styling/AppStyle.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Preferences/PersonaOptions.h"
#include "EditorViewportCommands.h"
#include "AnimViewportMenuCommands.h"
#include "AnimViewportShowCommands.h"
#include "AnimViewportPlaybackCommands.h"
#include "SEditorViewportToolBarMenu.h"
#include "STransformViewportToolbar.h"
#include "SEditorViewportViewMenu.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "AssetViewerSettings.h"
#include "PreviewSceneCustomizations.h"
#include "SimulationEditorExtender.h"
#include "ClothingSimulationFactory.h"
#include "ClothingSystemEditorInterfaceModule.h"
#include "Widgets/SWidget.h"
#include "Types/ISlateMetaData.h"
#include "Textures/SlateIcon.h"
#include "BufferVisualizationMenuCommands.h"
#include "IPinnedCommandList.h"
#include "UICommandList_Pinnable.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "Animation/MirrorDataTable.h"
#include "ScopedTransaction.h"
#include "ViewportToolbar/AnimationEditorMenus.h"
#include "ViewportToolbar/AnimationEditorWidgets.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"

#define LOCTEXT_NAMESPACE "AnimViewportToolBar"

///////////////////////////////////////////////////////////
// SAnimViewportToolBar

void SAnimViewportToolBar::Construct(
	const FArguments& InArgs,
	TSharedPtr<class SAnimationEditorViewportTabBody> InViewport,
	TSharedPtr<class SEditorViewport> InRealViewport
)
{
	bShowShowMenu = InArgs._ShowShowMenu;
	bShowCharacterMenu = InArgs._ShowCharacterMenu;
	bShowLODMenu = InArgs._ShowLODMenu;
	bShowPlaySpeedMenu = InArgs._ShowPlaySpeedMenu;
	bShowFloorOptions = InArgs._ShowFloorOptions;
	bShowTurnTable = InArgs._ShowTurnTable;
	bShowPhysicsMenu = InArgs._ShowPhysicsMenu;

	if (!InRealViewport)
	{
		return;
	}

	CommandList = InRealViewport->GetCommandList();
	Extenders = InArgs._Extenders;
	Extenders.Add(GetViewMenuExtender(InRealViewport));

	// If we have no extender, make an empty one
	if (Extenders.Num() == 0)
	{
		Extenders.Add(MakeShared<FExtender>());
	}

	const FMargin ToolbarSlotPadding(4.0f, 1.0f);
	const FMargin ToolbarButtonPadding(4.0f, 1.0f);


	// clang-format off
	TSharedRef<SHorizontalBox> LeftToolbar =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("ViewMenuTooltip", "View Options.\nShift-clicking items will 'pin' them to the toolbar."))
			.ParentToolBar(SharedThis(this))
			.Cursor(EMouseCursor::Default)
			.Image("EditorViewportToolBar.OptionsDropdown")
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.MenuDropdown")))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateViewMenu)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("ViewportMenuTooltip", "Viewport Options. Use this to switch between different orthographic or perspective views."))
			.ParentToolBar(SharedThis(this))
			.Cursor(EMouseCursor::Default)
			.Label(this, &SAnimViewportToolBar::GetCameraMenuLabel)
			.LabelIcon(this, &SAnimViewportToolBar::GetCameraMenuLabelIcon)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.CameraMenu")))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateViewportTypeMenu)
		]
		// View menu (lit, unlit, etc...)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportViewMenu, InRealViewport.ToSharedRef(), SharedThis(this))
			.ToolTipText(LOCTEXT("ViewModeMenuTooltip", "View Mode Options. Use this to change how the view is rendered, e.g. Lit/Unlit."))
			.MenuExtenders(FExtender::Combine(Extenders))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("ShowMenuTooltip", "Show Options. Use this enable/disable the rendering of types of scene elements."))
			.ParentToolBar(SharedThis(this))
			.Cursor(EMouseCursor::Default)
			.Label(LOCTEXT("ShowMenu", "Show"))
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewMenuButton")))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateShowMenu)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SPreviewSceneProfileSelector)
			.PreviewProfileController(InRealViewport->GetPreviewProfileController())
			.Visibility_Lambda(
				[ViewportWidgetWeak = InRealViewport.ToWeakPtr()]()
				{
					if (TSharedPtr<SEditorViewport> ViewportWidget = ViewportWidgetWeak.Pin())
					{
						// only show this menu if the user has customized it by adding their own profiles
						// this behavior was requested by UX to match the behavior of the static mesh editor
						if (TSharedPtr<IPreviewProfileController> PreviewProfileController = ViewportWidget->GetPreviewProfileController())
						{
							  return PreviewProfileController->HasAnyUserProfiles() ? EVisibility::Visible
																					: EVisibility::Collapsed;
						}
					}
					return EVisibility::Hidden;
				})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("PhysicsMenuTooltip", "Physics Options. Use this to control the physics of the scene."))
			.ParentToolBar(SharedThis(this))
			.Label(LOCTEXT("Physics", "Physics"))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GeneratePhysicsMenu)
			.Visibility(bShowPhysicsMenu ? EVisibility::Visible : EVisibility::Collapsed)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("CharacterMenuTooltip", "Character Options. Control character-related rendering options.\nShift-clicking items will 'pin' them to the toolbar."))
			.ParentToolBar(SharedThis(this))
			.Label(LOCTEXT("Character", "Character"))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateCharacterMenu)
			.Visibility(bShowCharacterMenu ? EVisibility::Visible : EVisibility::Collapsed)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			//LOD
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("LODMenuTooltip", "LOD Options. Control how LODs are displayed.\nShift-clicking items will 'pin' them to the toolbar."))
			.ParentToolBar(SharedThis(this))
			.Label(this, &SAnimViewportToolBar::GetLODMenuLabel)
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateLODMenu)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("PlaybackSpeedMenuTooltip", "Playback Speed Options. Control the time dilation of the scene's update.\nShift-clicking items will 'pin' them to the toolbar."))
			.ParentToolBar(SharedThis(this))
			.Label(this, &SAnimViewportToolBar::GetPlaybackMenuLabel)
			.LabelIcon(FAppStyle::GetBrush("AnimViewportMenu.PlayBackSpeed"))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GeneratePlaybackMenu)
		]
		+ SHorizontalBox::Slot()
		.Padding(ToolbarSlotPadding)
		.HAlign(HAlign_Right)
		[
			SNew(STransformViewportToolBar)
			.Viewport(InRealViewport)
			.CommandList(InRealViewport->GetCommandList())
			.Visibility(this, &SAnimViewportToolBar::GetTransformToolbarVisibility)
			.OnCamSpeedChanged(this, &SAnimViewportToolBar::OnCamSpeedChanged)
			.OnCamSpeedScalarChanged(this, &SAnimViewportToolBar::OnCamSpeedScalarChanged)
		];
	// clang-format on

	TWeakPtr<SAnimViewportToolBar> AnimViewportToolbarWeak = SharedThis(this);
	UE::UnrealEd::OnViewportClientCamSpeedChanged().BindLambda(
		[AnimViewportToolbarWeak, EditorViewportWeak = InRealViewport.ToWeakPtr()](const TSharedRef<SEditorViewport>& InEditorViewport, int32 InNewValue)
		{
			TSharedPtr<SAnimViewportToolBar> AnimViewportToolbar = AnimViewportToolbarWeak.Pin();
			if (!AnimViewportToolbar)
			{
				return;
			}

			if (TSharedPtr<SEditorViewport> EditorViewport = EditorViewportWeak.Pin())
			{
				if (EditorViewport == InEditorViewport)
				{
					AnimViewportToolbar->OnCamSpeedChanged(InNewValue);
				}
			}
		}
	);

	UE::UnrealEd::OnViewportClientCamSpeedScalarChanged().BindLambda(
		[AnimViewportToolbarWeak, EditorViewportWeak = InRealViewport.ToWeakPtr()](const TSharedRef<SEditorViewport>& InEditorViewport, float InNewValue)
		{
			TSharedPtr<SAnimViewportToolBar> AnimViewportToolbar = AnimViewportToolbarWeak.Pin();
			if (!AnimViewportToolbar)
			{
				return;
			}

			if (TSharedPtr<SEditorViewport> EditorViewport = EditorViewportWeak.Pin())
			{
				if (EditorViewport == InEditorViewport)
				{
					AnimViewportToolbar->OnCamSpeedScalarChanged(InNewValue);
				}
			}
		}
	);

	//@TODO: Need clipping horizontal box: LeftToolbar->AddWrapButton();

	TSharedPtr<IPinnedCommandList> PinnedCommands = InViewport->GetPinnedCommands();

	// clang-format off
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.Background"))
			.Cursor(EMouseCursor::Default)
			[
				LeftToolbar
			]
		]
	];
	// clang-format on

	SViewportToolBar::Construct(SViewportToolBar::FArguments());

	if (PinnedCommands)
	{
		// Register all the custom widgets we can use here
		PinnedCommands->RegisterCustomWidget(
			IPinnedCommandList::FOnGenerateCustomWidget::CreateSP(this, &SAnimViewportToolBar::MakeFloorOffsetWidget),
			TEXT("FloorOffsetWidget"),
			LOCTEXT("FloorHeightOffset", "Floor Height Offset")
		);
		PinnedCommands->RegisterCustomWidget(
			IPinnedCommandList::FOnGenerateCustomWidget::CreateSP(this, &SAnimViewportToolBar::MakeFOVWidget),
			TEXT("FOVWidget"),
			LOCTEXT("Viewport_FOVLabel", "Field Of View")
		);
	}

	// We assign the viewport pointer here rather than initially, as SViewportToolbar::Construct
	// ends up calling through and attempting to perform operations on the not-yet-full-constructed viewport
	Viewport = InViewport;
}

EVisibility SAnimViewportToolBar::GetTransformToolbarVisibility() const
{
	return Viewport.Pin()->CanUseGizmos() ? EVisibility::Visible : EVisibility::Hidden;
}

TSharedRef<SWidget> SAnimViewportToolBar::MakeFloorOffsetWidget() const
{
	return UE::AnimationEditor::MakeFloorOffsetWidget(Viewport);
}

TSharedRef<SWidget> SAnimViewportToolBar::MakeFOVWidget() const
{
	const float FOVMin = 5.f;
	const float FOVMax = 170.f;

	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew(SNumericEntryBox<float>)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.AllowSpin(true)
				.MinValue(FOVMin)
				.MaxValue(FOVMax)
				.MinSliderValue(FOVMin)
				.MaxSliderValue(FOVMax)
				.Value(this, &SAnimViewportToolBar::OnGetFOVValue)
				.OnValueChanged(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::OnFOVValueChanged)
				.OnValueCommitted(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::OnFOVValueCommitted)
			]
		];
}

TSharedRef<SWidget> SAnimViewportToolBar::MakeFollowBoneComboWidget() const
{
	TSharedRef<SComboButton> ComboButton = SNew(SComboButton)
		.ComboButtonStyle(FAppStyle::Get(), "ViewportPinnedCommandList.ComboButton")
		.ContentPadding(0.0f)
		.ButtonContent()
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), "ViewportPinnedCommandList.Label")
			.Text_Lambda([this]()
			{ 
				const FName BoneName = Viewport.Pin()->GetCameraFollowBoneName();
				if(BoneName != NAME_None)
				{
					return FText::Format(LOCTEXT("FollowingBoneMenuTitleFormat", "Following Bone: {0}"), FText::FromName(BoneName));
				}
				else
				{
					return LOCTEXT("FollowBoneMenuTitle", "Focus On Bone");
				}
			})
		];

	TWeakPtr<SComboButton> WeakComboButton = ComboButton;
	ComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda(
		[ViewportWeak = Viewport, WeakComboButton]()
		{
			return UE::AnimationEditor::MakeFollowBoneWidget(ViewportWeak, WeakComboButton);
		}
	));

	return ComboButton;
}

TSharedRef<SWidget> SAnimViewportToolBar::MakeFollowBoneWidget() const
{
	return UE::AnimationEditor::MakeFollowBoneWidget(Viewport, nullptr);
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateViewMenu() const
{
	const FAnimViewportMenuCommands& Actions = FAnimViewportMenuCommands::Get();

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());

	InMenuBuilder.BeginSection("AnimViewportSceneSetup", LOCTEXT("ViewMenu_SceneSetupLabel", "Scene Setup"));
	{
		InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().PreviewSceneSettings);
		InMenuBuilder.PopCommandList();

		if(bShowFloorOptions)
		{
			InMenuBuilder.AddWidget(MakeFloorOffsetWidget(), LOCTEXT("FloorHeightOffset", "Floor Height Offset"));

			InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
			InMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().AutoAlignFloorToMesh);
			InMenuBuilder.PopCommandList();
		}

		if (bShowTurnTable)
		{
			InMenuBuilder.AddSubMenu(
				LOCTEXT("TurnTableLabel", "Turn Table"),
				LOCTEXT("TurnTableTooltip", "Set up auto-rotation of preview."),
				FNewMenuDelegate::CreateRaw(this, &SAnimViewportToolBar::GenerateTurnTableMenu),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "AnimViewportMenu.TurnTableSpeed")
				);
		}
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.BeginSection("AnimViewportCamera", LOCTEXT("ViewMenu_CameraLabel", "Camera"));
	{
		InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().FocusViewportToSelection);
		InMenuBuilder.AddWidget(MakeFOVWidget(), LOCTEXT("Viewport_FOVLabel", "Field Of View"));
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().TogglePauseAnimationOnCameraMove);

		InMenuBuilder.AddSubMenu(
			LOCTEXT("CameraFollowModeLabel", "Follow Mode"),
			LOCTEXT("CameraFollowModeTooltip", "Set various camera follow modes"),
			FNewMenuDelegate::CreateLambda([ViewportWeak = Viewport](FMenuBuilder& InSubMenuBuilder)
			{
				InSubMenuBuilder.BeginSection(NAME_None, FText());
				{
					InSubMenuBuilder.AddWidget(UE::AnimationEditor::CreateFollowModeMenuWidget(ViewportWeak), FText());
				}
				InSubMenuBuilder.EndSection();
			}),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "AnimViewportMenu.CameraFollow")
			);
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.BeginSection("AnimViewportDefaultCamera", LOCTEXT("ViewMenu_DefaultCameraLabel", "Default Camera"));
	{
		InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().JumpToDefaultCamera);
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SaveCameraAsDefault);
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().ClearDefaultCamera);
		InMenuBuilder.PopCommandList();
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GeneratePhysicsMenu() const
{
	return UE::AnimationEditor::GeneratePhysicsMenuWidget(Viewport, FExtender::Combine(Extenders));
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateCharacterMenu() const
{
	static const FName MenuName("Persona.AnimViewportCharacterMenu");
	if (!UToolMenus::Get()->IsMenuRegistered(MenuName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(MenuName);
		{
			FToolMenuSection& Section = Menu->AddSection("AnimViewportSceneElements", LOCTEXT("CharacterMenu_SceneElements", "Scene Elements"));
			Section.AddSubMenu(TEXT("MeshSubMenu"),
				LOCTEXT("CharacterMenu_MeshSubMenu", "Mesh"),
				LOCTEXT("CharacterMenu_MeshSubMenuToolTip", "Mesh-related options"),
				FNewToolMenuDelegate::CreateLambda([](UToolMenu* InSubMenu)
				{
					{
						FToolMenuSection& Section = InSubMenu->AddSection("AnimViewportMesh", LOCTEXT("CharacterMenu_Actions_Mesh", "Mesh"));
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBound);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().UseInGameBound);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().UseFixedBounds);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().UsePreSkinnedBounds);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowPreviewMesh);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowMorphTargets);
					}
					{
						FToolMenuSection& Section = InSubMenu->AddSection("AnimViewportMeshInfo", LOCTEXT("CharacterMenu_Actions_MeshInfo", "Mesh Info"));
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowDisplayInfoBasic);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowDisplayInfoDetailed);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowDisplayInfoSkelControls);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().HideDisplayInfo);
					}
					{
						FToolMenuSection& Section = InSubMenu->AddSection("AnimViewportPreviewOverlayDraw", LOCTEXT("CharacterMenu_Actions_Overlay", "Mesh Overlay Drawing"));
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowOverlayNone);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneWeight);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowMorphTargetVerts);
					}
				})
			);
			Section.AddSubMenu(TEXT("AnimationSubMenu"),
				LOCTEXT("CharacterMenu_AnimationSubMenu", "Animation"),
				LOCTEXT("CharacterMenu_AnimationSubMenuToolTip", "Animation-related options"),
				FNewToolMenuDelegate::CreateLambda([](UToolMenu* InSubMenu)
			{
					UAnimViewportToolBarToolMenuContext* Context = InSubMenu->FindContext<UAnimViewportToolBarToolMenuContext>();
					const SAnimViewportToolBar* ContextThis = Context ? Context->AnimViewportToolBar.Pin().Get() : nullptr;

					{
						FToolMenuSection& Section = InSubMenu->AddSection("AnimViewportRootMotion", LOCTEXT("CharacterMenu_RootMotionLabel", "Root Motion"));
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().DoNotProcessRootMotion);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ProcessRootMotionLoop);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ProcessRootMotionLoopAndReset);
					}

					{
						FToolMenuSection& Section = InSubMenu->AddSection("AnimViewportVisualization", LOCTEXT("CharacterMenu_VisualizationsLabel", "Visualizations"));
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowNotificationVisualizations);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().DoNotVisualizeRootMotion);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().VisualizeRootMotionTrajectory);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().VisualizeRootMotionTrajectoryAndOrientation);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowAssetUserDataVisualizations);
					}

					{
						FToolMenuSection& Section = InSubMenu->AddSection("AnimViewportAnimation", LOCTEXT("CharacterMenu_Actions_AnimationAsset", "Animation"));
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowRawAnimation);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowNonRetargetedAnimation);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowAdditiveBaseBones);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowSourceRawAnimation);

						if (ContextThis && ContextThis->Viewport.IsValid())
						{
							if ( UDebugSkelMeshComponent* PreviewComponent = ContextThis->Viewport.Pin()->GetPreviewScene()->GetPreviewMeshComponent())
							{
								FUIAction DisableUnlessPreviewInstance(
									FExecuteAction::CreateLambda([](){}),
									FCanExecuteAction::CreateLambda([PreviewComponent]()
									{
										return (PreviewComponent->PreviewInstance && (PreviewComponent->PreviewInstance == PreviewComponent->GetAnimInstance() ) );
									})
								);

								Section.AddSubMenu(TEXT("MirrorSubMenu"),
									LOCTEXT("CharacterMenu_AnimationSubMenu_MirrorSubMenu", "Mirror"),
									LOCTEXT("CharacterMenu_AnimationSubMenu_MirrorSubMenuToolTip", "Mirror the animation using the selected mirror data table"),
									FNewToolMenuChoice(FNewMenuDelegate::CreateRaw(ContextThis, &SAnimViewportToolBar::FillCharacterMirrorMenu)),
									FToolUIActionChoice(DisableUnlessPreviewInstance),
									EUserInterfaceActionType::Button,
									false,
									FSlateIcon(),
									false);
							}
						}
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBakedAnimation);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().DisablePostProcessBlueprint);
					}
					if (ContextThis && ContextThis->Viewport.IsValid())
					{
						FToolMenuSection& Section = InSubMenu->AddSection("SkinWeights", LOCTEXT("SkinWeights_Label", "Skin Weight Profiles"));
						Section.AddEntry(FToolMenuEntry::InitWidget(TEXT("SkinWeightCombo"), ContextThis->Viewport.Pin()->SkinWeightCombo.ToSharedRef(), FText()));
					}
				})
			);
			
			Section.AddSubMenu(TEXT("BonesSubMenu"),
				LOCTEXT("CharacterMenu_BoneDrawSubMenu", "Bones"),
				LOCTEXT("CharacterMenu_BoneDrawSubMenuToolTip", "Bone Drawing Options"),
				FNewToolMenuDelegate::CreateLambda([](UToolMenu* InSubMenu)
				{
					UAnimViewportToolBarToolMenuContext* Context = InSubMenu->FindContext<UAnimViewportToolBarToolMenuContext>();
					const SAnimViewportToolBar* ContextThis = Context ? Context->AnimViewportToolBar.Pin().Get() : nullptr;
					{
						FToolMenuSection& Section = InSubMenu->AddSection("BonesAndSockets", LOCTEXT("CharacterMenu_BonesAndSocketsLabel", "Show"));
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowSockets);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowAttributes);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneNames);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneColors);
					}

					{
						FToolMenuSection& Section = InSubMenu->AddSection("AnimViewportPreviewHierarchyBoneDraw", LOCTEXT("CharacterMenu_Actions_BoneDrawing", "Bone Drawing"));
						if (ContextThis)
						{
							TSharedPtr<SWidget> BoneSizeWidget = SNew(UE::AnimationEditor::SBoneDrawSizeSetting).AnimEditorViewport(ContextThis->Viewport);
							Section.AddEntry(FToolMenuEntry::InitWidget(TEXT("BoneDrawSize"), BoneSizeWidget.ToSharedRef(), LOCTEXT("CharacterMenu_Actions_BoneDrawSize", "Bone Draw Size:")));
						}
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawAll);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawSelected);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawSelectedAndParents);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawSelectedAndChildren);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawSelectedAndParentsAndChildren);
						Section.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawNone);
					}
				})
				);

			Section.AddDynamicEntry("ClothingSubMenu", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				UAnimViewportToolBarToolMenuContext* Context = InSection.FindContext<UAnimViewportToolBarToolMenuContext>();
				const SAnimViewportToolBar* ContextThis = Context ? Context->AnimViewportToolBar.Pin().Get() : nullptr; 
				UDebugSkelMeshComponent* PreviewComp = ContextThis && ContextThis->Viewport.IsValid() ? ContextThis->Viewport.Pin()->GetPreviewScene()->GetPreviewMeshComponent() : nullptr;
				if (PreviewComp && GetDefault<UPersonaOptions>()->bExposeClothingSceneElementMenu)
				{
					constexpr bool bInOpenSubMenuOnClick = false;
					constexpr bool bShouldCloseWindowAfterMenuSelection = false;
					InSection.AddSubMenu(TEXT("ClothingSubMenu"),
						LOCTEXT("CharacterMenu_ClothingSubMenu", "Clothing"),
						LOCTEXT("CharacterMenu_ClothingSubMenuToolTip", "Options relating to clothing"),
						FNewToolMenuChoice(FNewMenuDelegate::CreateRaw(const_cast<SAnimViewportToolBar *>(ContextThis), &SAnimViewportToolBar::FillCharacterClothingMenu)),
						bInOpenSubMenuOnClick, TAttribute<FSlateIcon>(), bShouldCloseWindowAfterMenuSelection);
				}
			}));

			Section.AddSubMenu(TEXT("AudioSubMenu"),
				LOCTEXT("CharacterMenu_AudioSubMenu", "Audio"),
				LOCTEXT("CharacterMenu_AudioSubMenuToolTip", "Audio options"),
				FNewToolMenuDelegate::CreateLambda([](UToolMenu* InSubMenu)
			{			
				FToolMenuSection& Section = InSubMenu->AddSection("AnimViewportAudio", LOCTEXT("CharacterMenu_Audio", "Audio"));
				Section.AddMenuEntry(FAnimViewportShowCommands::Get().MuteAudio);
				Section.AddMenuEntry(FAnimViewportShowCommands::Get().UseAudioAttenuation);
			}));

			Section.AddDynamicEntry("AdvancedSubMenu", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				UAnimViewportToolBarToolMenuContext* Context = InSection.FindContext<UAnimViewportToolBarToolMenuContext>();
				const SAnimViewportToolBar* ContextThis = Context ? Context->AnimViewportToolBar.Pin().Get() : nullptr;
				if (ContextThis)
				{
					InSection.AddSubMenu(TEXT("AdvancedSubMenu"),
						LOCTEXT("CharacterMenu_AdvancedSubMenu", "Advanced"),
						LOCTEXT("CharacterMenu_AdvancedSubMenuToolTip", "Advanced options"),
						FNewToolMenuChoice(FNewMenuDelegate::CreateRaw(ContextThis, &SAnimViewportToolBar::FillCharacterAdvancedMenu)));
				}
			}));

			Section.AddDynamicEntry("Timecode", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				UAnimViewportToolBarToolMenuContext* Context = InSection.FindContext<UAnimViewportToolBarToolMenuContext>();
				const SAnimViewportToolBar* ContextThis = Context ? Context->AnimViewportToolBar.Pin().Get() : nullptr;
				if (ContextThis)
				{
					InSection.AddSubMenu(TEXT("TimecodeSubMenu"),
						LOCTEXT("CharacterMenu_TimecodeSubMenu", "Timecode"),
						LOCTEXT("CharacterMenu_TimecodeSubMenuToolTip", "Timecode options"),
						FNewToolMenuChoice(FNewMenuDelegate::CreateRaw(ContextThis, &SAnimViewportToolBar::FillCharacterTimecodeMenu)));
				}
			}));
		}
	}

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);
	TSharedPtr<SAnimationEditorViewportTabBody> PinnedViewport = Viewport.Pin();
	FToolMenuContext MenuContext(PinnedViewport->GetCommandList(), MenuExtender);
	PinnedViewport->GetAssetEditorToolkit()->InitToolMenuContext(MenuContext);
	UAnimViewportToolBarToolMenuContext* AnimViewportContext = NewObject<UAnimViewportToolBarToolMenuContext>();
	AnimViewportContext->AnimViewportToolBar = SharedThis(this);
	MenuContext.AddObject(AnimViewportContext);
	return UToolMenus::Get()->GenerateWidget(MenuName, MenuContext);
}

void SAnimViewportToolBar::FillCharacterTimecodeMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();
	MenuBuilder.BeginSection("Timecode", LOCTEXT("Timecode_Label", "Timecode"));
	{
		MenuBuilder.AddMenuEntry( Actions.ShowTimecode );
	}
	MenuBuilder.EndSection();
}

void SAnimViewportToolBar::FillCharacterAdvancedMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	// Draw UVs
	MenuBuilder.BeginSection("UVVisualization", LOCTEXT("UVVisualization_Label", "UV Visualization"));
	{
		MenuBuilder.AddWidget(Viewport.Pin()->UVChannelCombo.ToSharedRef(), FText());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Skinning", LOCTEXT("Skinning_Label", "Skinning"));
	{
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetCPUSkinning);
	}
	MenuBuilder.EndSection();
	

	MenuBuilder.BeginSection("ShowVertex", LOCTEXT("ShowVertex_Label", "Vertex Normal Visualization"));
	{
		// Vertex debug flags
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowNormals);
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowTangents);
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowBinormals);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportPreviewHierarchyLocalAxes", LOCTEXT("ShowMenu_Actions_HierarchyAxes", "Hierarchy Local Axes") );
	{
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesAll );
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesSelected );
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesNone );
	}
	MenuBuilder.EndSection();
}

void SAnimViewportToolBar::FillCharacterMirrorMenu(FMenuBuilder& MenuBuilder) const
{
	FAssetPickerConfig AssetPickerConfig;
	UDebugSkelMeshComponent* PreviewComp = Viewport.Pin()->GetPreviewScene()->GetPreviewMeshComponent();
	USkeletalMesh* Mesh = PreviewComp->GetSkeletalMeshAsset();
	UAnimPreviewInstance* PreviewInstance = PreviewComp->PreviewInstance; 
	if (Mesh && PreviewInstance)
	{
		USkeleton* Skeleton = Mesh->GetSkeleton();
	
		AssetPickerConfig.Filter.ClassPaths.Add(UMirrorDataTable::StaticClass()->GetClassPathName());
		AssetPickerConfig.Filter.bRecursiveClasses = false;
		AssetPickerConfig.bAllowNullSelection = true;
		AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateUObject(Skeleton, &USkeleton::ShouldFilterAsset, TEXT("Skeleton"));
		AssetPickerConfig.InitialAssetSelection = FAssetData(PreviewInstance->GetMirrorDataTable());
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::OnMirrorDataTableSelected);
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.ThumbnailScale = 0.1f;
		AssetPickerConfig.bAddFilterUI = false;
		
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		MenuBuilder.AddWidget(
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig),
			FText::GetEmpty()
		);
	}
}

void SAnimViewportToolBar::OnMirrorDataTableSelected(const FAssetData& SelectedMirrorTableData)
{
	UMirrorDataTable* MirrorDataTable = Cast<UMirrorDataTable>(SelectedMirrorTableData.GetAsset());
	if (Viewport.Pin().IsValid())
	{
		UDebugSkelMeshComponent* PreviewComp = Viewport.Pin()->GetPreviewScene()->GetPreviewMeshComponent();
		USkeletalMesh* Mesh = PreviewComp->GetSkeletalMeshAsset();
		UAnimPreviewInstance* PreviewInstance = PreviewComp->PreviewInstance; 
		if (Mesh && PreviewInstance)
		{
			PreviewInstance->SetMirrorDataTable(MirrorDataTable);
			PreviewComp->OnMirrorDataTableChanged();
		}
	}
}

void SAnimViewportToolBar::FillCharacterClothingMenu(FMenuBuilder& MenuBuilder)
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	MenuBuilder.BeginSection("ClothPreview", LOCTEXT("ClothPreview_Label", "Simulation"));
	{
		MenuBuilder.AddMenuEntry(Actions.EnableClothSimulation);
		MenuBuilder.AddMenuEntry(Actions.ResetClothSimulation);

		TSharedPtr<SWidget> WindWidget = SNew(UE::AnimationEditor::SClothWindSettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(WindWidget.ToSharedRef(), LOCTEXT("ClothPreview_WindStrength", "Wind Strength:"));

		TSharedPtr<SWidget> GravityWidget = SNew(UE::AnimationEditor::SGravitySettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(GravityWidget.ToSharedRef(), LOCTEXT("ClothPreview_GravityScale", "Gravity Scale:"));

		MenuBuilder.AddMenuEntry(Actions.EnableCollisionWithAttachedClothChildren);
		MenuBuilder.AddMenuEntry(Actions.PauseClothWithAnim);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ClothAdditionalVisualization", LOCTEXT("ClothAdditionalVisualization_Label", "Sections Display Mode"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowAllSections);
		MenuBuilder.AddMenuEntry(Actions.ShowOnlyClothSections);
		MenuBuilder.AddMenuEntry(Actions.HideOnlyClothSections);
	}
	MenuBuilder.EndSection();

	// Call into the clothing editor module to customize the menu (this is mainly for debug visualizations and sim-specific options)
	TSharedPtr<SAnimationEditorViewportTabBody> SharedViewport = Viewport.Pin();
	if (SharedViewport.IsValid())
	{
		if (const TSharedPtr<FAnimationViewportClient>& AnimationViewportClient = SharedViewport->GetAnimationViewportClient())
		{
			TSharedRef<IPersonaPreviewScene> PreviewScene = AnimationViewportClient->GetPreviewScene();
			if (UDebugSkelMeshComponent* PreviewComponent = PreviewScene->GetPreviewMeshComponent())
			{
				if (PreviewComponent->ClothingSimulationFactory)  // The cloth plugin could be disabled, and the factory would be null in this case
				{
					FClothingSystemEditorInterfaceModule& ClothingEditorModule = FModuleManager::LoadModuleChecked<FClothingSystemEditorInterfaceModule>(TEXT("ClothingSystemEditorInterface"));

					if (ISimulationEditorExtender* Extender = ClothingEditorModule.GetSimulationEditorExtender(PreviewComponent->ClothingSimulationFactory->GetFName()))
					{
						Extender->ExtendViewportShowMenu(MenuBuilder, PreviewScene);
					}
				}
			}
		}
	}
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateShowMenu() const
{
	if (TSharedPtr<SAnimationEditorViewportTabBody> ViewportPinned = Viewport.Pin())
	{
		constexpr bool bShowViewportStatsToggle = false;
		return UE::AnimationEditor::CreateShowMenuWidget(
			ViewportPinned->GetViewportWidget().ToSharedRef(), Extenders, bShowViewportStatsToggle
		);
	}

	return SNullWidget::NullWidget;
}

FText SAnimViewportToolBar::GetLODMenuLabel() const
{
	return UE::AnimationEditor::GetLODMenuLabel(Viewport);
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateLODMenu() const
{
	return UE::AnimationEditor::GenerateLODMenuWidget(Viewport);
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateViewportTypeMenu() const
{
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList, MenuExtender);
	InMenuBuilder.SetStyle(&FAppStyle::Get(), "Menu");
	InMenuBuilder.PushCommandList(CommandList.ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());

	// Camera types
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

	InMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Orthographic"));
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
	InMenuBuilder.EndSection();

	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GeneratePlaybackMenu() const
{
	if (const TSharedPtr<SAnimationEditorViewportTabBody>& ViewportTabPinned = Viewport.Pin())
	{
		return UE::AnimationEditor::GeneratePlaybackMenu(ViewportTabPinned->GetPreviewScene(), Extenders);
	}

	return SNullWidget::NullWidget;
}

void SAnimViewportToolBar::GenerateTurnTableMenu(FMenuBuilder& MenuBuilder) const
{
	MenuBuilder.AddWidget(
		UE::AnimationEditor::GenerateTurnTableMenu(Viewport),
		FText(),
		false,
		true,
		LOCTEXT("TurnTableTooltip", "Set up auto-rotation of preview.")
	);
}

FSlateColor SAnimViewportToolBar::GetFontColor() const
{
	const UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
	const UEditorPerProjectUserSettings* PerProjectUserSettings = GetDefault<UEditorPerProjectUserSettings>();
	const int32 ProfileIndex = Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex) ? PerProjectUserSettings->AssetViewerProfileIndex : 0;

	ensureMsgf(Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex), TEXT("Invalid default settings pointer or current profile index"));

	FLinearColor FontColor;
	if (Settings->Profiles[ProfileIndex].bShowEnvironment)
	{
		FontColor = FLinearColor::White;
	}
	else
	{
		FLinearColor Color = Settings->Profiles[ProfileIndex].EnvironmentColor * Settings->Profiles[ProfileIndex].EnvironmentIntensity;

		// see if it's dark, if V is less than 0.2
		if (Color.B < 0.3f )
		{
			FontColor = FLinearColor::White;
		}
		else
		{
			FontColor = FLinearColor::Black;
		}
	}

	return FontColor;
}

FText SAnimViewportToolBar::GetPlaybackMenuLabel() const
{
	FText Label = LOCTEXT("PlaybackError", "Error");
	if (const TSharedPtr<SAnimationEditorViewportTabBody>& ViewportTabPinned = Viewport.Pin())
	{
		return UE::AnimationEditor::GetPlaybackMenuLabel(ViewportTabPinned->GetPreviewScene());
	}
	return Label;
}

FText SAnimViewportToolBar::GetCameraMenuLabel() const
{
	TSharedPtr< SAnimationEditorViewportTabBody > PinnedViewport(Viewport.Pin());
	if( PinnedViewport.IsValid() )
	{
		return UE::UnrealEd::GetCameraSubmenuLabelFromViewportType(PinnedViewport->GetLevelViewportClient().ViewportType);
	}

	return LOCTEXT("Viewport_Default", "Camera");
}

const FSlateBrush* SAnimViewportToolBar::GetCameraMenuLabelIcon() const
{
	TSharedPtr< SAnimationEditorViewportTabBody > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		return GetCameraMenuLabelIconFromViewportType(PinnedViewport->GetLevelViewportClient().ViewportType );
	}

	return FAppStyle::Get().GetBrush("NoBrush");
}

TOptional<float> SAnimViewportToolBar::OnGetFOVValue() const
{
	if(Viewport.IsValid())
	{
		return Viewport.Pin()->GetLevelViewportClient().ViewFOV;
	}
	return 0.0f;
}

void SAnimViewportToolBar::OnFOVValueChanged( float NewValue )
{
	FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();

	ViewportClient.FOVAngle = NewValue;

	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)(ViewportClient);
	AnimViewportClient.ConfigOption->SetViewFOV(AnimViewportClient.GetAssetEditorToolkit()->GetEditorName(), NewValue, AnimViewportClient.GetViewportIndex());

	ViewportClient.ViewFOV = NewValue;
	ViewportClient.Invalidate();

	if (TSharedPtr<SAnimationEditorViewportTabBody> ViewportTabPinned = Viewport.Pin())
	{
		if (TSharedPtr<IPinnedCommandList> CommandsPinned = ViewportTabPinned->GetPinnedCommands())
		{
			CommandsPinned->AddCustomWidget(TEXT("FOVWidget"));
		}
	}
}

void SAnimViewportToolBar::OnFOVValueCommitted( float NewValue, ETextCommit::Type CommitInfo )
{
	//OnFOVValueChanged will be called... nothing needed here.
}

void SAnimViewportToolBar::OnCamSpeedChanged(int32 NewValue)
{
	FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)(ViewportClient);
	AnimViewportClient.ConfigOption->SetCameraSpeed(AnimViewportClient.GetAssetEditorToolkit()->GetEditorName(), NewValue, AnimViewportClient.GetViewportIndex());
}

void SAnimViewportToolBar::OnCamSpeedScalarChanged(float NewValue)
{
	FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)(ViewportClient);
	AnimViewportClient.ConfigOption->SetCameraSpeedScalar(AnimViewportClient.GetAssetEditorToolkit()->GetEditorName(), NewValue, AnimViewportClient.GetViewportIndex());
}

void SAnimViewportToolBar::AddMenuExtender(FName MenuToExtend, FMenuExtensionDelegate MenuBuilderDelegate)
{
	TSharedRef<FExtender> Extender(new FExtender());
	Extender->AddMenuExtension(
		MenuToExtend,
		EExtensionHook::After,
		CommandList,
		MenuBuilderDelegate
	);
	Extenders.Add(Extender);
}

TSharedRef<FExtender> SAnimViewportToolBar::GetViewMenuExtender(TSharedPtr<class SEditorViewport> InRealViewport)
{
	TSharedRef<FExtender> Extender(new FExtender());

	Extender->AddMenuExtension(
		TEXT("ViewMode"),
		EExtensionHook::After,
		InRealViewport->GetCommandList(),
		FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& InMenuBuilder)
		{
			InMenuBuilder.AddSubMenu(
				LOCTEXT("VisualizeBufferViewModeDisplayName", "Buffer Visualization"),
				LOCTEXT("BufferVisualizationMenu_ToolTip", "Select a mode for buffer visualization"),
				FNewMenuDelegate::CreateStatic(&FBufferVisualizationMenuCommands::BuildVisualisationSubMenu),
				FUIAction(
					FExecuteAction(),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([this]()
					{
						const TSharedPtr<SAnimationEditorViewportTabBody> ViewportPtr = Viewport.Pin();
						if (ViewportPtr.IsValid())
						{
							const FEditorViewportClient& ViewportClient = ViewportPtr->GetViewportClient();
							return ViewportClient.IsViewModeEnabled(VMI_VisualizeBuffer);
						}
						return false;
					})
				),
				"VisualizeBufferViewMode",
				EUserInterfaceActionType::RadioButton,
				/* bInOpenSubMenuOnClick = */ false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeBufferMode")
				);
		}));

	return Extender;
}

#undef LOCTEXT_NAMESPACE
