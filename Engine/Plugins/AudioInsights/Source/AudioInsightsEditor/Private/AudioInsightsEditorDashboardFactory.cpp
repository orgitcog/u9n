// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioInsightsEditorDashboardFactory.h"

#include "Async/Async.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "AudioInsightsEditorModule.h"
#include "AudioInsightsEditorSettings.h"
#include "AudioInsightsStyle.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/Docking/TabManager.h"
#include "Internationalization/Text.h"
#include "IPropertyTypeCustomization.h"
#include "Kismet2/DebuggerCommands.h"
#include "Templates/SharedPointer.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	namespace EditorDashboardFactoryPrivate
	{
		static const FText ToolName = LOCTEXT("AudioDashboard_ToolName", "Audio Insights");

		static const FName MainToolbarName = "MainToolbar";
		static const FText MainToolbarDisplayName = LOCTEXT("AudioDashboard_MainToolbarDisplayName", "Dashboard Transport");

		static const FText PreviewDeviceDisplayName = LOCTEXT("AudioDashboard_PreviewDevice", "[Preview Audio]");
		static const FText DashboardWorldSelectDescription = LOCTEXT("AudioDashboard_SelectWorldDescription", "Select world(s) to monitor (worlds may share audio output).");

		static const FText OnlyTraceAudioChannelsName = LOCTEXT("AudioDashboard_OnlyTraceAudioChannelsDisplayName", "Only trace audio channels during PIE:");
		static const FText OnlyTraceAudioChannelsDescription = LOCTEXT("AudioDashboard_OnlyTraceAudioChannelsDescription", "Disable all non-command line trace channels apart from Audio, Audio Mixer and CPU during PIE. This will reduce the file sizes of trace sessions while using Audio Insights.");

		FText GetDebugNameFromDeviceId(::Audio::FDeviceId InDeviceId)
		{
			FString WorldName;
			if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
			{
				TArray<UWorld*> DeviceWorlds = DeviceManager->GetWorldsUsingAudioDevice(InDeviceId);
				for (const UWorld* World : DeviceWorlds)
				{
					if (!WorldName.IsEmpty())
					{
						WorldName += TEXT(", ");
					}
					WorldName += World->GetDebugDisplayName();
				}
			}

			if (WorldName.IsEmpty())
			{
				return PreviewDeviceDisplayName;
			}

			return FText::FromString(WorldName);
		}
	} // namespace EditorDashboardFactoryPrivate

	void FEditorDashboardFactory::OnWorldRegisteredToAudioDevice(const UWorld* InWorld, ::Audio::FDeviceId InDeviceId)
	{
		if (InDeviceId != INDEX_NONE)
		{
			StartTraceAnalysis(InWorld, InDeviceId);
		}

		RefreshDeviceSelector();
	}

	void FEditorDashboardFactory::OnWorldUnregisteredFromAudioDevice(const UWorld* InWorld, ::Audio::FDeviceId InDeviceId)
	{
		RefreshDeviceSelector();
	}

	void FEditorDashboardFactory::OnPIEStarted(bool bSimulating)
	{
		IAudioInsightsTraceModule& TraceModule = FAudioInsightsEditorModule::GetChecked().GetTraceModule();
		TraceModule.StartTraceAnalysis(bOnlyTraceAudioChannels);
	}

	void FEditorDashboardFactory::OnPIEStopped(bool bSimulating)
	{
		IAudioInsightsTraceModule& TraceModule = FAudioInsightsEditorModule::GetChecked().GetTraceModule();
		TraceModule.StopTraceAnalysis();

		RefreshDeviceSelector();
	}

	void FEditorDashboardFactory::OnDeviceCreated(::Audio::FDeviceId InDeviceId)
	{
		OnActiveAudioDeviceChanged.Broadcast();
	}

	void FEditorDashboardFactory::OnDeviceDestroyed(::Audio::FDeviceId InDeviceId)
	{
		if (ActiveDeviceId == InDeviceId)
		{
			if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
			{
				ActiveDeviceId = DeviceManager->GetMainAudioDeviceID();
			}
		}

		AudioDeviceIds.RemoveAll([InDeviceId](const TSharedPtr<::Audio::FDeviceId>& DeviceIdPtr)
		{
			return *DeviceIdPtr.Get() == InDeviceId;
		});

		if (AudioDeviceComboBox.IsValid())
		{
			AudioDeviceComboBox->RefreshOptions();
		}

		OnActiveAudioDeviceChanged.Broadcast();
	}

	void FEditorDashboardFactory::RefreshDeviceSelector()
	{
		if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
		{
			if (!DeviceManager->IsValidAudioDevice(ActiveDeviceId))
			{
				ActiveDeviceId = DeviceManager->GetMainAudioDeviceID();
			}
		}

		AudioDeviceIds.Empty();
		if (const FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
		{
			DeviceManager->IterateOverAllDevices([this, &DeviceManager](::Audio::FDeviceId DeviceId, const FAudioDevice* AudioDevice)
			{
				AudioDeviceIds.Add(MakeShared<::Audio::FDeviceId>(DeviceId));
			});
		}

		if (AudioDeviceComboBox.IsValid())
		{
			AudioDeviceComboBox->RefreshOptions();
		}
	}

	void FEditorDashboardFactory::ResetDelegates()
	{
		if (OnWorldRegisteredToAudioDeviceHandle.IsValid())
		{
			FAudioDeviceWorldDelegates::OnWorldRegisteredToAudioDevice.Remove(OnWorldRegisteredToAudioDeviceHandle);
			OnWorldRegisteredToAudioDeviceHandle.Reset();
		}

		if (OnWorldUnregisteredFromAudioDeviceHandle.IsValid())
		{
			FAudioDeviceWorldDelegates::OnWorldUnregisteredWithAudioDevice.Remove(OnWorldUnregisteredFromAudioDeviceHandle);
			OnWorldUnregisteredFromAudioDeviceHandle.Reset();
		}

		if (OnDeviceCreatedHandle.IsValid())
		{
			FAudioDeviceManagerDelegates::OnAudioDeviceCreated.Remove(OnDeviceCreatedHandle);
			OnDeviceCreatedHandle.Reset();
		}

		if (OnDeviceDestroyedHandle.IsValid())
		{
			FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.Remove(OnDeviceDestroyedHandle);
			OnDeviceDestroyedHandle.Reset();
		}

		if (OnPIEStartedHandle.IsValid())
		{
			FEditorDelegates::PreBeginPIE.Remove(OnPIEStartedHandle);
			OnPIEStartedHandle.Reset();
		}

		if (OnPIEStoppedHandle.IsValid())
		{
			FEditorDelegates::EndPIE.Remove(OnPIEStoppedHandle);
			OnPIEStoppedHandle.Reset();
		}
	}

	::Audio::FDeviceId FEditorDashboardFactory::GetDeviceId() const
	{
		return ActiveDeviceId;
	}

	TSharedRef<SDockTab> FEditorDashboardFactory::MakeDockTabWidget(const FSpawnTabArgs& Args)
	{
		const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.Label(EditorDashboardFactoryPrivate::ToolName)
			.Clipping(EWidgetClipping::ClipToBounds)
			.TabRole(ETabRole::NomadTab);

		DashboardTabManager = FGlobalTabmanager::Get()->NewTabManager(DockTab);

		DashboardTabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateStatic([](const TSharedRef<FTabManager::FLayout>& InLayout)
		{
			if (InLayout->GetPrimaryArea().Pin().IsValid())
			{
				FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, InLayout);
			}
		}));

		InitDelegates();

		RegisterTabSpawners();
		RefreshDeviceSelector();

		if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get();
			DeviceManager && GEditor && GEditor->PlayWorld)
		{
			StartTraceAnalysis(GEditor->PlayWorld, DeviceManager->GetActiveAudioDevice()->DeviceID);
		}

		const TSharedRef<FTabManager::FLayout> TabLayout = LoadLayoutFromConfig();

		const TSharedRef<SWidget> TabContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeMenuBarWidget()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeMainToolbarWidget()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(4.0f)
			]
			+ SVerticalBox::Slot()
			[
				DashboardTabManager->RestoreFrom(TabLayout, Args.GetOwnerWindow()).ToSharedRef()
			];

		DockTab->SetContent(TabContent);

		DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([this](TSharedRef<SDockTab> TabClosed)
		{
			// If we are still in PIE, make sure we stop tracing for Audio Insights
			if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get();
				DeviceManager && GEditor && GEditor->PlayWorld)
			{
				IAudioInsightsTraceModule& TraceModule = FAudioInsightsEditorModule::GetChecked().GetTraceModule();
				TraceModule.StopTraceAnalysis();
			}

			ResetDelegates();
			UnregisterTabSpawners();
			SaveLayoutToConfig();

			for (const auto& KVP : DashboardViewFactories)
			{
				if (TSharedPtr<SDockTab> DashboardTab = DashboardTabManager->FindExistingLiveTab(KVP.Key))
				{
					// Explicitly close each dashboard tab. This will give a chance for it to close any undocked sub-managed tabs of its own:
					DashboardTab->RequestCloseTab();
				}
			}

			DashboardTabManager->CloseAllAreas();

			DashboardTabManager.Reset();
			DashboardWorkspace.Reset();
		}));

		return DockTab;
	}

	TSharedRef<SWidget> FEditorDashboardFactory::MakeMenuBarWidget()
	{
		FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList>());

		MenuBarBuilder.AddPullDownMenu(
			LOCTEXT("File_MenuLabel", "File"),
			FText::GetEmpty(),
			FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
			{
				MenuBuilder.AddMenuEntry(LOCTEXT("Close_MenuLabel", "Close"),
					LOCTEXT("Close_MenuLabel_Tooltip", "Closes the Audio Insights dashboard."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([this]()
					{
						if (DashboardTabManager.IsValid())
						{
							if (TSharedPtr<SDockTab> OwnerTab = DashboardTabManager->GetOwnerTab())
							{
								OwnerTab->RequestCloseTab();
							}
						}
					}))
				);
			}),
			"File"
		);

		MenuBarBuilder.AddPullDownMenu(
			LOCTEXT("ViewMenuLabel", "View"),
			FText::GetEmpty(),
			FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
			{
				for (const auto& KVP : DashboardViewFactories)
				{
					const FName& FactoryName = KVP.Key;
					const TSharedPtr<IDashboardViewFactory>& Factory = KVP.Value;

					MenuBuilder.AddMenuEntry(Factory->GetDisplayName(),
						FText::GetEmpty(),
						FSlateStyle::Get().CreateIcon(Factory->GetIcon().GetStyleName()),
						FUIAction(FExecuteAction::CreateLambda([this, FactoryName]()
						{
							if (DashboardTabManager.IsValid())
							{
								if (TSharedPtr<SDockTab> ViewportTab = DashboardTabManager->FindExistingLiveTab(FactoryName);
									!ViewportTab.IsValid())
								{
									DashboardTabManager->TryInvokeTab(FactoryName);

									if (TSharedPtr<SDockTab> InvokedOutputMeterTab = DashboardTabManager->TryInvokeTab(FactoryName);
										InvokedOutputMeterTab.IsValid() && DashboardViewFactories[FactoryName].IsValid())
									{
										if (const EDefaultDashboardTabStack DefaultTabStack = DashboardViewFactories[FactoryName]->GetDefaultTabStack();
											DefaultTabStack == EDefaultDashboardTabStack::AudioAnalyzerRack)
										{
											InvokedOutputMeterTab->SetParentDockTabStackTabWellHidden(true);
										}
									}
								}
								else
								{
									ViewportTab->RequestCloseTab();
								}
							}
						}),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda([&DashboardTabManager = DashboardTabManager, FactoryName]()
						{
							return DashboardTabManager.IsValid() ? DashboardTabManager->FindExistingLiveTab(FactoryName).IsValid() : false;
						})),
						NAME_None,
						EUserInterfaceActionType::Check
					);

					if (const EDefaultDashboardTabStack DefaultTabStack = DashboardViewFactories[FactoryName]->GetDefaultTabStack();
						DefaultTabStack == EDefaultDashboardTabStack::Log || DefaultTabStack == EDefaultDashboardTabStack::AudioMeters)
					{
						MenuBuilder.AddMenuSeparator();
					}
				}

				MenuBuilder.AddMenuSeparator();

				MenuBuilder.AddMenuEntry(LOCTEXT("ViewMenu_ResetLayoutText", "Reset Layout"), 
					FText::GetEmpty(), 
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([this]()
					{
						if (DashboardTabManager)
						{
							for (const auto& KVP : DashboardViewFactories)
							{
								// Try and get the dashboard tab:
								TSharedPtr<SDockTab> DashboardTab = DashboardTabManager->FindExistingLiveTab(KVP.Key);
								if (!DashboardTab.IsValid())
								{
									DashboardTab = DashboardTabManager->TryInvokeTab(KVP.Key);
								}

								if (DashboardTab.IsValid())
								{
									if (TSharedPtr<FTabManager> SubTabManager = FGlobalTabmanager::Get()->GetTabManagerForMajorTab(DashboardTab))
									{
										// There is a sub tab manager for this dashbaord tab; clear its persisted areas:
										SubTabManager->CloseAllAreas();
										SubTabManager->SavePersistentLayout();
									}
								}
							}

							if (TSharedPtr<SDockTab> OwnerTab = DashboardTabManager->GetOwnerTab())
							{
								// Wipe all the persisted areas and close tab
								DashboardTabManager->CloseAllAreas();
								OwnerTab->RequestCloseTab();

								// Can't invoke the tab immediately (it won't show up), needs to be done a bit later
								AsyncTask(ENamedThreads::GameThread, [AudioInsightsTabId = OwnerTab->GetLayoutIdentifier()]()
								{
									FGlobalTabmanager::Get()->TryInvokeTab(AudioInsightsTabId);
								});
							}
						}
					}),
					FCanExecuteAction()));
			}),
			"View"
		);

		return MenuBarBuilder.MakeWidget();
	}

	TSharedRef<SWidget> FEditorDashboardFactory::MakeMainToolbarWidget()
	{
		using namespace EditorDashboardFactoryPrivate;

		static const FName PlayWorldToolBarName = "Kismet.DebuggingViewToolBar";
		if (!UToolMenus::Get()->IsMenuRegistered(PlayWorldToolBarName))
		{
			UToolMenu* ToolBar = UToolMenus::Get()->RegisterMenu(PlayWorldToolBarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
			FToolMenuSection& Section = ToolBar->AddSection("Debug");
			FPlayWorldCommands::BuildToolbar(Section);
		}

		static FSlateBrush TransportBackgroundColorBrush;
		TransportBackgroundColorBrush.TintColor = FSlateColor(FLinearColor(0.018f, 0.018f, 0.018f, 1.0f));
		TransportBackgroundColorBrush.DrawAs    = ESlateBrushDrawType::Box;

		return SNew(SBorder)
			.BorderImage(&TransportBackgroundColorBrush)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
					[
						UToolMenus::Get()->GenerateWidget(PlayWorldToolBarName, { FPlayWorldCommands::GlobalPlayWorldActions })
					]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(OnlyTraceAudioChannelsName)
					.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SCheckBox)
					.ToolTipText(OnlyTraceAudioChannelsDescription)
					.IsChecked_Lambda([this]() { return bOnlyTraceAudioChannels ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) 
					{ 
						bOnlyTraceAudioChannels = NewState == ECheckBoxState::Checked;

						IAudioInsightsTraceModule& TraceModule = FAudioInsightsEditorModule::GetChecked().GetTraceModule();
						TraceModule.OnOnlyTraceAudioChannelsStateChanged(bOnlyTraceAudioChannels);
					})
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SelectDashboardWorld_DisplayName", "World Filter:"))
					.ToolTipText(DashboardWorldSelectDescription)
					.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SAssignNew(AudioDeviceComboBox, SComboBox<TSharedPtr<::Audio::FDeviceId>>)
					.ToolTipText(DashboardWorldSelectDescription)
					.OptionsSource(&AudioDeviceIds)
					.OnGenerateWidget_Lambda([](const TSharedPtr<::Audio::FDeviceId>& WidgetDeviceId)
					{
						FText NameText = GetDebugNameFromDeviceId(*WidgetDeviceId);
						return SNew(STextBlock)
							.Text(NameText)
							.Font(IPropertyTypeCustomizationUtils::GetRegularFont());
					})
					.OnSelectionChanged_Lambda([this](TSharedPtr<::Audio::FDeviceId> NewDeviceId, ESelectInfo::Type)
					{
						if (NewDeviceId.IsValid())
						{
							ActiveDeviceId = *NewDeviceId;
							RefreshDeviceSelector();

							OnActiveAudioDeviceChanged.Broadcast();
						}
					})
					[
						SNew(STextBlock)
						.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
						.Text_Lambda([this]()
						{
							return GetDebugNameFromDeviceId(ActiveDeviceId);
						})
					]
				]
			];
	}

	void FEditorDashboardFactory::StartTraceAnalysis(const TObjectPtr<const UWorld> InWorld, const ::Audio::FDeviceId InDeviceId)
	{
		if (InWorld && InWorld->IsGameWorld())
		{
			IAudioInsightsTraceModule& TraceModule = FAudioInsightsEditorModule::GetChecked().GetTraceModule();
			TraceModule.StartTraceAnalysis(bOnlyTraceAudioChannels);

			const TObjectPtr<const UAudioInsightsEditorSettings> AudioInsightsEditorSettings = GetDefault<UAudioInsightsEditorSettings>();

			// We don't want to set ActiveDeviceId if bWorldFilterDefaultsToFirstClient is true and more than 2 PIE clients are running
			if (AudioInsightsEditorSettings == nullptr ||
				(AudioInsightsEditorSettings && !AudioInsightsEditorSettings->bWorldFilterDefaultsToFirstClient) ||
				AudioDeviceIds.Num() < 2)
			{
				ActiveDeviceId = InDeviceId;
			}
		}
	}

	void FEditorDashboardFactory::InitDelegates()
	{
		if (!OnWorldRegisteredToAudioDeviceHandle.IsValid())
		{
			OnWorldRegisteredToAudioDeviceHandle = FAudioDeviceWorldDelegates::OnWorldRegisteredToAudioDevice.AddSP(this, &FEditorDashboardFactory::OnWorldRegisteredToAudioDevice);
		}

		if (!OnWorldUnregisteredFromAudioDeviceHandle.IsValid())
		{
			OnWorldUnregisteredFromAudioDeviceHandle = FAudioDeviceWorldDelegates::OnWorldUnregisteredWithAudioDevice.AddSP(this, &FEditorDashboardFactory::OnWorldUnregisteredFromAudioDevice);
		}

		if (!OnDeviceCreatedHandle.IsValid())
		{
			OnDeviceCreatedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceCreated.AddSP(this, &FEditorDashboardFactory::OnDeviceCreated);
		}

		if (!OnDeviceDestroyedHandle.IsValid())
		{
			OnDeviceDestroyedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddSP(this, &FEditorDashboardFactory::OnDeviceDestroyed);
		}

		if (!OnPIEStartedHandle.IsValid())
		{
			OnPIEStartedHandle = FEditorDelegates::PreBeginPIE.AddSP(this, &FEditorDashboardFactory::OnPIEStarted);
		}

		if (!OnPIEStoppedHandle.IsValid())
		{
			OnPIEStoppedHandle = FEditorDelegates::EndPIE.AddSP(this, &FEditorDashboardFactory::OnPIEStopped);
		}
	}

	TSharedRef<FTabManager::FLayout> FEditorDashboardFactory::GetDefaultTabLayout()
	{
		using namespace EditorDashboardFactoryPrivate;

		TSharedRef<FTabManager::FStack> ViewportTabStack = FTabManager::NewStack();
		TSharedRef<FTabManager::FStack> LogTabStack = FTabManager::NewStack();
		TSharedRef<FTabManager::FStack> AnalysisTabStack = FTabManager::NewStack();
		TSharedRef<FTabManager::FStack> AudioMetersTabStack = FTabManager::NewStack();
		TSharedRef<FTabManager::FStack> AudioAnalyzerRackTabStack = FTabManager::NewStack()->SetHideTabWell(true)->SetSizeCoefficient(0.15f);

		for (const auto& KVP : DashboardViewFactories)
		{
			const FName& FactoryName = KVP.Key;
			const TSharedPtr<IDashboardViewFactory>& Factory = KVP.Value;

			const EDefaultDashboardTabStack DefaultTabStack = Factory->GetDefaultTabStack();
			switch (DefaultTabStack)
			{
				case EDefaultDashboardTabStack::Viewport:
				{
					ViewportTabStack->AddTab(FactoryName, ETabState::OpenedTab);
				}
				break;

				case EDefaultDashboardTabStack::Log:
				{
					LogTabStack->AddTab(FactoryName, ETabState::OpenedTab);
				}
				break;

				case EDefaultDashboardTabStack::Analysis:
				{
					AnalysisTabStack->AddTab(FactoryName, ETabState::OpenedTab);
				}
				break;

				case EDefaultDashboardTabStack::AudioMeters:
				{
					AudioMetersTabStack->AddTab(FactoryName, ETabState::OpenedTab);
				}
				break;

				case EDefaultDashboardTabStack::AudioAnalyzerRack:
				{
					AudioAnalyzerRackTabStack->AddTab(FactoryName, ETabState::OpenedTab);
				}
				break;

				default:
					break;
			}
		}

		AnalysisTabStack->SetForegroundTab(FName("Sounds"));

		return FTabManager::NewLayout("AudioDashboard_Editor_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				// Left column
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					// Top
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.25f) // Column width
					->Split
					(
						ViewportTabStack
						->SetSizeCoefficient(0.5f)
					)
					// Bottom
					->Split
					(
						LogTabStack
						->SetSizeCoefficient(0.5f)
					)
				)
				
				// Middle column
				->Split
				(
					// Top
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.6f) // Column width
					->Split
					(
						FTabManager::NewSplitter()
						->SetOrientation(Orient_Horizontal)
						->Split
						(
							AnalysisTabStack
							->SetSizeCoefficient(0.58f)
						)
					)
					// Bottom
					->Split
					(
						AudioMetersTabStack
						->SetSizeCoefficient(0.42f)
					)
				)
				// Right column
				->Split
				(
					AudioAnalyzerRackTabStack
				)
			)
		);
	}

	void FEditorDashboardFactory::RegisterTabSpawners()
	{
		using namespace EditorDashboardFactoryPrivate;

		DashboardWorkspace = DashboardTabManager->AddLocalWorkspaceMenuCategory(ToolName);

		for (const auto& KVP : DashboardViewFactories)
		{
			const FName& FactoryName = KVP.Key;
			const TSharedPtr<IDashboardViewFactory>& Factory = KVP.Value;

			DashboardTabManager->RegisterTabSpawner(FactoryName, FOnSpawnTab::CreateLambda([this, Factory](const FSpawnTabArgs& Args)
			{
				TSharedRef<SDockTab> DockTab = SNew(SDockTab)
					.Clipping(EWidgetClipping::ClipToBounds)
					.Label(Factory->GetDisplayName());

				TSharedRef<SWidget> DashboardView = Factory->MakeWidget(DockTab, Args);
				DockTab->SetContent(DashboardView);

				return DockTab;
			}))
			.SetDisplayName(Factory->GetDisplayName())
			.SetGroup(DashboardWorkspace->AsShared())
			.SetIcon(Factory->GetIcon());
		}
	}

	void FEditorDashboardFactory::UnregisterTabSpawners()
	{
		if (DashboardTabManager.IsValid())
		{
			for (const auto& KVP : DashboardViewFactories)
			{
				const FName& FactoryName = KVP.Key;
				DashboardTabManager->UnregisterTabSpawner(FactoryName);
			}
		}
	}

	void FEditorDashboardFactory::RegisterViewFactory(TSharedRef<IDashboardViewFactory> InFactory)
	{
		if (const FName Name = InFactory->GetName(); 
			ensureAlwaysMsgf(!DashboardViewFactories.Contains(Name), TEXT("Failed to register Audio Insights Dashboard '%s': Dashboard with name already registered"), *Name.ToString()))
		{
			DashboardViewFactories.Add(Name, InFactory);
		}
	}

	void FEditorDashboardFactory::UnregisterViewFactory(FName InName)
	{
		DashboardViewFactories.Remove(InName);
	}

	TSharedRef<FTabManager::FLayout> FEditorDashboardFactory::LoadLayoutFromConfig()
	{
		return FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, GetDefaultTabLayout());
	}

	void FEditorDashboardFactory::SaveLayoutToConfig()
	{
		if (DashboardTabManager.IsValid())
		{
			FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, DashboardTabManager->PersistLayout());
		}
	}
} // namespace UE::Audio::Insights

#undef LOCTEXT_NAMESPACE
