// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubApplicationMode.h"

#include "LiveLinkHubApplicationBase.h"
#include "UI/Widgets/SLiveLinkHubSettings.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "ToolMenus.h"
#include "UI/Widgets/SLiveLinkHubTopologyModeSwitcher.h"

const FName ILiveLinkHubApplicationModeFactory::ModularFeatureName = "LiveLinkHubApplicationModeFactory";
const FName FLiveLinkHubApplicationMode::FileMenuExtensionPoint = "LiveLinkHubFileMenu";


TSharedRef<class FLiveLinkHubApplicationMode> ILiveLinkHubApplicationModeFactory::CreateApplicationMode(TSharedPtr<FWorkflowCentricApplication> InApp)
{
	// Using MakeShareable here in order to call the private FLiveLinkHubApplicationMode constructor which is available to use since this class is a friend of it.
	return MakeShareable(new FLiveLinkHubApplicationMode());
};


FLiveLinkHubApplicationMode::FLiveLinkHubApplicationMode(FName InApplicationMode, FText InDisplayName, TSharedPtr<FLiveLinkHubApplicationBase> InApp)
	: FApplicationMode(InApplicationMode)
{
	LayoutIni = TEXT("LiveLinkHubLayout");
	WeakApp = InApp;
	DisplayName = InDisplayName;

	WorkspaceMenuCategory = FWorkspaceItem::NewGroup(InDisplayName);
}

TArray<TSharedRef<SWidget>> FLiveLinkHubApplicationMode::GetToolbarWidgets()
{
	TArray<TSharedRef<SWidget>> Widgets = GetToolbarWidgets_Impl();

	// LiveLinkHub Settings should always be last.
	Widgets.Add(SNew(SLiveLinkHubSettings));

	return Widgets;
}

void FLiveLinkHubApplicationMode::PostActivateMode()
{
	FToolMenuOwnerScoped OwnerScoped(GetModeName());


	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LiveLinkHub.ToolBar"));

	FToolMenuSection& GlobalWidgetsSection = Menu->AddSection(TEXT("GlobalWidgets"), FText::GetEmpty(), FToolMenuInsert(NAME_None, EToolMenuInsertType::First));

	TSharedRef<SWidget> GlobalWidget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0, 0.0)
		.VAlign(VAlign_Center)
		[
			SNew(SLiveLinkHubTopologyModeSwitcher)
		];

	GlobalWidgetsSection.AddEntry(
		FToolMenuEntry::InitWidget(TEXT("TestWidget"), GlobalWidget, FText::GetEmpty(), true, false)
	);
	GlobalWidgetsSection.AddSeparator(NAME_None);

	FToolMenuSection& SessionConfigSection = Menu->AddSection(TEXT("ModeWidgets"), FText::GetEmpty(), FToolMenuInsert(TEXT("GlobalWidgets"), EToolMenuInsertType::After));

	TSharedRef<SHorizontalBox> ModeWidgets = SNew(SHorizontalBox);

	TArray<TSharedRef<SWidget>> Widgets = GetStatusBarWidgets_Impl();
	for (const TSharedRef<SWidget>& Widget : Widgets)
	{
		ModeWidgets->AddSlot()
			.AutoWidth()
			.Padding(FMargin(0.0, 0.0, 0.0, 1.0))
			.VAlign(VAlign_Center)
			[
				Widget
			];
	}

	SessionConfigSection.AddEntry(
		FToolMenuEntry::InitWidget(TEXT("ModeWidget"), ModeWidgets, FText::GetEmpty(), true, false)
	);
}

void FLiveLinkHubApplicationMode::PreDeactivateMode()
{
	UToolMenus::Get()->UnregisterOwner(GetModeName());
}

