// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubWindowController.h"

#include "Clients/LiveLinkHubProvider.h"
#include "CoreGlobals.h"
#include "Features/IModularFeatures.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/Notifications/NotificationManager.h"
#include "LiveLinkHub.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "UI/Widgets/SLiveLinkHubMemoryStats.h"
#include "UI/Widgets/SLiveLinkHubTopologyModeSwitcher.h"
#include "UI/Window/ModalWindowManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "LiveLinkHubWindowController"

FLiveLinkHubWindowController::FLiveLinkHubWindowController()
{
	ModalWindowManager = InitializeSlateApplication();
}

FLiveLinkHubWindowController::~FLiveLinkHubWindowController()
{
	if (RootWindow)
	{
		RootWindow->SetOnWindowClosed(nullptr);
	}
}

TSharedRef<SWindow> FLiveLinkHubWindowController::CreateWindow()
{
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(DisplayMetrics.PrimaryDisplayWorkAreaRect.Left, DisplayMetrics.PrimaryDisplayWorkAreaRect.Top);

	constexpr bool bEmbedTitleAreaContent = true;
	const FVector2D ClientSize(1200.0f * DPIScaleFactor, 800.0f * DPIScaleFactor);
	TSharedRef<SWindow> RootWindowRef = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Live Link Hub"))
		.CreateTitleBar(!bEmbedTitleAreaContent)
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.IsInitiallyMaximized(false)
		.IsInitiallyMinimized(false)
		.SizingRule(ESizingRule::UserSized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.ClientSize(ClientSize)
		.AdjustInitialSizeAndPositionForDPIScale(false);

	RootWindow = RootWindowRef;
		
	constexpr bool bShowRootWindowImmediately = false;
	FSlateApplication::Get().AddWindow(RootWindowRef, bShowRootWindowImmediately);
	FGlobalTabmanager::Get()->SetRootWindow(RootWindowRef);
	FGlobalTabmanager::Get()->SetAllowWindowMenuBar(true);
	FSlateNotificationManager::Get().SetRootWindow(RootWindowRef);	

	RootWindow->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &FLiveLinkHubWindowController::OnWindowClosed));

	return RootWindowRef;
}
void FLiveLinkHubWindowController::RestoreLayout(TSharedPtr<FAssetEditorToolkit> AssetEditorToolkit)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FLiveLinkHubWindowController::RestoreLayout);

	static const FTabId StandaloneTabId("StandaloneToolkit");

	const FName LayoutName = TEXT("LiveLinkHub_v1.1");
	const TSharedRef<FTabManager::FLayout> DefaultLayout =
		FTabManager::NewLayout(LayoutName)
		->AddArea
		(
			// Toolkits window
			FTabManager::NewPrimaryArea()
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(1.0f)
				->AddTab(StandaloneTabId, ETabState::ClosedTab)
			)
		);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FLiveLinkHubWindowController::RestoreFrom);
		constexpr bool bEmbedTitleAreaContent = true;
		const TSharedPtr<SWidget> Content = FGlobalTabmanager::Get()->RestoreFrom(DefaultLayout, RootWindow, bEmbedTitleAreaContent, EOutputCanBeNullptr::Never);
		RootWindow->SetContent(Content.ToSharedRef());
	}

	RootWindow->ShowWindow();
	constexpr bool bForceWindowToFront = true;
	RootWindow->BringToFront(bForceWindowToFront);

	// Pass a dummy object to the asset editor since we're not actually editing an object.
	UObject* DummyObject = GetMutableDefault<UObject>();
	AssetEditorToolkit->InitAssetEditor(EToolkitMode::Standalone, nullptr, "LiveLinkHub", DefaultLayout, /*bCreateDefaultStandaloneMenu*/true, /*bCreateDefaultToolbar*/true, DummyObject, /*bInIsToolbarFocusable*/true, true, {});

	FGlobalTabmanager::Get()->SetMainTab(StandaloneTabId);

	TSharedRef<SWidget> ProviderNameWidget =
		SNew(SBox)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
				.Text(FText::FromString(FLiveLinkHub::Get()->GetLiveLinkProvider()->GetProviderName()))
		];

	TSharedRef<SWidget> RightMenuWidget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			ProviderNameWidget
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f)
		[
			SNew(SLiveLinkHubMemoryStats)
		];

	TSharedPtr<SDockTab> MainTab = FGlobalTabmanager::Get()->FindExistingLiveTab(StandaloneTabId);
	MainTab->SetTitleBarRightContent(RightMenuWidget);
	MainTab->SetTabIcon(FSlateIcon("LiveLinkStyle", "LiveLinkHub.Icon.Small").GetIcon());
}

TSharedPtr<FModalWindowManager> FLiveLinkHubWindowController::InitializeSlateApplication()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FLiveLinkHubWindowController::InitializeSlateApplication);

	const FText ApplicationTitle = LOCTEXT("AppTitle", "Live Link Hub");
	FGlobalTabmanager::Get()->SetApplicationTitle(ApplicationTitle);

	return MakeShared<FModalWindowManager>(CreateWindow());
}

void FLiveLinkHubWindowController::OnWindowClosed(const TSharedRef<SWindow>& Window)
{
	if (TSharedPtr<FLiveLinkHub> LiveLinkHub = FLiveLinkHub::Get())
	{
		FGlobalTabmanager::Get()->SaveAllVisualState();
		LiveLinkHub->CloseWindow(EAssetEditorCloseReason::CloseAllAssetEditors);
	}

	RootWindow.Reset();

	RequestEngineExit(TEXT("FLiveLinkHubWindowController::OnWindowClosed"));
}


#undef LOCTEXT_NAMESPACE 
