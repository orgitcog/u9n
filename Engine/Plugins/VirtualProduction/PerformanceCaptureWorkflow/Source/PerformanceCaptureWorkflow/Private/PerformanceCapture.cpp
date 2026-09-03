// Copyright Epic Games, Inc. All Rights Reserved.

#include "PerformanceCapture.h"

#include "EditorUtilityWidgetBlueprint.h"
#include "PerformanceCaptureStyle.h"
#include "PerformanceCaptureCommands.h"
#include "ISettingsModule.h"
#include "LevelEditorOutlinerSettings.h"
#include "PCapAssetDefinition.h"
#include "PCapDatabase.h"
#include "PCapSettings.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "Visualizers/LiveLinkDataPreview.h"

static const FName PerformanceCapturePanelTabName("PerformanceCaptureTab");

#define LOCTEXT_NAMESPACE "FPerformanceCaptureModule"

DEFINE_LOG_CATEGORY(LogPCap);

void FPerformanceCaptureModule::StartupModule()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "PerformanceCapture", LOCTEXT("RuntimeSettingsName", "Performance Capture"), LOCTEXT("RuntimeSettingsDescription", "Performance Capture"), GetMutableDefault<UPerformanceCaptureSettings>());
	}

	FPerformanceCaptureStyle::Initialize();
	FPerformanceCaptureStyle::ReloadTextures();

	FPerformanceCaptureCommands::Register();
	
	PluginCommands = MakeShared<FUICommandList>();

	PluginCommands->MapAction(
		FPerformanceCaptureCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FPerformanceCaptureModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPerformanceCaptureModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PerformanceCapturePanelTabName, FOnSpawnTab::CreateRaw(this, &FPerformanceCaptureModule::OnSpawnMocapManager))
		.SetDisplayName(NSLOCTEXT("PerformanceCapture", "MocapManagerTabTitle", "Mocap Manager"))
		.SetTooltipText(NSLOCTEXT("PerformanceCapture", "PerformanceCaptureTooltipText", "Open the Mocap Manager tab"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FPerformanceCaptureStyle::GetStyleSetName(), "PerformanceCapture.MocapManagerTabIcon", "PerformanceCapture.MocapManagerTabIcon.Small")
		);
	RegisterPlacementModeItems();
}

void FPerformanceCaptureModule::ShutdownModule()
{
	//Clean up settings
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "PerformanceCapture");
	}

	if (!IsEngineExitRequested() && GEditor && UObjectInitialized())
	{
		UnregisterPlacementModeItems();
	}
	//Clean up nomad tab spawner
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PerformanceCapturePanelTabName);
	

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FPerformanceCaptureStyle::Shutdown();

	FPerformanceCaptureCommands::Unregister();
	
}

TSharedRef<SDockTab> FPerformanceCaptureModule::OnSpawnMocapManager(const FSpawnTabArgs& SpawnTabArgs)
{
	const UPerformanceCaptureSettings* Settings = GetDefault<UPerformanceCaptureSettings>();
	
	if(Settings->MocapManagerUI.IsValid())
	{
		UEditorUtilityWidgetBlueprint* MocapManagerEW = LoadObject<UEditorUtilityWidgetBlueprint>(NULL, *Settings->MocapManagerUI.ToString(), NULL, LOAD_None, NULL);
	
		TSharedRef<SDockTab> TabWidget = MocapManagerEW->SpawnEditorUITab(SpawnTabArgs);
		
			return TabWidget;
	}
	
	{
	//Define message
	FText WidgetText = FText::Format(
	LOCTEXT("WindowWidgetText", "Performance Capture Project settings missing a valid UI Widget"),
	FText::FromString(TEXT("FPerformanceCaptureModule::OnSpawnMocapManager")),
	FText::FromString(TEXT("PerformanceCapture.cpp"))
	);
		//Create default tab message
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				// Put your tab content here!
		
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(WidgetText)
				]
			];
	}
}

void FPerformanceCaptureModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PerformanceCapturePanelTabName);
}

void FPerformanceCaptureModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window.VirtualProduction");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("VirtualProduction");
			Section.AddMenuEntryWithCommandList(FPerformanceCaptureCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FPerformanceCaptureCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

void FPerformanceCaptureModule::UnregisterPlacementModeItems()
{
	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

	for (TOptional<FPlacementModeID>& PlaceActor : PlaceActors)
	{
		if (PlaceActor.IsSet())
		{
			PlacementModeModule.UnregisterPlaceableItem(*PlaceActor);
		}
	}

	PlaceActors.Empty();
}

const FPlacementCategoryInfo* FPerformanceCaptureModule::GetVirtualProductionCategoryRegisteredInfo() const
{
	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

	if (const FPlacementCategoryInfo* RegisteredInfo = PlacementModeModule.GetRegisteredPlacementCategory(FLevelEditorOutlinerBuiltInCategories::VirtualProduction()))
	{
		return RegisteredInfo;
	}
	else
	{
		FPlacementCategoryInfo Info(
			LOCTEXT("VirtualProductionCategoryName", "Virtual Production"),
			FSlateIcon(FPerformanceCaptureStyle::Get().GetStyleSetName(), "PlacementBrowser.Icons.VirtualProduction"),
			FLevelEditorOutlinerBuiltInCategories::VirtualProduction(),
			TEXT("PMVirtualProduction"),
			25 // Determines where the category shows up in the list with respect to the others.
		);
		Info.ShortDisplayName = LOCTEXT("VirtualProductionShortCategoryName", "VP");
		IPlacementModeModule::Get().RegisterPlacementCategory(Info);

		return PlacementModeModule.GetRegisteredPlacementCategory(FLevelEditorOutlinerBuiltInCategories::VirtualProduction());
	}
}

void FPerformanceCaptureModule::RegisterPlacementModeItems()
{
	auto RegisterPlaceActors = [&]() -> void
	{
		if (!GEditor)
		{
			return;
		}

		const FPlacementCategoryInfo* Info = GetVirtualProductionCategoryRegisteredInfo();

		if (!Info)
		{
			UE_LOG(LogPCap, Warning, TEXT("Could not find or create VirtualProduction Place Actor Category"));
			return;
		}

		/*Register the Live Link Data Prevew Actor*/
		PlaceActors.Add(IPlacementModeModule::Get().RegisterPlaceableItem(Info->UniqueHandle, MakeShared<FPlaceableItem>(
			*ALiveLinkDataPreview::StaticClass(),
			FAssetData(ALiveLinkDataPreview::StaticClass()),
			NAME_None,
			NAME_None,
			TOptional<FLinearColor>(),
			TOptional<int32>(),
			NSLOCTEXT("PlacementMode", "LiveLinkDataPreview", "Live Link Data Preview")
		)));
	};

	if (FApp::CanEverRender())
	{
		if (GEngine && GEngine->IsInitialized())
		{
			RegisterPlaceActors();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda(RegisterPlaceActors);
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPerformanceCaptureModule, PerformanceCaptureWorkflow)

