// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowSimulationViewportToolbar.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowPreviewProfileController.h"
#include "Dataflow/DataflowSimulationViewport.h"
#include "Dataflow/DataflowSimulationViewportClient.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowSceneProfileIndexStorage.h"
#include "Dataflow/DataflowSimulationScene.h"
#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SEditorViewportToolBarMenu.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "LODSyncInterface.h"
#include "SEditorViewportViewMenu.h"

#define LOCTEXT_NAMESPACE "DataflowSimulationViewportToolBar"

void SDataflowSimulationViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<SDataflowSimulationViewport> InDataflowViewport)
{
	EditorViewport = InDataflowViewport;
	CommandList = InArgs._CommandList;
	Extenders = InArgs._Extenders;

	TSharedPtr<FDataflowSimulationSceneProfileIndexStorage> ProfileIndexStorage;

	if (const TSharedPtr<SDataflowSimulationViewport> Viewport = EditorViewport.Pin())
	{
		if (const TSharedPtr<FDataflowSimulationViewportClient> ViewportClient = StaticCastSharedPtr<FDataflowSimulationViewportClient>(Viewport->GetViewportClient()))
		{
			if (ViewportClient->GetDataflowEditorToolkit().IsValid())
			{
				if (const TSharedPtr<FDataflowEditorToolkit> Toolkit = ViewportClient->GetDataflowEditorToolkit().Pin())
				{
					ProfileIndexStorage = Toolkit->GetSimulationSceneProfileIndexStorage();
				}
			}
		}
	}
	
	if (!ProfileIndexStorage)
	{
		ProfileIndexStorage = MakeShared<FDataflowSimulationSceneProfileIndexStorage>(InDataflowViewport->GetSimulationScene().Get());
	}
	
	SCommonEditorViewportToolbarBase::FArguments Args = SCommonEditorViewportToolbarBase::FArguments().
		PreviewProfileController(MakeShared<FDataflowPreviewProfileController>(ProfileIndexStorage));

	SCommonEditorViewportToolbarBase::Construct(Args, InDataflowViewport);


	const FMargin ToolbarSlotPadding(4.0f, 1.0f);
	TSharedPtr<SHorizontalBox> MainBox;

	ChildSlot
	[
		SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.Background"))
			.Cursor(EMouseCursor::Default)
			[
				SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(MainBox, SHorizontalBox)
					]
			]
	];

	// Options menu
	MainBox->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Cursor(EMouseCursor::Default)
				.Image("EditorViewportToolBar.OptionsDropdown")
				.OnGetMenuContent(this, &SDataflowSimulationViewportToolBar::GenerateOptionsMenu)
		];

	// Display (Lit, Unlit, Wireframe, etc.)
	const TSharedRef<SEditorViewport> ViewportRef = StaticCastSharedPtr<SEditorViewport>(EditorViewport.Pin()).ToSharedRef();
	MainBox->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportViewMenu, ViewportRef, SharedThis(this))
				.Cursor(EMouseCursor::Default)
				.MenuExtenders(Extenders)
		];

	// Preview Profile selector
	MainBox->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SPreviewSceneProfileSelector).PreviewProfileController(
				MakeShared<FDataflowPreviewProfileController>(ProfileIndexStorage))
		];

	// Add optional toolbar slots to be added by child classes inherited from this common viewport toolbar
	ExtendLeftAlignedToolbarSlots(MainBox, SharedThis(this));

}

void SDataflowSimulationViewportToolBar::ExtendLeftAlignedToolbarSlots(TSharedPtr<SHorizontalBox> MainBoxPtr, TSharedPtr<SViewportToolBar> ParentToolBarPtr) const
{
	const TSharedPtr<class FDataflowSimulationScene>& SimulationScene = EditorViewport.Pin()->GetSimulationScene();

	auto HasCacheAsset = [SimulationScene]()
	{
		if(SimulationScene && SimulationScene->GetPreviewSceneDescription())
		{
			return (SimulationScene->GetPreviewSceneDescription()->CacheAsset == nullptr) ? EVisibility::Visible : EVisibility::Collapsed;
		}
		return EVisibility::Collapsed;
	};
	
	const FMargin ToolbarSlotPadding(2.0f, 2.0f);
	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		.HAlign(HAlign_Left)
		[
			SNew(SEditorViewportToolbarMenu)
				.Label(this, &SDataflowSimulationViewportToolBar::LODButtonLabel)
				.ParentToolBar(ParentToolBarPtr)
				.OnGetMenuContent(this, &SDataflowSimulationViewportToolBar::MakeLODMenu)
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SDataflowSimulationViewportToolBar::LODButtonVisibility)))
		];

	MainBoxPtr->AddSlot()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SBox)
			.Visibility(TAttribute<EVisibility>::Create(HasCacheAsset))
			[
				MakeToolBar(Extenders)
			]
		];
}

TSharedRef<SWidget> SDataflowSimulationViewportToolBar::MakeToolBar(const TSharedPtr<FExtender> InExtenders) const
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(CommandList, FMultiBoxCustomization::None, InExtenders);
	
	const FName ToolBarStyle = "EditorViewportToolBar";
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	ToolbarBuilder.BeginSection("Sim Controls");
	ToolbarBuilder.BeginBlockGroup();
	{
		ToolbarBuilder.AddToolBarButton(FDataflowEditorCommands::Get().RebuildSimulationScene,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Animation.Backward_End"),
			FName(*FDataflowEditorCommands::Get().RebuildSimulationSceneIdentifier));

		ToolbarBuilder.AddToolBarButton(FDataflowEditorCommands::Get().PauseSimulationScene,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Animation.Pause"),
			FName(*FDataflowEditorCommands::Get().PauseSimulationSceneIdentifier));

		ToolbarBuilder.AddToolBarButton(FDataflowEditorCommands::Get().StartSimulationScene,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Play"),
			FName(*FDataflowEditorCommands::Get().StartSimulationSceneIdentifier));

		ToolbarBuilder.AddToolBarButton(FDataflowEditorCommands::Get().StepSimulationScene,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Animation.Forward_Step"),
			FName(*FDataflowEditorCommands::Get().StepSimulationSceneIdentifier));
	}
	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

EVisibility SDataflowSimulationViewportToolBar::LODButtonVisibility() const
{
	const TSharedPtr<SDataflowSimulationViewport> Viewport = EditorViewport.Pin();
	checkf(Viewport, TEXT("Viewport not found in ViewportToolBar"));

	return (NumLODs() > 1) ? EVisibility::Visible : EVisibility::Collapsed;
}


FText SDataflowSimulationViewportToolBar::LODButtonLabel() const
{
	if (const TSharedPtr<SDataflowSimulationViewport> Viewport = EditorViewport.Pin())
	{
		if (const TSharedPtr<FDataflowSimulationScene>& SimScene = Viewport->GetSimulationScene())
		{
			const int32 CurrentLOD = SimScene->GetPreviewLOD();
			if (CurrentLOD == INDEX_NONE)
			{
				return LOCTEXT("LODMenuButtonLabelAuto", "LOD Auto");
			}
			else
			{
				return FText::Format(LOCTEXT("LODMenuButtonLabel", "LOD {0}"), CurrentLOD);
			}
		}
	}

	return LOCTEXT("LODMenuButtonLabelNoLODs", "LOD");
}

int32 SDataflowSimulationViewportToolBar::NumLODs() const
{
	int32 MaxNumLODs = 0;
	if (const TSharedPtr<SDataflowSimulationViewport> Viewport = EditorViewport.Pin())
	{
		if (const TSharedPtr<const FDataflowSimulationScene>& SimScene = Viewport->GetSimulationScene())
		{
			if (const AActor* const PreviewActor = SimScene->GetPreviewActor())
			{
				PreviewActor->ForEachComponent<UActorComponent>(true, [&MaxNumLODs](UActorComponent* Component)
				{
					if (const ILODSyncInterface* const LODInterface = Cast<ILODSyncInterface>(Component))
					{
						MaxNumLODs = FMath::Max(MaxNumLODs, LODInterface->GetNumSyncLODs());
					}
				});
			}
		}
	}
	return MaxNumLODs;
}

TSharedRef<SWidget> SDataflowSimulationViewportToolBar::MakeLODMenu() const
{
	const auto SetCurrentLOD = [this](int32 NewLOD)
	{
		if (const TSharedPtr<SDataflowSimulationViewport> Viewport = EditorViewport.Pin())
		{
			if (const TSharedPtr<FDataflowSimulationScene>& SimScene = Viewport->GetSimulationScene())
			{
				SimScene->SetPreviewLOD(NewLOD);
			}
		}
	};

	const auto IsLODCurrent = [this](int32 QueryLOD) -> bool
	{
		if (const TSharedPtr<const SDataflowSimulationViewport> Viewport = EditorViewport.Pin())
		{
			if (const TSharedPtr<const FDataflowSimulationScene>& SimScene = Viewport->GetSimulationScene())
			{
				return (SimScene->GetPreviewLOD() == QueryLOD);
			}
		}
		return false;
	};

	constexpr bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);

	MenuBuilder.PushCommandList(CommandList.ToSharedRef());
	MenuBuilder.BeginSection("ClothAssetPreviewLODs", LOCTEXT("LODMenuSectionLabel", "LODs"));
	{
		const FUIAction LODAutoAction(
			FExecuteAction::CreateLambda(SetCurrentLOD, (int32)INDEX_NONE),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda(IsLODCurrent, (int32)INDEX_NONE));
		MenuBuilder.AddMenuEntry(LOCTEXT("LODMenuEntryLabelAuto", "LOD Auto"), FText::GetEmpty(), FSlateIcon(), LODAutoAction, NAME_None, EUserInterfaceActionType::RadioButton);

		for (int32 LODIndex = 0; LODIndex < NumLODs(); ++LODIndex)
		{
			const FUIAction Action(
				FExecuteAction::CreateLambda(SetCurrentLOD, LODIndex),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(IsLODCurrent, LODIndex));
			const FText MenuEntryLabel = FText::Format(LOCTEXT("LODMenuEntryLabel", "LOD {0}"), LODIndex);
			MenuBuilder.AddMenuEntry(MenuEntryLabel, FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::RadioButton);
		}
	}
	MenuBuilder.EndSection();
	MenuBuilder.PopCommandList();
	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
