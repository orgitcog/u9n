// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorModeToolkit.h"

#include "Algo/AnyOf.h"
#include "EditorModeManager.h"
#include "EdMode.h"
#include "IDetailsView.h"
#include "Framework/Application/SlateApplication.h"
#include "MetaHumanCharacterEditorCommands.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "MetaHumanCharacterEditorToolkitBuilder.h"
#include "MetaHumanCharacterEditorViewportClient.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "SPrimaryButton.h"
#include "Toolkits/AssetEditorModeUILayer.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "Tools/MetaHumanCharacterEditorBodyConformTool.h"
#include "Tools/MetaHumanCharacterEditorBodyEditingTools.h"
#include "Tools/MetaHumanCharacterEditorConformTool.h"
#include "Tools/MetaHumanCharacterEditorCostumeTools.h"
#include "Tools/MetaHumanCharacterEditorEyesTool.h"
#include "Tools/MetaHumanCharacterEditorFaceEditingTools.h"
#include "Tools/MetaHumanCharacterEditorHeadModelTool.h"
#include "Tools/MetaHumanCharacterEditorMakeupTool.h"
#include "Tools/MetaHumanCharacterEditorPresetsTool.h"
#include "Tools/MetaHumanCharacterEditorSkinTool.h"
#include "Tools/MetaHumanCharacterEditorSubTools.h"
#include "Tools/MetaHumanCharacterEditorPipelineTools.h"
#include "Tools/MetaHumanCharacterEditorWardrobeTools.h"
#include "UI/Views/SMetaHumanCharacterEditorBlendToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorBodyModelToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorConformToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorCostumeToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorEyesToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorFaceToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorHeadMaterialsToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorHeadModelToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorMakeupToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorPipelineToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorPresetsToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorSkinToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorToolView.h"
#include "UI/Views/SMetaHumanCharacterEditorWardrobeToolView.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "SWarningOrErrorBox.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

FMetaHumanCharacterEditorModeToolkit::FMetaHumanCharacterEditorModeToolkit()
{
	// Creates the widget to display warning messages for tools
	// This could potentially be inlined in GetInlineContent but since that function is const
	// create the widget here in constructor instead so its always ready to be used
	SAssignNew(ToolWarningArea, STextBlock)
		.AutoWrapText(true)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f)))
		.Text(FText::GetEmpty())
		.Visibility(EVisibility::Collapsed);

}

void FMetaHumanCharacterEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	bUsesToolkitBuilder = true;

	FModeToolkit::Init(InitToolkitHost, InOwningMode);
	if (HasToolkitBuilder())
	{
		ToolkitBuilder->VerticalToolbarElement->GenerateWidget();
	}

	RegisterPalettes();

	ClearNotification();
	ClearWarning();

	ToolkitSections->ToolWarningArea = ToolWarningArea;

	ToolkitBuilder->OnActivePaletteChanged.AddSP(this, &FMetaHumanCharacterEditorModeToolkit::OnActivePaletteChanged);

	// The ToolkitWidget is returned in GetInlineContent and represents the main
	// widget of the Mode Tools. Using FToolkitBuilder to offload the actual widget
	// creation and it already has all the basic interactions implemented
	SAssignNew(ToolkitWidget, SBorder)
	.HAlign(HAlign_Fill)
	.Padding(0)
	.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
	[
		ToolkitBuilder->GenerateWidget()->AsShared()
	];

	// Register callbacks to display tool messages in the status bar and warnings
	UEditorInteractiveToolsContext* ToolsContext = GetScriptableEditorMode()->GetInteractiveToolsContext();

	// Set the default tracking mode of tools. By default, activating a tool creates a transaction called "Activate Tool"
	// but we want more control over which transactions are created for the MH editing tools, so no transaction will
	// will be created by default when activating a tool
	ToolsContext->ToolManager->ConfigureChangeTrackingMode(EToolChangeTrackingMode::NoChangeTracking);

	ToolsContext->OnToolNotificationMessage.AddSP(this, &FMetaHumanCharacterEditorModeToolkit::PostNotification);
	ToolsContext->OnToolWarningMessage.AddSP(this, &FMetaHumanCharacterEditorModeToolkit::PostWarning);
}

FName FMetaHumanCharacterEditorModeToolkit::GetToolkitFName() const
{
	return TEXT("MetaHumanCharacterEditorMode");
}

FText FMetaHumanCharacterEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("TookitModeEditorName", "MetaHuman Character Editor Mode");
}

FText FMetaHumanCharacterEditorModeToolkit::GetActiveToolDisplayName() const
{
	return ActiveToolName;
}

TSharedPtr<SWidget> FMetaHumanCharacterEditorModeToolkit::GetInlineContent() const
{
	return ToolkitWidget.ToSharedRef();
}

void FMetaHumanCharacterEditorModeToolkit::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	FModeToolkit::OnToolStarted(Manager, Tool);

	ActiveToolName = Tool->GetToolInfo().ToolDisplayName;

	// Update last selected tool
	HandleLastToolActivation(Tool);

	// Builds the name of the active tool icon based on the active tool name.
	// Its important to have the tool identifiers used registering tools to match
	// the command names so we can build the correct icon name here
	FString ActiveToolIdentifier = GetScriptableEditorMode()->GetToolManager()->GetActiveToolName(EToolSide::Mouse);
	ActiveToolIdentifier.InsertAt(0, ".");
	FName ActiveToolIconName = ISlateStyle::Join(FMetaHumanCharacterEditorToolCommands::Get().GetContextName(), TCHAR_TO_ANSI(*ActiveToolIdentifier));
	ActiveToolIcon = FMetaHumanCharacterEditorStyle::Get().GetOptionalBrush(ActiveToolIconName);

	// make the standard tool warning area not visible (as we are using a custom warning area)
	ToolWarningArea->SetVisibility(EVisibility::Collapsed);

	// Sorting order matters. First we need to activate optional subtools, then to create the tool widget.
	UpdateSubToolsToolbar();
	UpdateActiveToolViewWidget();
}

void FMetaHumanCharacterEditorModeToolkit::OnToolEnded(UInteractiveToolManager* InManager, UInteractiveTool* InTool)
{
	FModeToolkit::OnToolEnded(InManager, InTool);

	ActiveToolName = FText::GetEmpty();

	UpdateSubToolsToolbar();
	UpdateActiveToolViewWidget();

	InTool->OnPropertySetsModified.RemoveAll(this);
}

void FMetaHumanCharacterEditorModeToolkit::PostNotification(const FText& InMessage)
{
	ClearNotification();

	if (TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin())
	{
		const FName StatusBarName = ModeUILayerPtr->GetStatusBarName();
		UStatusBarSubsystem* StatusBarSubsystem = GEditor->GetEditorSubsystem<UStatusBarSubsystem>();
		ActiveToolMessageHandle = StatusBarSubsystem->PushStatusBarMessage(StatusBarName, InMessage);
	}
}

void FMetaHumanCharacterEditorModeToolkit::ClearNotification()
{
	if (TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin())
	{
		const FName StatusBarName = ModeUILayerPtr->GetStatusBarName();
		UStatusBarSubsystem* StatusBarSubsystem = GEditor->GetEditorSubsystem<UStatusBarSubsystem>();
		StatusBarSubsystem->PopStatusBarMessage(StatusBarName, ActiveToolMessageHandle);
	}

	ActiveToolMessageHandle.Reset();
}

void FMetaHumanCharacterEditorModeToolkit::PostWarning(const FText& InMessage)
{
	if (InMessage.IsEmpty())
	{
		ClearWarning();
	}
	else
	{
		CustomWarning = InMessage;
	}
}

void FMetaHumanCharacterEditorModeToolkit::ClearWarning()
{
	CustomWarning = FText();
}

EVisibility FMetaHumanCharacterEditorModeToolkit::GetCustomWarningVisibility() const
{
	if (CustomWarning.IsEmpty())
	{
		return EVisibility::Collapsed;
	}
	else
	{
		return EVisibility::Visible;
	}
}

FText FMetaHumanCharacterEditorModeToolkit::GetCustomWarning() const
{
	return CustomWarning;
}

TSharedRef<SWidget> FMetaHumanCharacterEditorModeToolkit::CreateSubToolsToolbar(TNotNull<UInteractiveTool*> Tool) const
{
	const UMetaHumanCharacterEditorToolWithSubTools* ToolWithSubTools = Cast<UMetaHumanCharacterEditorToolWithSubTools>(Tool);
	const UMetaHumanCharacterEditorSubToolsProperties* SubTools = IsValid(ToolWithSubTools) ? ToolWithSubTools->GetSubTools() : nullptr;
	if (!SubTools)
	{
		return SNullWidget::NullWidget;
	}

	const TSharedRef<FSlimHorizontalUniformToolBarBuilder> ToolbarBuilder = MakeShared<FSlimHorizontalUniformToolBarBuilder>(SubTools->GetCommandList(), FMultiBoxCustomization::None);
	ToolbarBuilder->SetStyle(&FAppStyle::Get(), TEXT("SlimPaletteToolBar"));

	const TSharedPtr<FUICommandList> CommandList = SubTools->GetCommandList();
	const TArray<TSharedPtr<FUICommandInfo>> SubToolCommands = SubTools->GetSubToolCommands();
	for (TSharedPtr<FUICommandInfo> SubToolCommand : SubToolCommands)
	{
		FButtonArgs Args;
		Args.Command = SubToolCommand;
		Args.CommandList = CommandList;
		Args.UserInterfaceActionType = SubToolCommand->GetUserInterfaceType();
		ToolbarBuilder->AddToolBarButton(Args);
	}

	// Automatically trigger the default subtool or the last active subtool action
	FString ActiveToolIdentifier = GetScriptableEditorMode()->GetToolManager()->GetActiveToolName(EToolSide::Mouse);
	if (SubToolCommands.Num() > 0)
	{
		TSharedPtr<FUICommandInfo> Command = SubTools->GetDefaultCommand();
		if (ToolNameToLastActiveSubToolNameMap.Contains(*ActiveToolIdentifier))
		{
			const FName& LastActiveSubToolName = ToolNameToLastActiveSubToolNameMap.FindChecked(*ActiveToolIdentifier);
			const int32 Index = SubToolCommands.IndexOfByPredicate(
				[LastActiveSubToolName](const TSharedPtr<FUICommandInfo>& Command)
				{
					return Command->GetCommandName() == LastActiveSubToolName;
				});

			Command = SubToolCommands.IsValidIndex(Index) ? SubToolCommands[Index] : nullptr;
		}
		else if (!Command)
		{
			Command = SubToolCommands[0];
		}
			
		if (Command.IsValid() && CommandList.IsValid())
		{
			CommandList->TryExecuteAction(Command.ToSharedRef());
		}
	}

	const TSharedRef<SWidget> SubToolsToolbar =
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(16.f, 4.f)
			[
				SNew(STextBlock)
				.Text(ActiveToolName)
				.Font(FCoreStyle::Get().GetFontStyle("BoldFont"))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ToolbarBuilder->MakeWidget()
		];

	return SubToolsToolbar;
}

TSharedRef<SWidget> FMetaHumanCharacterEditorModeToolkit::CreateToolView(UInteractiveTool* Tool) const
{
	if (UMetaHumanCharacterEditorPresetsTool* PresetsTool = Cast<UMetaHumanCharacterEditorPresetsTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorPresetsToolView, PresetsTool);
	}
	else if (UMetaHumanCharacterEditorFaceBlendTool* FaceBlendTool = Cast<UMetaHumanCharacterEditorFaceBlendTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorHeadBlendToolView, FaceBlendTool);
	}
	else if (UMetaHumanCharacterEditorFaceSculptTool* FaceSculptTool = Cast<UMetaHumanCharacterEditorFaceSculptTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorFaceSculptToolView, FaceSculptTool);
	}
	else if (UMetaHumanCharacterEditorFaceMoveTool* FaceMoveTool = Cast<UMetaHumanCharacterEditorFaceMoveTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorFaceMoveToolView, FaceMoveTool);
	}
	else if (UMetaHumanCharacterEditorBodyBlendTool* BodyBlendTool = Cast<UMetaHumanCharacterEditorBodyBlendTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorBodyBlendToolView, BodyBlendTool);
	}
	else if (UMetaHumanCharacterEditorConformTool* ConformTool = Cast<UMetaHumanCharacterEditorConformTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorConformToolView, ConformTool);
	}
	else if (UMetaHumanCharacterEditorBodyConformTool* BodyConformTool = Cast<UMetaHumanCharacterEditorBodyConformTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorConformToolView, BodyConformTool);
	}
	else if (UMetaHumanCharacterEditorBodyModelTool* BodyModelTool = Cast<UMetaHumanCharacterEditorBodyModelTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorBodyModelToolView, BodyModelTool);
	}
	else if (UMetaHumanCharacterEditorEyesTool* EyesTool = Cast<UMetaHumanCharacterEditorEyesTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorEyesToolView, EyesTool);
	}
	else if (UMetaHumanCharacterEditorHeadMaterialsTool* HeadMaterialsTool = Cast<UMetaHumanCharacterEditorHeadMaterialsTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorHeadMaterialsToolView, HeadMaterialsTool);
	}
	else if (UMetaHumanCharacterEditorHeadModelTool* HeadModelTool = Cast<UMetaHumanCharacterEditorHeadModelTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorHeadModelToolView, HeadModelTool);
	}
	else if (UMetaHumanCharacterEditorMakeupTool* MakeupTool = Cast<UMetaHumanCharacterEditorMakeupTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorMakeupToolView, MakeupTool);
	}
	else if (UMetaHumanCharacterEditorSkinTool* SkinTool = Cast<UMetaHumanCharacterEditorSkinTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorSkinToolView, SkinTool);
	}
	else if (UMetaHumanCharacterEditorWardrobeTool* WardrobeTool = Cast<UMetaHumanCharacterEditorWardrobeTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorWardrobeToolView, WardrobeTool);
	}
	else if (UMetaHumanCharacterEditorCostumeTool* CostumeTool = Cast<UMetaHumanCharacterEditorCostumeTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorCostumeToolView, CostumeTool);
	}
	else if (UMetaHumanCharacterEditorPipelineTool* PipelineTool = Cast<UMetaHumanCharacterEditorPipelineTool>(Tool))
	{
		return SNew(SMetaHumanCharacterEditorPipelineToolView, PipelineTool);
	}
	else
	{
		return CreateToolDetailsView(Tool);
	}
}

TSharedRef<IDetailsView> FMetaHumanCharacterEditorModeToolkit::CreateToolDetailsView(UInteractiveTool* Tool) const
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	const TSharedRef<IDetailsView> ToolDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	if (IsValid(Tool))
	{
		ToolDetailsView->SetObjects(Tool->GetToolProperties());
	}

	return ToolDetailsView;
}

TSharedRef<SWidget> FMetaHumanCharacterEditorModeToolkit::MakeCustomWarningsWidget()
{
	return
		SNew(SBox)
			.Padding(4.f)
			[
				SNew(SWarningOrErrorBox)
				.AutoWrapText(true)
				.MessageStyle(EMessageStyle::Warning)
				.Visibility(this, &FMetaHumanCharacterEditorModeToolkit::GetCustomWarningVisibility)
				.Message_Lambda([this] { return GetCustomWarning(); })
			];
}


TSharedRef<SWidget> FMetaHumanCharacterEditorModeToolkit::MakeActiveToolViewWidget()
{
	return
		SNew(SBorder)
		.BorderImage(FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.MainToolbar"))
		[
			SNew(SVerticalBox)

			// Subtools Toolbar section
			+ SVerticalBox::Slot()
			.Padding(-2.f, -2.f, -2.f, 0.f)
			.AutoHeight()
			[
				SAssignNew(SubToolsToolbarWidget, SVerticalBox)
			]
			// Tool View section
			+ SVerticalBox::Slot()
			[
				SAssignNew(ActiveToolViewWidget, SVerticalBox)
			]
		];
}

void FMetaHumanCharacterEditorModeToolkit::RegisterPalettes()
{
	const FMetaHumanCharacterEditorToolCommands& Commands = FMetaHumanCharacterEditorToolCommands::Get();

	const TSharedRef<FMetaHumanCharacterEditorToolkitSections> Sections = MakeShared<FMetaHumanCharacterEditorToolkitSections>();
	Sections->ToolViewArea = MakeActiveToolViewWidget();
	Sections->ToolCustomWarningsArea = MakeCustomWarningsWidget();
	ToolkitSections = Sections;

	FToolkitBuilderArgs ToolkitBuilderArgs{ GetScriptableEditorMode()->GetModeInfo().ToolbarCustomizationName };
	ToolkitBuilderArgs.ToolkitCommandList = GetToolkitCommands();
	ToolkitBuilderArgs.ToolkitSections = ToolkitSections;
	ToolkitBuilderArgs.SelectedCategoryTitleVisibility = EVisibility::Visible;

	ToolkitBuilder = MakeShared<FMetaHumanCharacterEditorToolkitBuilder>(ToolkitBuilderArgs);

	const TArray<TSharedPtr<FUICommandInfo>> PresetsCommands =
	{
		Commands.BeginPresetsTool
	};
	ToolkitBuilder->AddPalette(MakeShared<FToolPalette>(Commands.LoadPresetsTools, PresetsCommands));

	const TArray<TSharedPtr<FUICommandInfo>> BodyCommands =
	{
		Commands.BeginBodyBlendTool,
		Commands.BeginBodyConformTools,
		Commands.BeginBodyModelTool
	};
	ToolkitBuilder->AddPalette(MakeShared<FToolPalette>(Commands.LoadBodyTools, BodyCommands));

	const TArray<TSharedPtr<FUICommandInfo>> HeadToolsCommand =
	{
		Commands.BeginFaceBlendTool,
		Commands.BeginConformTools,
		Commands.BeginFaceMoveTool,
		Commands.BeginFaceSculptTool,
		Commands.BeginHeadModelTools
	};
	ToolkitBuilder->AddPalette(MakeShared<FToolPalette>(Commands.LoadHeadTools, HeadToolsCommand));

	const TArray<TSharedPtr<FUICommandInfo>> MaterialsCommands =
	{
		Commands.BeginSkinTool,
		Commands.BeginEyesTool,
		Commands.BeginMakeupTool,
		Commands.BeginHeadMaterialsTools,
	};
	ToolkitBuilder->AddPalette(MakeShared<FToolPalette>(Commands.LoadMaterialsTools, MaterialsCommands));

	const TArray<TSharedPtr<FUICommandInfo>> HairAndClothingCommands =
	{
		Commands.BeginWardrobeSelectionTool,
		Commands.BeginCostumeDetailsTool,
	};
	ToolkitBuilder->AddPalette(MakeShared<FToolPalette>(Commands.LoadHairAndClothingTools, HairAndClothingCommands));

	const TArray<TSharedPtr<FUICommandInfo>> PipelineCommands =
	{
		Commands.BeginPipelineTool
	};
	ToolkitBuilder->AddPalette(MakeShared<FToolPalette>(Commands.LoadPipelineTools, PipelineCommands));


	ToolkitBuilder->SetActivePaletteOnLoad(Commands.LoadHeadTools.Get());
	ToolkitBuilder->UpdateWidget();
}

void FMetaHumanCharacterEditorModeToolkit::UpdateActiveToolViewWidget()
{
	TSharedPtr<SVerticalBox> ToolViewVerticalBox = StaticCastSharedPtr<SVerticalBox>(ActiveToolViewWidget);
	if (!ToolViewVerticalBox)
	{
		return;
	}

	ToolViewVerticalBox->ClearChildren();

	UInteractiveToolManager* ToolManager = GetScriptableEditorMode().IsValid() ? GetScriptableEditorMode()->GetToolManager() : nullptr;
	TSharedPtr<FToolPalette> ActivePalette = ToolkitBuilder.IsValid() ? ToolkitBuilder->ActivePalette : nullptr;
	UInteractiveTool* ActiveTool = IsValid(ToolManager) ? ToolManager->GetActiveTool(EToolSide::Mouse) : nullptr;
	if (!ActivePalette || !ActiveTool)
	{
		return;
	}

	const FString ActiveToolIdentifier = ToolManager->GetActiveToolName(EToolSide::Mouse);
	bool bIsToolInActivePalette = Algo::AnyOf(ActivePalette->PaletteActions,
		[this, ActiveToolIdentifier](const TSharedRef<FButtonArgs>& Args)
		{
			return Args->Command.IsValid() && Args->Command->GetCommandName().ToString() == ActiveToolIdentifier;
		});

	if (bIsToolInActivePalette)
	{
		const TSharedRef<SWidget> ActiveToolView = CreateToolView(ActiveTool);
		ToolViewVerticalBox->AddSlot()
			[
				ActiveToolView
			];
	}
}

void FMetaHumanCharacterEditorModeToolkit::UpdateSubToolsToolbar()
{
	TSharedPtr<SVerticalBox> SubToolsToolbarBox = StaticCastSharedPtr<SVerticalBox>(SubToolsToolbarWidget);
	if (!SubToolsToolbarBox.IsValid())
	{
		return;
	}

	SubToolsToolbarBox->ClearChildren();

	TSharedPtr<FToolPalette> ActivePalette = ToolkitBuilder.IsValid() ? ToolkitBuilder->ActivePalette : nullptr;
	UInteractiveToolManager* ToolManager = GetScriptableEditorMode().IsValid() ? GetScriptableEditorMode()->GetToolManager() : nullptr;
	if (!ActivePalette.IsValid() || !ToolManager)
	{
		return;
	}

	const FString ActiveToolIdentifier = ToolManager->GetActiveToolName(EToolSide::Mouse);
	bool bIsToolInActivePalette = Algo::AnyOf(ActivePalette->PaletteActions,
		[this, ActiveToolIdentifier](const TSharedRef<FButtonArgs>& Args)
		{
			return Args->Command.IsValid() && Args->Command->GetCommandName().ToString() == ActiveToolIdentifier;
		});

	if (bIsToolInActivePalette)
	{
		UInteractiveTool* ActiveTool = ToolManager->GetActiveTool(EToolSide::Mouse);
		const TSharedRef<SWidget> SubToolsToolbar = CreateSubToolsToolbar(ActiveTool);
		SubToolsToolbarBox->AddSlot()
			.AutoHeight()
			[
				SubToolsToolbar
			];
	}
}

void FMetaHumanCharacterEditorModeToolkit::HandleAutoActivatingTools()
{
	if (!ToolkitBuilder.IsValid() || !ToolkitBuilder->ActivePalette.IsValid())
	{
		return;
	}

	const FName PaletteName = ToolkitBuilder->GetActivePaletteName();
	const TArray<TSharedRef<FButtonArgs>> PaletteActions = ToolkitBuilder->ActivePalette->PaletteActions;
	if (PaletteActions.IsEmpty())
	{
		return;
	}

	TSharedPtr<const FUICommandList> CommandList = PaletteActions[0]->CommandList;
	TSharedPtr<const FUICommandInfo> Command = PaletteActions[0]->Command;
	if (ModeNameToLastActiveToolNameMap.Contains(PaletteName))
	{
		const FName& LastActiveToolName = ModeNameToLastActiveToolNameMap.FindChecked(PaletteName);
		const int32 Index = PaletteActions.IndexOfByPredicate(
			[LastActiveToolName](const TSharedRef<FButtonArgs>& PaletteAction)
			{
				return PaletteAction->Command->GetCommandName() == LastActiveToolName;
			});

		CommandList = PaletteActions.IsValidIndex(Index) ? PaletteActions[Index]->CommandList : nullptr;
		Command = PaletteActions.IsValidIndex(Index) ? PaletteActions[Index]->Command : nullptr;
	}
	else
	{
		CommandList = PaletteActions[0]->CommandList;
	    Command = PaletteActions[0]->Command;
	}

	if (!CommandList.IsValid() || !Command.IsValid())
	{
		return;
	}

	CommandList->ExecuteAction(Command.ToSharedRef());
	if (PaletteActions.Num() == 1)
	{
		ToolkitBuilder->SetActivePaletteCommandsVisibility(EVisibility::Collapsed);
	}
}

void FMetaHumanCharacterEditorModeToolkit::HandleLastToolActivation(UInteractiveTool* Tool)
{
	const FName ToolName = *GetScriptableEditorMode()->GetToolManager()->GetActiveToolName(EToolSide::Mouse);
	const FName PaletteName = ToolkitBuilder.IsValid() ? ToolkitBuilder->GetActivePaletteName() : NAME_None;
	if (!PaletteName.IsNone())
	{
		ModeNameToLastActiveToolNameMap.Add(PaletteName, ToolName);
	}

	UMetaHumanCharacterEditorToolWithSubTools* ToolWithSubTools = Cast<UMetaHumanCharacterEditorToolWithSubTools>(Tool);
	if (ToolWithSubTools && !ToolWithSubTools->OnPropertySetsModified.IsBoundToObject(this))
	{
		ToolWithSubTools->OnPropertySetsModified.AddSP(this, &FMetaHumanCharacterEditorModeToolkit::OnSubToolPropertySetsModified, Tool, ToolName);
	}
}

void FMetaHumanCharacterEditorModeToolkit::OnActivePaletteChanged()
{
	UpdateSubToolsToolbar();
	UpdateActiveToolViewWidget();

	GetScriptableEditorMode()->GetInteractiveToolsContext()->EndTool(EToolShutdownType::Completed);
	
	if (ToolkitBuilder.IsValid())
	{
		ToolkitBuilder->SetActivePaletteCommandsVisibility(EVisibility::Visible);
	}

	HandleAutoActivatingTools();

	UEditorInteractiveToolsContext* ToolsContext = GetScriptableEditorMode()->GetInteractiveToolsContext();
	FViewport* Viewport = ToolsContext->ToolManager->GetContextQueriesAPI()->GetFocusedViewport();
	FMetaHumanCharacterViewportClient* MHCViewportClient = static_cast<FMetaHumanCharacterViewportClient*>(Viewport->GetClient());
	const FName PaletteName = ToolkitBuilder->GetActivePaletteName();
	if (PaletteName == FMetaHumanCharacterEditorToolCommands::Get().LoadHeadTools->GetCommandName())
	{
		MHCViewportClient->SetAutoFocusToSelectedFrame(EMetaHumanCharacterCameraFrame::Face, /*bInRotate*/false);
	}
	else if (PaletteName == FMetaHumanCharacterEditorToolCommands::Get().LoadBodyTools->GetCommandName())
	{
		MHCViewportClient->SetAutoFocusToSelectedFrame(EMetaHumanCharacterCameraFrame::Body, /*bInRotate*/false);
	}
}

void FMetaHumanCharacterEditorModeToolkit::OnSubToolPropertySetsModified(UInteractiveTool* Tool, const FName ToolName)
{
	const UMetaHumanCharacterEditorToolWithSubTools* ToolWithSubTools = Cast<UMetaHumanCharacterEditorToolWithSubTools>(Tool);
	const UMetaHumanCharacterEditorSubToolsProperties* SubTools = IsValid(ToolWithSubTools) ? ToolWithSubTools->GetSubTools() : nullptr;
	if (SubTools)
	{
		ToolNameToLastActiveSubToolNameMap.Add(ToolName, SubTools->GetActiveSubToolName());
	}
}

#undef LOCTEXT_NAMESPACE
