// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureManagerWidget.h"
#include "CaptureSourcesWidget.h"
#include "CaptureManagerCommands.h"
#include "CaptureManagerLog.h"
#include "FootageIngestWidget.h"
#include "MetaHumanCaptureSource.h"
#include "Framework/Commands/GenericCommands.h"
#include "Editor.h"
#include "EditorViewportCommands.h"
#include "Interfaces/IMainFrameModule.h"
#include "MetaHumanFootageRetrievalWindowStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "ToolMenus.h"
#include "ToolMenuContext.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h" //to be able to use FPathPickerConfig
#include "IContentBrowserDataModule.h"

#include "Commands/LiveLinkFaceConnectionCommands.h"

#include "LiveLinkHubLauncherUtils.h"
#include "SWarningOrErrorBox.h"
#include "Features/IModularFeatures.h"
#include "MetaHumanFaceTrackerInterface.h"


namespace
{
	TAutoConsoleVariable<bool> CVarShowCMToolbar{
		TEXT("mh.CaptureManager.ShowCMToolbar"),
		true,
		TEXT("Shows Capture Manager toolbar"),
		ECVF_Default
	};
}

static const FName QuickSettingsMenuName("CaptureManager.QuickSettings");
static const FName MainMenuName("CaptureManager.MainMenu");

const FName SCaptureManagerWidget::ToolMenuToolbarName("CaptureManager.Toolbar");
const FName SCaptureManagerWidget::DefaultToolbarName("CaptureManager.Toolbar");

const FName SCaptureManagerWidget::CaptureSourcesTabName("Capture Sources");
const FName SCaptureManagerWidget::FootageIngestTabName("Footage Ingest");

#ifdef SHOW_MONITORING_TABS
const FName SCaptureManagerWidget::Monitor1TabName("Device 1 Monitor");
const FName SCaptureManagerWidget::Monitor2TabName("Device 2 Monitor");
#endif //SHOW_MONITORING_TABS

#ifdef SHOW_VIEWPORT_TABS
const FName SCaptureManagerWidget::Viewport1TabName("Device 1 Viewport");
const FName SCaptureManagerWidget::Viewport2TabName("Device 2 Viewport");
#endif //SHOW_VIEWPORT_TABS

#define LOCTEXT_NAMESPACE "CaptureManagerWidget"

static const FText OtherDevelopersFilterTooltipHidingText = LOCTEXT("ShowOtherDevelopersTooltipText.Hiding", "Hiding Other Developers Assets");
static const FText OtherDevelopersFilterTooltipShowingText = LOCTEXT("ShowOtherDevelopersTooltipText.Showing", "Showing Other Developers Assets");
static const FText ShowDevelopersContentText = LOCTEXT("ShowDevelopersContent", "Show Developers Content");
static const FText ShowDevelopersContentTooltipText = LOCTEXT("ShowDevelopersContentTooltip", "Show developers content in the view?");

static const FName DynamicToolbarSectionName = TEXT("DynamicToolbarSection");

static FString SourceCreationPath;

static void UnregisterToolMenus()
{
	UToolMenus::Get()->RemoveMenu(QuickSettingsMenuName);
	UToolMenus::Get()->RemoveMenu(MainMenuName);
}

SCaptureManagerWidget::SCaptureManagerWidget()
	: ToolkitUICommandList(new FUICommandList())
{
	UpdateDefaultAssetCreationLocation();
}

SCaptureManagerWidget::~SCaptureManagerWidget()
{
	RemoveDynamicToolbarSection();
	UnregisterToolMenus();
}

void SCaptureManagerWidget::RegisterCommands()
{
	StartCapture = MakeUnique<FAutoConsoleCommand>(TEXT("CaptureManager.StartCapture"),
		TEXT("Start the capture on the currently selected source if it is supported.\n")
		TEXT("Usage: CaptureManager.StartCapture SlateName TakeNumber [Actor] [Scenario]\n")
		TEXT("Arguments:\n")
		TEXT(" * SlateName (String)\n")
		TEXT(" * TakeNumber (Number)\n")
		TEXT(" * Actor (String)\n")
		TEXT(" * Scenario (String)"),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &SCaptureManagerWidget::StartCaptureConsoleHandler));

	StopCapture = MakeUnique<FAutoConsoleCommand>(TEXT("CaptureManager.StopCapture"),
		TEXT("Stop the capture on the currently selected source if it is supported.\n")
		TEXT("Usage: CaptureManager.StopCapture"),
		FConsoleCommandDelegate::CreateRaw(this, &SCaptureManagerWidget::StopCaptureConsoleHandler));
}

void SCaptureManagerWidget::Initialize()
{
	if (bIsInitialized)
	{
		return;
	}

	TArray<TSharedPtr<FFootageCaptureSource>> CaptureSources = CaptureSourcesWidget->GetCaptureSources();

	FootageIngestWidget->OnCaptureSourcesChanged(CaptureSources);
	FootageIngestWidget->SetAutosaveAfterImport(bAutosaveAfterImport);

	CaptureSourcesWidget->StartCaptureSources();

	bIsInitialized = true;
}

void SCaptureManagerWidget::UpdateDefaultAssetCreationLocation()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	// Default asset creation path is usually the root project folder
	DefaultAssetCreationPath = ContentBrowserModule.Get().GetInitialPathToSaveAsset(FContentBrowserItemPath(SourceCreationPath, EContentBrowserPathType::Internal)).GetInternalPathString();
	SourceCreationPath = DefaultAssetCreationPath;

	if (FootageIngestWidget)
	{
		FootageIngestWidget->SetDefaultAssetCreationPath(DefaultAssetCreationPath);
	}
}

void SCaptureManagerWidget::ShowCMToolbar(IConsoleVariable* InVar)
{
	Toolbar->GetParentWidget()->Invalidate(EInvalidateWidgetReason::Visibility);
}

void SCaptureManagerWidget::Construct(const FArguments& InArgs, const TSharedPtr<SWindow>& OwningWindow, const TSharedRef<SDockTab>& OwningTab)
{

	Commands = InArgs._CaptureManagerCommands;

	CVarShowCMToolbar.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateSP(this, &SCaptureManagerWidget::ShowCMToolbar));

	TabManager = FGlobalTabmanager::Get()->NewTabManager(OwningTab);

	const auto& PersistLayout = [](const TSharedRef<FTabManager::FLayout>& LayoutToSave)
		{
			FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, LayoutToSave);
		};
	TabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateLambda(PersistLayout));

	FTabSpawnerEntry& CaptureSourcesTabSpawnerEntry = RegisterCaptureSourcesTabSpawner();
	FTabSpawnerEntry& FootageIngestTabSpawnerEntry = RegisterFootageIngestTabSpawner();

	const TSharedRef<FWorkspaceItem> TargetSetsWorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("CaptureManagerWorkspaceMenuCategory", "Capture Manager"));
	TargetSetsWorkspaceMenuCategory->AddItem(CaptureSourcesTabSpawnerEntry.AsShared());
	TargetSetsWorkspaceMenuCategory->AddItem(FootageIngestTabSpawnerEntry.AsShared());

	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("CaptureManager_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.2f) //horizontal splitter position for the Capture Sources tab
				->SetHideTabWell(false)
				->AddTab(CaptureSourcesTabName, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.8f) //horizontal splitter position for the right side (Footage Ingest tab)
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
#ifdef SHOW_MONITORING_TABS
					->SetSizeCoefficient(0.7f)
#else
					->SetSizeCoefficient(1.0f)
#endif //SHOW_MONITORING_TABS
					->SetHideTabWell(false)
					->AddTab(FootageIngestTabName, ETabState::OpenedTab)
#ifdef SHOW_VIEWPORT_TABS
					//the additional tabs are for future reference
					//we put viewport tabs in the main area as they need a lot of space
					//the user can undock them
					->AddTab(Viewport1TabName, ETabState::OpenedTab)
					->AddTab(Viewport2TabName, ETabState::OpenedTab)
#endif //SHOW_VIEWPORT_TABS

				)
#ifdef SHOW_MONITORING_TABS
				->Split
				(
					//the following tabs are for future reference
					//we group monitoring tabs under the main area as the user
					//will want to monitor disk space during the ingest too

					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.3f)
					->SetOrientation(Orient_Horizontal)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f) //horizontal splitter position
						->SetHideTabWell(false)
						->AddTab(Monitor1TabName, ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f) //horizontal splitter position
						->SetHideTabWell(false)
						->AddTab(Monitor2TabName, ETabState::OpenedTab)
					)
				)
#endif //SHOW_MONITORING_TABS
			)
		);

	Layout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, Layout);

	FToolMenuContext ToolMenuContext;
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");

	TabManager->SetAllowWindowMenuBar(true);

	GenerateToolbar();

	GenerateMessageWidget();

	FText CaptureManagerWarningText = LOCTEXT("CaptureManagerMoved", "Capture Manager has moved to Live Link Hub and will be removed from Unreal Editor in 5.9");

	ChildSlot
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f)
				[
					Toolbar.ToSharedRef()
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					TabManager->RestoreFrom(Layout, OwningWindow).ToSharedRef()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 8.0f, 0.0f, 4.0f))
				[
					MessageWidget.ToSharedRef()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 8.0f, 0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
						.MessageStyle(EMessageStyle::Warning)
						.Message(CaptureManagerWarningText)
						[
							SNew(SButton)
								.OnClicked(this, &SCaptureManagerWidget::OpenLiveLinkHub)
								.TextStyle(FAppStyle::Get(), "DialogButtonText")
								.Text(LOCTEXT("GoToLiveLinkHubButton", "Go To Live Link Hub"))
								.ToolTipText(LOCTEXT("GoToLiveLinkHub_Tooltip", "Open Live Link Hub or go to download page"))
						]
				]
		];

#if !defined(HIDE_MAIN_MENU)
	UToolMenus::Get()->RegisterMenu(MainMenuName, "MainFrame.NomadMainMenu");

	MainFrameModule.MakeMainMenu(TabManager, MainMenuName, ToolMenuContext);
#endif

	RegisterCommands();
	BindCommands();
}

TSharedRef<SWidget> SCaptureManagerWidget::GenerateTakeSlateWidget()
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), "AssetEditorToolbar");

	//the widget we are putting this in is a horizontal box, so we need a vertical one
	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	HorizontalBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(10.0f, 5.0f, 0.0f, 5.0f)
		[
			SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FMetaHumanFootageRetrievalWindowStyle::Get().GetBrush("CaptureManager.Toolbar.CaptureSlate"))
		];
	HorizontalBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(10.0f, 5.0f, 0.0f, 5.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("CaptureTakeTitleLabel", "Slate"))
		];
	HorizontalBox->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(5.0f, 5.0f, 0.0f, 5.0f)
		[
			SNew(SBox)
				.WidthOverride(160.0f)
				[
					SAssignNew(SlateNameTextBox, SEditableTextBox)
						.OnVerifyTextChanged(this, &SCaptureManagerWidget::VerifySlateName)
						.OnTextCommitted(this, &SCaptureManagerWidget::HandleSlateNameTextCommited)
						.IsEnabled(this, &SCaptureManagerWidget::IsCaptureSourceSelected)
						.Text(this, &SCaptureManagerWidget::GetSlateName)
						.ToolTipText(LOCTEXT("CaptureSlateNameTextBoxTooltip", "Enter Slate Name here"))
				]
		];
	HorizontalBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(10.0f, 5.0f, 0.0f, 5.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("CaptureTakeNumberLabel", "Take No."))
		];
	HorizontalBox->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(5.0f, 5.0f, 0.0f, 5.0f)
		[
			SNew(SBox)
				.WidthOverride(50.0f)
				[
					SAssignNew(TakeNumberTextBox, SEditableTextBox)
						.Font(FAppStyle::Get().GetFontStyle(TEXT("MenuItem.Font")))
						.OnVerifyTextChanged(this, &SCaptureManagerWidget::VerifyTakeNumber)
						.OnTextCommitted(this, &SCaptureManagerWidget::HandleTakeNumberCommited)
						.IsEnabled(this, &SCaptureManagerWidget::IsCaptureSourceSelected)
						.Text(this, &SCaptureManagerWidget::GetTakeNumber)
						.ToolTipText(LOCTEXT("CaptureManagerTakeNumber", "Enter Take Number here\nAutomatically increased on each Stop Capture,\nand reset to 1 on entering a new Slate Name"))
				]
		];
	//add an empty box with FillWidth at the end so the toolbar doesn't end abruptly before the edge
	HorizontalBox->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.FillWidth(1.0f)
		.Padding(5.0f, 5.0f, 0.0f, 5.0f)
		[
			SNew(SBox)
				.HAlign(EHorizontalAlignment::HAlign_Fill)
		];

#ifdef SHOW_NOTE
	HorizontalBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(10.0f, 5.0f, 0.0f, 5.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("CaptureTakeNoteLabel", "Note"))
		];
	HorizontalBox->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.FillWidth(10.0f)
		.Padding(5.0f, 5.0f, 10.0f, 5.0f)
		[
			SNew(SEditableTextBox)
				.Text(TAttribute<FText>())
		];
#endif //SHOW_NOTE

	VerticalBox->AddSlot()
		.VAlign(VAlign_Center)
		[
			HorizontalBox
		];

	return HorizontalBox;
}

void SCaptureManagerWidget::BindCommands()
{
#ifdef SHOW_SAVE_BUTTON
	ToolkitUICommandList->MapAction(Commands->Save,
		FExecuteAction::CreateSP(this, &SCaptureManagerWidget::HandleSave),
		FCanExecuteAction::CreateSP(this, &SCaptureManagerWidget::CanSave));
#endif

	ToolkitUICommandList->MapAction(Commands->SaveAll,
		FExecuteAction::CreateSP(this, &SCaptureManagerWidget::HandleSaveAll),
		FCanExecuteAction::CreateSP(this, &SCaptureManagerWidget::CanSaveAll));

	ToolkitUICommandList->MapAction(Commands->Refresh,
		FExecuteAction::CreateSP(this, &SCaptureManagerWidget::HandleRefresh),
		FCanExecuteAction::CreateSP(this, &SCaptureManagerWidget::CanRefresh));

	ToolkitUICommandList->MapAction(Commands->StartStopCapture,
		FExecuteAction::CreateSP(this, &SCaptureManagerWidget::HandleStartStopCaptureToggle),
		FCanExecuteAction::CreateSP(this, &SCaptureManagerWidget::CanStartStopCapture));
}

TSharedRef<SWidget> SCaptureManagerWidget::PopulateAddCaptureSourceComboBox()
{
	FName ToolBarName = "CaptureManager.CommonActions";
	UToolMenu* Menu = UToolMenus::Get()->FindMenu(ToolBarName);

	UCaptureManagerEditorContext* Context = Menu->FindContext<UCaptureManagerEditorContext>();

	TSharedPtr<SCaptureManagerWidget> Widget = Context->CaptureManagerWidget.Pin();
	UEnum* EnumPtr = FindFirstObjectSafe<UEnum>(TEXT("EMetaHumanCaptureSourceType"));

	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection(FName(TEXT("Capture Sources")), FText::FromString(TEXT("Available Types")));

	for (int32 Option = 0; Option < EnumPtr->NumEnums() - 1; ++Option) // -1 is for omitting the last entry (end entry)
	{
		int64 Type = EnumPtr->GetValueByIndex(Option);
		if (static_cast<EMetaHumanCaptureSourceType>(Type) != EMetaHumanCaptureSourceType::Undefined)
		{
			FText CurString = EnumPtr->GetDisplayNameTextByIndex(Option);

			FUIAction ItemAction(FExecuteAction::CreateSP(Widget.ToSharedRef(), &SCaptureManagerWidget::CreateSelectedCaptureSourceType, Type));

			TAttribute<FText> Tooltip = TAttribute<FText>::CreateSP(Widget.Get(), &SCaptureManagerWidget::GetCreationTooltip, CurString);
			MenuBuilder.AddMenuEntry(CurString, MoveTemp(Tooltip), FSlateIcon(), MoveTemp(ItemAction));
		}
	}

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SCaptureManagerWidget::CreateSelectedCaptureSourceType(int64 InType)
{
	EMetaHumanCaptureSourceType Type = static_cast<EMetaHumanCaptureSourceType>(InType);

	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	UClass* Class = UMetaHumanCaptureSource::StaticClass();

	TArray<FAssetData> AssetsData;
	AssetRegistry.GetAssetsByPath(*SourceCreationPath, AssetsData);

	FString AssetName;
	if (AssetsData.Num() == 0)
	{
		AssetName = FString::Format(TEXT("New{0}"), { Class->GetName() });
	}
	else
	{
		AssetName = FString::Format(TEXT("New{0}{1}"), { Class->GetName(), AssetsData.Num() });
	}

	UMetaHumanCaptureSource* CaptureSourceAsset = Cast<UMetaHumanCaptureSource>(AssetTools.CreateAsset(AssetName, SourceCreationPath, Class, nullptr));
	if (CaptureSourceAsset)
	{
		CaptureSourceAsset->CaptureSourceType = Type;

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(CaptureSourceAsset);
	}
}

FText SCaptureManagerWidget::GetCreationTooltip(FText InTypeName) const
{
	return FText::Format(LOCTEXT("CaptureManagerWidget.CreationTooltip", "Capture Source of type {0} will be created in {1}"), InTypeName, FText::FromString(SourceCreationPath));
}

bool SCaptureManagerWidget::CanSave()
{
	return true;
}

bool SCaptureManagerWidget::CanSaveAll()
{
	return !bAutosaveAfterImport;
}

bool SCaptureManagerWidget::CanRefresh()
{
	return IsCaptureSourceSelected();
}

bool SCaptureManagerWidget::CanStartStopCapture() const
{
	FFootageCaptureSource* CurrentSource = CaptureSourcesWidget.IsValid() ?
		CaptureSourcesWidget->GetCurrentCaptureSource() : nullptr;

	//it is possible to start/stop capture only if the capture source is selected and it is not an archive
	return CurrentSource != nullptr &&
		CurrentSource->GetIngester().GetCaptureSourceType() == EMetaHumanCaptureSourceType::LiveLinkFaceConnection &&
		CurrentSource->Status == EFootageCaptureSourceStatus::Online;
}

bool SCaptureManagerWidget::IsCaptureSourceSelected() const
{
	return CaptureSourcesWidget != nullptr && CaptureSourcesWidget->GetCurrentCaptureSource() != nullptr;
}

void SCaptureManagerWidget::HandleSave()
{
}

void SCaptureManagerWidget::HandleSaveAll()
{
	FootageIngestWidget->SaveImportedAssets();
}

void SCaptureManagerWidget::HandleRefresh()
{
	CaptureSourcesWidget->RefreshCurrentCaptureSource();
}

void SCaptureManagerWidget::HandleStartStopCaptureToggle()
{
	FFootageCaptureSource* CurrentCaptureSource = CaptureSourcesWidget->GetCurrentCaptureSource();
	if (!CurrentCaptureSource)
	{
		UE_LOG(LogCaptureManager, Error, TEXT("Failed to start/stop capture: current capture source is invalid"));
		return;
	}

	if (!CurrentCaptureSource->bIsRecording)
	{
		FText ErrorText;

		bool IsSlateNameValid = VerifySlateName(FText::FromString(CurrentCaptureSource->SlateName), ErrorText);
		SlateNameTextBox->SetError(ErrorText); // Sets or clears (if the error message is empty) the error on the text box
		// Clearing the text box is essentially a workaround for the bug in UE
		if (!IsSlateNameValid)
		{
			return;
		}

		TakeNumberTextBox->SetError(TEXT("")); // Clears (if the error message is empty) the error on the text box
		// Clearing the text box is essentially a workaround for the bug in UE

		TOptional<FString> Subject = TOptional<FString>();
		TOptional<FString> Scenario = TOptional<FString>();
		TSharedPtr<FStartCaptureCommandArgs> Command = MakeShared<FStartCaptureCommandArgs>(CurrentCaptureSource->SlateName,
			CurrentCaptureSource->TakeNumber,
			MoveTemp(Subject),
			MoveTemp(Scenario));

		bool Result = CurrentCaptureSource->GetIngester().ExecuteCommand(MoveTemp(Command));
		if (!Result)
		{
			UE_LOG(LogCaptureManager, Error, TEXT("Failed to start capture"));
			return;
		}
		CurrentCaptureSource->bIsRecording = true;
	}
	else
	{
		TSharedPtr<FStopCaptureCommandArgs> Command = MakeShared<FStopCaptureCommandArgs>();

		bool Result = CurrentCaptureSource->GetIngester().ExecuteCommand(MoveTemp(Command));
		if (!Result)
		{
			UE_LOG(LogCaptureManager, Error, TEXT("Failed to stop capture"));
			return;
		}

		CurrentCaptureSource->bIsRecording = false;
		CurrentCaptureSource->TakeNumber++; //increase take counter every time the capture stops, for the next round
	}

	RegenerateMenusAndToolbars();
}

void SCaptureManagerWidget::PresentTargetPicker()
{
	FString NewSourceCreationPath = SourceCreationPath;
	bool bIsNewPathSet = false;

	FPathPickerConfig PathPickerConfig;
	//the path picker button is disabled if CurrentCaptureSource is not selected, so we can safely use the source, and we also know that the TargetAssetFolderPath is set
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateLambda([&NewSourceCreationPath](const FString& InPath)
		{
			NewSourceCreationPath = InPath;
		});

	PathPickerConfig.DefaultPath = NewSourceCreationPath; //open the picker on the current path (CaptureSource folder by default)
	PathPickerConfig.bAddDefaultPath = false; //since the default path is the path to the current CaptureSource, it surely exists; this flag is do not add it if it doesn't
	PathPickerConfig.bAllowContextMenu = true;
	PathPickerConfig.bAllowClassesFolder = false;
	PathPickerConfig.bOnPathSelectedPassesVirtualPaths = false; //ensures we don't have "/All" prefix in the paths that the picker returns; they will start with "/Game" instead
	PathPickerConfig.bAllowReadOnlyFolders = false;
	PathPickerConfig.bFocusSearchBoxWhenOpened = true;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	TSharedPtr<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("CaptureManager.SelectCreationPath", "Select Source Creation Path"))
		.ClientSize(FVector2D(500.0, 300.0));

	Window->SetContent(
		SNew(SBox)
		.Padding(4)
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(EVerticalAlignment::VAlign_Bottom)
				[
					SNew(SBox)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.Padding(4)
								.HAlign(EHorizontalAlignment::HAlign_Left)
								.VAlign(EVerticalAlignment::VAlign_Center)
								.AutoWidth()
								[
									SNew(SImage)
										.Image(FAppStyle::Get().GetBrush("Icons.FolderClosed"))
								]
								+ SHorizontalBox::Slot()
								.Padding(4)
								.VAlign(EVerticalAlignment::VAlign_Center)
								.FillWidth(1.f)
								[
									SNew(STextBlock)
										.Text_Lambda([&NewSourceCreationPath]()
											{
												return FText::FromString(NewSourceCreationPath);
											})
								]
								+ SHorizontalBox::Slot()
								.Padding(4)
								.HAlign(EHorizontalAlignment::HAlign_Right)
								.VAlign(EVerticalAlignment::VAlign_Center)
								.AutoWidth()
								[
									SNew(SButton)
										.Text(LOCTEXT("Confirm", "Confirm"))
										.OnClicked_Lambda([Window, &bIsNewPathSet]()
											{
												bIsNewPathSet = true;

												Window->RequestDestroyWindow();

												return FReply::Handled();
											})
								]
						]
				]
		]
	);

	FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), FGlobalTabmanager::Get()->GetRootWindow());

	if (bIsNewPathSet)
	{
		SourceCreationPath = NewSourceCreationPath;
	}
}

FText SCaptureManagerWidget::GetAutoSaveOnImportTooltip()
{
	UToolMenu* FoundMenu = UToolMenus::Get()->FindMenu(QuickSettingsMenuName);

	UCaptureManagerEditorContext* Context = FoundMenu->FindContext<UCaptureManagerEditorContext>();

	if (Context->CaptureManagerWidget.Pin()->bAutosaveAfterImport)
	{
		return LOCTEXT("CaptureManagerToolbarAutoSaveCheckboxEnabledTextToolTip", "Disable autosaving assets after Import");
	}

	return LOCTEXT("CaptureManagerToolbarAutoSaveCheckboxDisabledTextToolTip", "Enable autosaving assets after Import");
}

void SCaptureManagerWidget::ToggleAutoSaveOnImport()
{
	UToolMenu* FoundMenu = UToolMenus::Get()->FindMenu(QuickSettingsMenuName);

	UCaptureManagerEditorContext* Context = FoundMenu->FindContext<UCaptureManagerEditorContext>();

	TSharedPtr<SCaptureManagerWidget> Widget = Context->CaptureManagerWidget.Pin();
	Widget->bAutosaveAfterImport = !Widget->bAutosaveAfterImport;

	Widget->FootageIngestWidget->SetAutosaveAfterImport(Widget->bAutosaveAfterImport);
}

bool SCaptureManagerWidget::IsAutoSaveOnImportToggled()
{
	UToolMenu* FoundMenu = UToolMenus::Get()->FindMenu(QuickSettingsMenuName);

	UCaptureManagerEditorContext* Context = FoundMenu->FindContext<UCaptureManagerEditorContext>();

	return Context->CaptureManagerWidget.Pin()->bAutosaveAfterImport;
}

void SCaptureManagerWidget::GenerateToolbar()
{
	RegisterDefaultToolBar();
	ExtendToolBar();
#if !defined(HIDE_MAIN_MENU)
	ExtendMenu();
#endif

	FName ParentToolbarName;
	const FName ToolBarName = GetToolMenuToolbarName(ParentToolbarName);
	UToolMenus* ToolMenus = UToolMenus::Get();
	UToolMenu* FoundMenu = ToolMenus->FindMenu(ToolBarName);
	if (!FoundMenu || !FoundMenu->IsRegistered())
	{
		FoundMenu = ToolMenus->RegisterMenu(ToolBarName, ParentToolbarName, EMultiBoxType::SlimHorizontalToolBar);
	}

	FToolMenuContext MenuContext(GetToolkitCommands());
	InitToolMenuContext(MenuContext);

	UToolMenu* GeneratedToolbar = ToolMenus->GenerateMenu(ToolBarName, MenuContext);
	GeneratedToolbar->bToolBarIsFocusable = false;
	GeneratedToolbar->bToolBarForceSmallIcons = false;

	UToolMenu* CommonActionsToolbar = GenerateCommonActionsToolbar(MenuContext);
	TSharedRef< SWidget > CommonActionsToolbarWidget = ToolMenus->GenerateWidget(CommonActionsToolbar);

	//the command buttons section specific to this toolkit
	TSharedRef<SWidget > CaptureCommandButtonsWidget = ToolMenus->GenerateWidget(GeneratedToolbar);

	RegisterSettingsToolBar(MenuContext);

	UToolMenu* SettingsToolbar = GenerateSettingsToolbar(MenuContext);
	//the section for future Settings button etc on the right edge
	TSharedRef<SWidget> SettingsWidget = ToolMenus->GenerateWidget(SettingsToolbar);

	if (CVarShowCMToolbar.GetValueOnAnyThread())
	{
		Toolbar =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				CommonActionsToolbarWidget
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					.BorderImage(FAppStyle::Get().GetBrush("AssetEditorToolbar.Background"))
					.Padding(FMargin(0.0f))
					[
						GenerateTakeSlateWidget()
					]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.FillWidth(1.0f)
			[
				SNew(SBorder)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.BorderImage(FAppStyle::Get().GetBrush("AssetEditorToolbar.Background"))
					.Padding(FMargin(0.0f))
					[
						CaptureCommandButtonsWidget
					]
			]
#ifdef SHOW_CAPTURE_SOURCE_FILTER
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SNew(SBorder)
					.VAlign(VAlign_Center)
					.BorderImage(FAppStyle::Get().GetBrush("AssetEditorToolbar.Background"))
					.Padding(5.f)
					[
						SNew(SCheckBox)
							.Style(FAppStyle::Get(), "ToggleButtonCheckBox")
							.ToolTipText(this, &SCaptureManagerWidget::GetShowOtherDevelopersToolTip)
							.OnCheckStateChanged(this, &SCaptureManagerWidget::OnShowOtherDevelopersCheckStateChanged)
							.IsChecked(this, &SCaptureManagerWidget::GetShowOtherDevelopersCheckState)
							.Padding(4.f)
							[
								SNew(SImage)
									.ColorAndOpacity(FSlateColor::UseForeground())
									.Image(FAppStyle::GetBrush("ContentBrowser.ColumnViewDeveloperFolderIcon"))
							]
					]
			]
#endif // SHOW_CAPTURE_SOURCE_FILTER
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SNew(SBorder)
					.VAlign(VAlign_Center)
					.BorderImage(FAppStyle::Get().GetBrush("AssetEditorToolbar.Background"))
					.Padding(FMargin(5.0f))
					[
						SettingsWidget
					]
			]
			;
	}
	else
	{
		Toolbar = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
			];
	}

	if (ToolbarWidgetContent.IsValid())
	{
		ToolbarWidgetContent->SetContent(Toolbar.ToSharedRef());
	}
}

void SCaptureManagerWidget::InitToolMenuContext(struct FToolMenuContext& InMenuContext)
{
	UCaptureManagerEditorContext* Context = NewObject<UCaptureManagerEditorContext>();
	Context->CaptureManagerWidget = SharedThis(this);
	InMenuContext.AddObject(Context);
}

FName SCaptureManagerWidget::GetToolkitFName() const
{
	return FName("CaptureManager");
}

FName SCaptureManagerWidget::GetToolMenuAppName() const
{
	return GetToolkitFName();
}

FName SCaptureManagerWidget::GetToolMenuName() const
{
	return *(GetToolMenuAppName().ToString() + TEXT(".MainMenu"));
}

FName SCaptureManagerWidget::GetToolMenuToolbarName() const
{
	FName ParentName;
	return GetToolMenuToolbarName(ParentName);
}

FName SCaptureManagerWidget::GetToolMenuToolbarName(FName& OutParentName) const
{
	OutParentName = DefaultToolbarName;

	return *(GetToolMenuAppName().ToString() + TEXT(".ToolBar"));
}

void SCaptureManagerWidget::RegisterDefaultToolBar()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus->IsMenuRegistered(ToolMenuToolbarName))
	{
		UToolMenu* ToolbarBuilder = ToolMenus->RegisterMenu(ToolMenuToolbarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
		FToolMenuSection& Section = ToolbarBuilder->AddSection("Asset");
	}
}

void SCaptureManagerWidget::RegisterSettingsToolBar(const FToolMenuContext& InContext)
{
	if (UToolMenus::Get()->IsMenuRegistered(QuickSettingsMenuName))
	{
		UToolMenus::Get()->FindMenu(QuickSettingsMenuName)->Context = InContext;

		return;
	}

	UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(QuickSettingsMenuName);
	Menu->Context = InContext;

	{
		FToolMenuSection& Section = Menu->AddSection("CaptureSource", LOCTEXT("CaptureManager_CaptureSourceSettings", "Capture Source Settings"));

		Section.AddMenuEntry(
			"SelectSourceCreationPath",
			LOCTEXT("CaptureManager.SelectSourceCreationPath", "Select Source Creation Path"),
			LOCTEXT("CaptureManager.SelectSourceCreationPathTooltip", "Select the path where the Capture Source will be created"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderClosed"),
			FUIAction(
				FExecuteAction::CreateStatic(&SCaptureManagerWidget::PresentTargetPicker)),
			EUserInterfaceActionType::Button);

#ifdef SHOW_CAPTURE_SOURCE_FILTER
		Section.AddMenuEntry(
			"ShowDevelopersContent",
			ShowDevelopersContentText,
			ShowDevelopersContentTooltipText,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &SCaptureManagerWidget::ToggleShowDevelopersContent),
				FCanExecuteAction(),
				FIsActionChecked::CreateRaw(this, &SCaptureManagerWidget::IsShowingDevelopersContent)
			),
			EUserInterfaceActionType::ToggleButton);
#endif // SHOW_CAPTURE_SOURCE_FILTER
	}

	{
		FToolMenuSection& Section = Menu->AddSection("Import", LOCTEXT("CaptureManager_ImportSettings", "Import Settings"));

		Section.AddMenuEntry(
			"AutoSaveOnImport",
			LOCTEXT("CaptureManager.AutoSaveOnImport", "Auto Save on Import"),
			TAttribute<FText>::Create(&SCaptureManagerWidget::GetAutoSaveOnImportTooltip),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&SCaptureManagerWidget::ToggleAutoSaveOnImport),
				FCanExecuteAction(),
				FIsActionChecked::CreateStatic(&SCaptureManagerWidget::IsAutoSaveOnImportToggled)),
			EUserInterfaceActionType::ToggleButton);
	}
}

UToolMenu* SCaptureManagerWidget::GenerateCommonActionsToolbar(FToolMenuContext& MenuContext)
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	FName ToolBarName = "CaptureManager.CommonActions";

	UToolMenu* FoundMenu = ToolMenus->FindMenu(ToolBarName);

	if (!FoundMenu || !FoundMenu->IsRegistered())
	{
		FoundMenu = ToolMenus->RegisterMenu(ToolBarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
		FoundMenu->StyleName = "AssetEditorToolbar";

		FToolMenuSection& Section = FoundMenu->AddSection("CommonActions");

#ifdef SHOW_SAVE_BUTTON
		//this would require remembering what was saved for which capture source as the user switches between them, which is not supported currently
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			Commands->Save,
			Commands->Save->GetLabel(),
			Commands->Save->GetDescription(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "AssetEditor.SaveAsset")
		));
#endif
		Section.AddEntry(FToolMenuEntry::InitComboButton(
			NAME_None,
			FUIAction(),
			FOnGetContent::CreateStatic(&SCaptureManagerWidget::PopulateAddCaptureSourceComboBox),
			FText(),
			LOCTEXT("CaptureManagerWidget_AddCaptureSourceTooltip", "Adds the capture source to the specified location"),
			FSlateIcon(FMetaHumanFootageRetrievalWindowStyle::Get().GetStyleSetName(), "CaptureManager.Toolbar.AddSource")
		));

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			Commands->SaveAll,
			Commands->SaveAll->GetLabel(),
			Commands->SaveAll->GetDescription(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.SaveAll")
		));
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			Commands->Refresh,
			Commands->Refresh->GetLabel(),
			Commands->Refresh->GetDescription(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh")
		));

#ifdef SHOW_JUMP_TO_CONTENT_BROWSER
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(FGlobalEditorCommonCommands::Get().FindInContentBrowser,
			LOCTEXT("FindInContentBrowserButton", "Browse")));
#endif //SHOW_JUMP_TO_CONTENT_BROWSER

		if (CVarShowCMToolbar.GetValueOnAnyThread())
		{
			Section.AddSeparator(NAME_None);
		}
	}

	FoundMenu->Context = MenuContext;

	return FoundMenu;
}

UToolMenu* SCaptureManagerWidget::GenerateSettingsToolbar(FToolMenuContext& MenuContext)
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	FName ToolBarName = "CaptureManager.Settings";

	UToolMenu* FoundMenu = ToolMenus->FindMenu(ToolBarName);

	if (!FoundMenu || !FoundMenu->IsRegistered())
	{
		FoundMenu = ToolMenus->RegisterMenu(ToolBarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
		FoundMenu->StyleName = "AssetEditorToolbar";

		{
			FToolMenuSection& SettingsSection = FoundMenu->AddSection("ProjectSettings");
			FToolMenuEntry SettingsEntry =
				FToolMenuEntry::InitComboButton(
					"CaptureManagerQuickSettings",
					FUIAction(),
					FOnGetContent::CreateStatic(&SCaptureManagerWidget::GenerateQuickSettingsMenu),
					LOCTEXT("CaptureManagerQuickSettingsCombo", "Settings"),
					LOCTEXT("CaptureManagerQuickSettingsCombo_ToolTip", "Capture Manager Settings"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings"),
					false,
					"CaptureManagerQuickSettings");
			SettingsEntry.StyleNameOverride = "CalloutToolbar";

			SettingsSection.AddEntry(SettingsEntry);
		}
	}

	FoundMenu->Context = MenuContext;

	return FoundMenu;
}

TSharedRef<SWidget> SCaptureManagerWidget::GenerateQuickSettingsMenu()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	FName ToolBarName = "CaptureManager.Settings";

	UToolMenu* FoundMenu = ToolMenus->FindMenu(ToolBarName);

	return UToolMenus::Get()->GenerateWidget(QuickSettingsMenuName, FoundMenu->Context);
}

void SCaptureManagerWidget::GenerateMessageWidget()
{
	if (!IModularFeatures::Get().IsModularFeatureAvailable(IFaceTrackerNodeImplFactory::GetModularFeatureName()))
	{
		FText DepthWarningMessage = LOCTEXT("DepthPluginNotEnabled", "Some MetaHuman Animator processes may not function as expected. Please make sure the Depth Processing plugin is enabled. (Available on Fab)");

		MessageWidget =
			SNew(SWarningOrErrorBox)
			.MessageStyle(EMessageStyle::Warning)
			.Message(DepthWarningMessage);
	}
	else
	{
		MessageWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
			];
	}
}

FTabSpawnerEntry& SCaptureManagerWidget::RegisterCaptureSourcesTabSpawner()
{
	const auto& CreateCaptureSourcesTab = [this](const FSpawnTabArgs& SpawnTabArgs) -> TSharedRef<SDockTab>
		{
			check(SpawnTabArgs.GetTabId() == CaptureSourcesTabName);

			const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
				.Label(LOCTEXT("CaptureSourceTabLabel", "Capture Sources"))
				.CanEverClose(false)
				.OnCanCloseTab_Lambda([]()
					{
						return false;
					});

			TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox);
			DockTab->SetContent(VBox);

			VBox->AddSlot()
				.FillHeight(1.0f)
				[
					//Capture Sources list 
					SNew(SBox)
						.Padding(FMargin(4.f))
						[
							SNew(SBorder)
								.Padding(FMargin(0))
								.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
								[
									SAssignNew(CaptureSourcesWidget, SCaptureSourcesWidget)
										.OwnerTab(DockTab)
										.OnCurrentCaptureSourceChanged(this, &SCaptureManagerWidget::OnCurrentCaptureSourceChanged)
										.OnCaptureSourcesChanged(this, &SCaptureManagerWidget::OnCaptureSourcesChanged)
										.OnCaptureSourceUpdated(this, &SCaptureManagerWidget::OnCaptureSourceUpdated)
										.OnCaptureSourceFinishedImportingTakes(this, &SCaptureManagerWidget::OnCaptureSourceFinishedImportingTakes)
								]
						]
				];

			if (FootageIngestWidget.IsValid())
			{
				Initialize();
			}

			return DockTab;
		};

	return TabManager->RegisterTabSpawner(CaptureSourcesTabName, FOnSpawnTab::CreateLambda(CreateCaptureSourcesTab))
		.SetDisplayName(LOCTEXT("CaptureSourcesTabSpawner", "Capture Sources"))
		.SetIcon(FSlateIcon(FMetaHumanFootageRetrievalWindowStyle::Get().GetStyleSetName(), TEXT("CaptureManager.Tabs.CaptureSources")));
}

FTabSpawnerEntry& SCaptureManagerWidget::RegisterFootageIngestTabSpawner()
{
	const auto& CreateFootageIngestTab = [this](const FSpawnTabArgs& SpawnTabArgs) -> TSharedRef<SDockTab>
		{
			check(SpawnTabArgs.GetTabId() == FootageIngestTabName);

			const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
				.Label(LOCTEXT("FootageIngestTabLabel", "Footage Ingest"))
				.CanEverClose(false)
				.OnCanCloseTab_Lambda([]()
					{
						return false;
					});

			TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox);
			DockTab->SetContent(VBox);

			VBox->AddSlot()
				.FillHeight(1.0f)
				[
					//Capture Sources list 
					SNew(SBox)
						.Padding(FMargin(4.f))
						[
							SNew(SBorder)
								.Padding(FMargin(0))
								.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
								[
									SAssignNew(FootageIngestWidget, SFootageIngestWidget)
										.OwnerTab(DockTab)
										.OnTargetFolderAssetPathChanged(this, &SCaptureManagerWidget::OnTargetFolderAssetPathChanged)
								]
						]
				];

			if (CaptureSourcesWidget.IsValid())
			{
				Initialize();
			}

			if (FootageIngestWidget.IsValid())
			{
				FootageIngestWidget->SetDefaultAssetCreationPath(DefaultAssetCreationPath);
			}

			return DockTab;
		};

	return TabManager->RegisterTabSpawner(FootageIngestTabName, FOnSpawnTab::CreateLambda(CreateFootageIngestTab))
		.SetDisplayName(LOCTEXT("FootageIngestTabSpawner", "Footage Ingest"))
		.SetIcon(FSlateIcon(FMetaHumanFootageRetrievalWindowStyle::Get().GetStyleSetName(), TEXT("CaptureManager.Tabs.FootageIngest")));
}


TWeakPtr<SDockTab> SCaptureManagerWidget::ShowMonitoringTab(UMetaHumanCaptureSource* const CaptureSource)
{
	// Create tab if not existent.
	TWeakPtr<SDockTab>& MonitoringDockTab = CaptureSourceToTabMap.FindOrAdd(MakeWeakObjectPtr(CaptureSource));

	if (!MonitoringDockTab.IsValid())
	{
		//TODO: create a new dynamic tab for monitoring capture source
		//(ShowTargetEditorTab in LocalizationDashboard as an example)
	}
	else
	{
		const TSharedPtr<SDockTab> OldMonitoringDockTab = MonitoringDockTab.Pin();
		TabManager->DrawAttention(OldMonitoringDockTab.ToSharedRef());
	}
	return MonitoringDockTab;
}

void SCaptureManagerWidget::OnCurrentCaptureSourceChanged(TSharedPtr<FFootageCaptureSource> InCaptureSource, ESelectInfo::Type InSelectInfo)
{
	if (FootageIngestWidget.IsValid())
	{
		FootageIngestWidget->OnCurrentCaptureSourceChanged(InCaptureSource, InSelectInfo);
	}
}

void SCaptureManagerWidget::OnCaptureSourcesChanged(TArray<TSharedPtr<FFootageCaptureSource>> InCaptureSources)
{
	if (FootageIngestWidget.IsValid())
	{
		FootageIngestWidget->OnCaptureSourcesChanged(InCaptureSources);
	}
}

void SCaptureManagerWidget::OnCaptureSourceUpdated(TSharedPtr<FFootageCaptureSource> InCaptureSource)
{
	if (FootageIngestWidget.IsValid())
	{
		FootageIngestWidget->OnCaptureSourceUpdated(InCaptureSource);
	}
}

void SCaptureManagerWidget::OnCaptureSourceFinishedImportingTakes(const TArray<FMetaHumanTake>& InTakes, TSharedPtr<FFootageCaptureSource> InCaptureSource)
{
	if (FootageIngestWidget.IsValid())
	{
		FootageIngestWidget->OnCaptureSourceFinishedImportingTakes(InTakes, InCaptureSource);
	}
}

void SCaptureManagerWidget::OnTargetFolderAssetPathChanged(FText InTargetFolderAssetPath)
{
	if (CaptureSourcesWidget.IsValid())
	{
		CaptureSourcesWidget->OnTargetFolderAssetPathChanged(InTargetFolderAssetPath);
	}
}

void SCaptureManagerWidget::ExtendToolBar(bool bRegenerateDynamicSection)
{
	const FName MainToolbarMenuName = GetToolMenuToolbarName();
	const FName SectionName = UToolMenus::JoinMenuPaths(MainToolbarMenuName, DynamicToolbarSectionName);

	if (UToolMenu* ToolBarMenu = UToolMenus::Get()->ExtendMenu(MainToolbarMenuName))
	{
		// Define the dynamic section only once and use the UMetaHumanIdentityAssetEditorContext 
		// to get the state of the open asset
		if (!ToolBarMenu->FindSection(SectionName) || bRegenerateDynamicSection)
		{
			if (bRegenerateDynamicSection) //in case of pressing Start/Stop we want this section to be regenerated
			{
				ToolBarMenu->RemoveSection(SectionName);
			}
			ToolBarMenu->AddDynamicSection(
				SectionName,
				FNewToolMenuDelegate::CreateLambda([this](UToolMenu* InMenu)
					{
						UCaptureManagerEditorContext* Context = InMenu->FindContext<UCaptureManagerEditorContext>();
						if (Context && Context->CaptureManagerWidget.IsValid())
						{
							SCaptureManagerWidget* CaptureManagerWidget = Context->CaptureManagerWidget.Pin().Get();
							FToolMenuSection& CaptureSection = InMenu->AddSection(TEXT("Capture"));
							{
								CaptureSection.AddEntry(
									FToolMenuEntry::InitToolBarButton(
										Commands->StartStopCapture,
										TAttribute<FText>::CreateSP(CaptureManagerWidget, &SCaptureManagerWidget::GetStartStopCaptureButtonLabel),
										TAttribute<FText>::CreateSP(CaptureManagerWidget, &SCaptureManagerWidget::GetStartStopCaptureButtonTooltip),
										TAttribute<FSlateIcon>::CreateSP(CaptureManagerWidget, &SCaptureManagerWidget::GetStartStopCaptureButtonIcon)
									)
								);

								CaptureSection.AddSeparator(NAME_None);
							}
						}
					})
			);
		}
	}
}

void SCaptureManagerWidget::RemoveDynamicToolbarSection() const
{
	const FName MainToolbarMenuName = GetToolMenuToolbarName();
	const FName SectionName = UToolMenus::JoinMenuPaths(MainToolbarMenuName, DynamicToolbarSectionName);

	if (UToolMenu* ToolBarMenu = UToolMenus::Get()->FindMenu(MainToolbarMenuName))
	{
		ToolBarMenu->RemoveSection(SectionName);
	}
}

FText SCaptureManagerWidget::GetStartStopCaptureButtonLabel() const
{
	if (!IsCurrentSourceRecording())
	{
		return LOCTEXT("StartStopCaptureButtonLabel_Start", "Start Capture");
	}

	return LOCTEXT("StartStopCaptureButtonLabel_Stop", "Stop Capture");
}

FText SCaptureManagerWidget::GetStartStopCaptureButtonTooltip() const
{
	FText TooltipText;

	FFootageCaptureSource* CurrentCaptureSource = CaptureSourcesWidget.IsValid() ?
		CaptureSourcesWidget->GetCurrentCaptureSource() : nullptr;
	if (CurrentCaptureSource == nullptr || !CurrentCaptureSource->bIsRecording)
	{
		TooltipText = LOCTEXT("StartStopCaptureButtonTooltip_Start", "Start capturing on a remote device");
	}
	else
	{
		TooltipText = LOCTEXT("StartStopCaptureButtonTooltip_Stop", "Stop capturing on a remote device");
	}

	if (!CanStartStopCapture())
	{
		if (CurrentCaptureSource == nullptr)
		{
			TooltipText = FText::Format(LOCTEXT("StartStopCaptureButtonTooltip_NotSelected", "{0}\n\nCapture Source is not selected"), TooltipText);
		}
		else if (CurrentCaptureSource->GetIngester().GetCaptureSourceType() != EMetaHumanCaptureSourceType::LiveLinkFaceConnection)
		{
			TooltipText = FText::Format(LOCTEXT("StartStopCaptureButtonTooltip_NotSupported", "{0}\n\nSelected Capture Source does not support remote capture"), TooltipText);
		}
		else
		{
			TooltipText = FText::Format(LOCTEXT("StartStopCaptureButtonTooltip_NotConnected", "{0}\n\nCapture Source is not connected"), TooltipText);
		}
	}

	return TooltipText;
}

FSlateIcon SCaptureManagerWidget::GetStartStopCaptureButtonIcon() const
{
	if (!IsCurrentSourceRecording())
	{
		return FSlateIcon{ FMetaHumanFootageRetrievalWindowStyle::Get().GetStyleSetName(), TEXT("CaptureManager.Toolbar.StartCapture") };
	}

	return FSlateIcon{ FMetaHumanFootageRetrievalWindowStyle::Get().GetStyleSetName(), TEXT("CaptureManager.Toolbar.StopCapture") };
}

bool SCaptureManagerWidget::IsCurrentSourceRecording() const
{
	if (!CaptureSourcesWidget)
	{
		return false;
	}

	const FFootageCaptureSource* CurrentCaptureSource = CaptureSourcesWidget->GetCurrentCaptureSource();
	if (!CurrentCaptureSource || !CurrentCaptureSource->bIsRecording)
	{
		return false;
	}

	return true;
}

void SCaptureManagerWidget::ExtendMenu(bool bRegenerateMenu)
{
	const FName CaptureMenuName = UToolMenus::JoinMenuPaths(GetToolMenuAppName(), TEXT("Capture"));

	UToolMenus* ToolMenus = UToolMenus::Get();

	bool bMenuRegistered = ToolMenus->IsMenuRegistered(CaptureMenuName);
	if (!bMenuRegistered || bRegenerateMenu)
	{
		if (bRegenerateMenu)
		{
			ToolMenus->RemoveMenu(CaptureMenuName);
		}

		UToolMenu* CaptureMenu = ToolMenus->RegisterMenu(CaptureMenuName);

		if (CVarShowCMToolbar.GetValueOnAnyThread())
		{
			//We should not have sections with a single option, putting Refresh into Capture section
			FToolMenuSection& CaptureSection = CaptureMenu->AddSection(TEXT("Capture"), LOCTEXT("CaptureSection", "Capture"));
			{
				CaptureSection.AddMenuEntry(
					Commands->Refresh->GetCommandName(),
					Commands->Refresh->GetLabel(),
					Commands->Refresh->GetDescription(),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
					FUIAction(
						FExecuteAction::CreateSP(this, &SCaptureManagerWidget::HandleRefresh),
						FCanExecuteAction::CreateSP(this, &SCaptureManagerWidget::CanRefresh)
					)
				);

				CaptureSection.AddMenuEntry(
					Commands->StartStopCapture->GetCommandName(),
					TAttribute<FText>::CreateSP(this, &SCaptureManagerWidget::GetStartStopCaptureButtonLabel),
					TAttribute<FText>::CreateSP(this, &SCaptureManagerWidget::GetStartStopCaptureButtonTooltip),
					TAttribute<FSlateIcon>::CreateSP(this, &SCaptureManagerWidget::GetStartStopCaptureButtonIcon),
					FUIAction(
						FExecuteAction::CreateSP(this, &SCaptureManagerWidget::HandleStartStopCaptureToggle),
						FCanExecuteAction::CreateSP(this, &SCaptureManagerWidget::CanStartStopCapture)
					)
				);
			}
		}
	}

	const FName CaptureManagerMainMenuName = UToolMenus::JoinMenuPaths(GetToolMenuName(), TEXT("Capture"));

	if (!ToolMenus->IsMenuRegistered(CaptureManagerMainMenuName))
	{
		ToolMenus->RegisterMenu(CaptureManagerMainMenuName, CaptureMenuName);
	}

	if (UToolMenu* MainMenu = ToolMenus->ExtendMenu(GetToolMenuName()))
	{
		const FToolMenuInsert MenuInsert{ TEXT("Tools"), EToolMenuInsertType::After };

		FToolMenuSection& Section = MainMenu->FindOrAddSection(NAME_None);

		FToolMenuEntry& CaptureEntry = Section.AddSubMenu(TEXT("Capture"),
			LOCTEXT("CaptureManagerCaptureMenuLabel", "Capture"),
			LOCTEXT("CaptureManagerCaptureMenuTooltip", "Commands for capturing footage on a remote device"),
			FNewToolMenuChoice{});
		CaptureEntry.InsertPosition = MenuInsert;
	}
}

bool SCaptureManagerWidget::VerifyTakeNumber(const FText& InNewNumber, FText& OutErrorText)
{
	if (!InNewNumber.IsNumeric())
	{
		OutErrorText = LOCTEXT("CaptureManagerTakeNumberNoneNumericError", "Value must be numeric");
		return false;
	}

	int32 DesiredTakeNumber = FCString::Atoi(*InNewNumber.ToString());

	const TRange<int32> TakeNumberRange = TRange<int32>::Inclusive(1, 999);
	if (!TakeNumberRange.Contains(DesiredTakeNumber))
	{
		OutErrorText = LOCTEXT("CaptureManagerTakeNumberRangeError", "Value must be in range [1-999]");
		return false;
	}

	return true;
}

void SCaptureManagerWidget::HandleTakeNumberCommited(const FText& InNewNumber, ETextCommit::Type InCommitType)
{
	if (FFootageCaptureSource* CurrentCaptureSource = CaptureSourcesWidget->GetCurrentCaptureSource())
	{
		CurrentCaptureSource->TakeNumber = FCString::Atoi(*InNewNumber.ToString());
	}
}

FText SCaptureManagerWidget::GetTakeNumber() const
{
	if (CaptureSourcesWidget == nullptr)
	{
		return FText();
	}

	const FFootageCaptureSource* CurrentCaptureSource = CaptureSourcesWidget->GetCurrentCaptureSource();
	if (CurrentCaptureSource == nullptr)
	{
		return FText();
	}

	FNumberFormattingOptions Options;
	Options.SetMaximumIntegralDigits(3);
	Options.SetMaximumFractionalDigits(0);

	return FText::AsNumber(CurrentCaptureSource->TakeNumber, &Options);
}

bool SCaptureManagerWidget::VerifySlateName(const FText& InNewName, FText& OutErrorText)
{
	if (InNewName.IsEmpty())
	{
		OutErrorText = LOCTEXT("CaptureSlateNameTextBox", "Slate Name cannot be empty");
		return false;
	}

	return true;
}

void SCaptureManagerWidget::HandleSlateNameTextCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	if (FFootageCaptureSource* CurrentCaptureSource = CaptureSourcesWidget->GetCurrentCaptureSource())
	{
		//reset the take number for every new slate
		if (CurrentCaptureSource->SlateName != InText.ToString())
		{
			CurrentCaptureSource->SlateName = InText.ToString();
			CurrentCaptureSource->TakeNumber = 1;
		}
	}
}

FText SCaptureManagerWidget::GetSlateName() const
{
	if (CaptureSourcesWidget == nullptr)
	{
		return FText();
	}

	const FFootageCaptureSource* CurrentCaptureSource = CaptureSourcesWidget->GetCurrentCaptureSource();
	if (CurrentCaptureSource == nullptr)
	{
		return FText();
	}

	return FText::FromString(CurrentCaptureSource->SlateName);
}

void SCaptureManagerWidget::StartCaptureConsoleHandler(const TArray<FString>& InArguments)
{
	if (InArguments.Num() < 2)
	{
		UE_LOG(LogCaptureManager, Error, TEXT("Failed to start capture: Invalid number of arguments"));
		return;
	}

	if (!InArguments[1].IsNumeric())
	{
		UE_LOG(LogCaptureManager, Error, TEXT("Failed to start capture: TakeNumber argument must a number"));
		return;
	}

	FString Slate = InArguments[0];
	uint16 SlateTakeNumber = FCString::Atoi(*InArguments[1]);

	TOptional<FString> Subject = InArguments.Num() >= 3 ? InArguments[2] : TOptional<FString>();
	TOptional<FString> Scenario = InArguments.Num() >= 4 ? InArguments[3] : TOptional<FString>();

	FFootageCaptureSource* CurrentCaptureSource = CaptureSourcesWidget->GetCurrentCaptureSource();
	if (CurrentCaptureSource)
	{
		TSharedPtr<FStartCaptureCommandArgs> Command = MakeShared<FStartCaptureCommandArgs>(MoveTemp(Slate), SlateTakeNumber, MoveTemp(Subject), MoveTemp(Scenario));

		bool Result = CurrentCaptureSource->GetIngester().ExecuteCommand(MoveTemp(Command));
		if (!Result)
		{
			UE_LOG(LogCaptureManager, Error, TEXT("Failed to start capture"));
		}
	}
	else
	{
		UE_LOG(LogCaptureManager, Error, TEXT("Failed to start capture: Capture Source is not selected"));
	}
}

void SCaptureManagerWidget::StopCaptureConsoleHandler()
{
	FFootageCaptureSource* CurrentCaptureSource = CaptureSourcesWidget->GetCurrentCaptureSource();
	StopCaptureHandler(CurrentCaptureSource, true);
}

void SCaptureManagerWidget::StopCaptureHandler(FFootageCaptureSource* InSource, bool bInShouldFetchTake)
{
	const FFootageCaptureSource* CurrentCaptureSource = CaptureSourcesWidget->GetCurrentCaptureSource();
	if (InSource)
	{
		TSharedPtr<FStopCaptureCommandArgs> Command = MakeShared<FStopCaptureCommandArgs>(bInShouldFetchTake);

		bool Result = InSource->GetIngester().ExecuteCommand(MoveTemp(Command));
		if (!Result)
		{
			UE_LOG(LogCaptureManager, Error, TEXT("Failed to stop capture"));
		}
	}
	else
	{
		UE_LOG(LogCaptureManager, Error, TEXT("Failed to stop capture: Capture Source doesn't exist"));
	}
}

void SCaptureManagerWidget::RegenerateMenusAndToolbars()
{
	bool bRegenerate = true;
	ExtendToolBar(bRegenerate);

#if !defined(HIDE_MAIN_MENU)
	ExtendMenu(bRegenerate);
#endif
}

FReply SCaptureManagerWidget::OpenLiveLinkHub()
{
	UE::LiveLinkHubLauncherUtils::OpenLiveLinkHub();
	return FReply::Handled();
}

bool SCaptureManagerWidget::CanClose()
{
	bool bCanClose = true;

	if (FootageIngestWidget)
	{
		bCanClose &= FootageIngestWidget->CanClose();
	}

	if (CaptureSourcesWidget && bCanClose)
	{
		bCanClose &= CaptureSourcesWidget->CanClose();

		if (bCanClose)
		{
			TArray<TSharedPtr<FFootageCaptureSource>> CaptureSources = CaptureSourcesWidget->GetCaptureSources();
			for (const TSharedPtr<FFootageCaptureSource>& CaptureSource : CaptureSources)
			{
				if (CaptureSource->bIsRecording)
				{
					StopCaptureHandler(CaptureSource.Get(), false);
				}
			}
		}
	}

	return bCanClose;
}

void SCaptureManagerWidget::OnClose()
{
	if (CaptureSourcesWidget)
	{
		CaptureSourcesWidget->OnClose();
	}

	if (FootageIngestWidget)
	{
		FootageIngestWidget->OnClose();
	}

	bIsInitialized = false;
}

void SCaptureManagerWidget::ToggleShowDevelopersContent()
{
	ensureMsgf(CaptureSourcesWidget, TEXT("Capture sources widget is nullptr"));

	if (CaptureSourcesWidget)
	{
		CaptureSourcesWidget->ToggleShowDevelopersContent();
	}
}

bool SCaptureManagerWidget::IsShowingDevelopersContent()
{
	ensureMsgf(CaptureSourcesWidget, TEXT("Capture sources widget is nullptr"));

	if (CaptureSourcesWidget)
	{
		return CaptureSourcesWidget->IsShowingDevelopersContent();
	}

	return false;
}


FText SCaptureManagerWidget::GetShowOtherDevelopersToolTip() const
{
	const ECheckBoxState CheckBoxState = GetShowOtherDevelopersCheckState();

	switch (CheckBoxState)
	{
	case ECheckBoxState::Unchecked:
		return OtherDevelopersFilterTooltipHidingText;
	case ECheckBoxState::Checked:
		return OtherDevelopersFilterTooltipShowingText;
	default: checkNoEntry();
		return FText::GetEmpty();
	}
}

void SCaptureManagerWidget::OnShowOtherDevelopersCheckStateChanged([[maybe_unused]] const ECheckBoxState InCheckBoxState)
{
	ensureMsgf(CaptureSourcesWidget, TEXT("Capture sources widget is nullptr"));

	if (CaptureSourcesWidget)
	{
		CaptureSourcesWidget->ToggleShowOtherDevelopersContent();
	}
}

ECheckBoxState SCaptureManagerWidget::GetShowOtherDevelopersCheckState() const
{
	ensureMsgf(CaptureSourcesWidget, TEXT("Capture sources widget is nullptr"));

	ECheckBoxState CheckBoxState = ECheckBoxState::Undetermined;

	if (CaptureSourcesWidget)
	{
		CheckBoxState = CaptureSourcesWidget->IsShowingOtherDevelopersContent() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return CheckBoxState;
}

#undef LOCTEXT_NAMESPACE
