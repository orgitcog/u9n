// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkDeviceModule.h"
#include "DetailsViewArgs.h"
#include "Engine/Engine.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LiveLinkDevice.h"
#include "LiveLinkDeviceStyle.h"
#include "LiveLinkDeviceSubsystem.h"
#include "Logging/StructuredLog.h"
#include "PropertyEditorModule.h"
#include "SPositiveActionButton.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/DeviceTable.h"


DEFINE_LOG_CATEGORY(LogLiveLinkDevice);


const FName FLiveLinkDeviceModule::DevicesTabName = FName("LiveLinkDevices");
const FName FLiveLinkDeviceModule::DeviceDetailsTabName = FName("LiveLinkDeviceDetails");


#define LOCTEXT_NAMESPACE "LiveLinkDevice"


IMPLEMENT_MODULE(FLiveLinkDeviceModule, LiveLinkDevice);


//////////////////////////////////////////////////////////////////////////


void FLiveLinkDeviceModule::StartupModule()
{
	FLiveLinkDeviceStyle::Initialize();

	FGlobalTabmanager::Get()
		->RegisterNomadTabSpawner(DevicesTabName,
			FOnSpawnTab::CreateRaw(this, &FLiveLinkDeviceModule::OnSpawnDevicesTab))
		.SetIcon(FSlateIcon("LiveLinkDeviceStyle", "LiveLinkHub.Devices.Icon"))
		.SetDisplayName(LOCTEXT("DevicesTabDisplayName", "Devices"));

	FGlobalTabmanager::Get()
		->RegisterNomadTabSpawner(DeviceDetailsTabName,
			FOnSpawnTab::CreateRaw(this, &FLiveLinkDeviceModule::OnSpawnDeviceDetailsTab))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"))
		.SetDisplayName(LOCTEXT("DeviceDetailsTabDisplayName", "Device Details"));
}


void FLiveLinkDeviceModule::ShutdownModule()
{
	FLiveLinkDeviceStyle::Shutdown();
}


void FLiveLinkDeviceModule::DeviceSelectionChanged(ULiveLinkDevice* InSelectedDevice)
{
	WeakSelectedDevice = InSelectedDevice;

	if (DetailsView)
	{
		DetailsView->SetObject(InSelectedDevice ? InSelectedDevice->GetDeviceSettings() : nullptr);
	}

	OnDeviceSelectionChangedDelegate.Broadcast(InSelectedDevice);
}


TSharedRef<SWidget> FLiveLinkDeviceModule::OnGenerateAddDeviceMenu()
{
	ULiveLinkDeviceSubsystem* DeviceSubsystem = GEngine->GetEngineSubsystem<ULiveLinkDeviceSubsystem>();
	check(DeviceSubsystem);

	const TSet<TSubclassOf<ULiveLinkDevice>>& DeviceClasses = DeviceSubsystem->GetKnownDeviceClasses();

	const bool bCloseAfterSelection_true = true;
	FMenuBuilder MenuBuilder(bCloseAfterSelection_true, nullptr);
	MenuBuilder.BeginSection("DevicesSection", LOCTEXT("DevicesSectionHeading", "Live Link Devices"));

	for (const TSubclassOf<ULiveLinkDevice>& DeviceClass : DeviceClasses)
	{
		if (DeviceClass->HasAnyClassFlags(CLASS_Abstract | CLASS_NotPlaceable))
		{
			continue;
		}

		FText ToolTip = DeviceClass->GetToolTipText();
		if (ToolTip.IsEmpty())
		{
			ToolTip = FText::FromString(DeviceClass->GetPathName());
		}

		MenuBuilder.AddMenuEntry(
			DeviceClass->GetDisplayNameText(),
			ToolTip,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda(
				[DeviceSubsystem, DeviceClass]
				{
					DeviceSubsystem->CreateDeviceOfClass(DeviceClass);
				}
			)),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}

	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}


TSharedRef<SDockTab> FLiveLinkDeviceModule::OnSpawnDevicesTab(const FSpawnTabArgs& InSpawnTabArgs)
{
	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	DeviceTable = SNew(SLiveLinkDeviceTable, DockTab)
		.OnSelectionChanged_Raw(this, &FLiveLinkDeviceModule::DeviceSelectionChanged);

	DockTab->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 5.f, 0.f, 5.f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FSlateIcon("LiveLinkDeviceStyle", "LiveLinkHub.Devices.Icon").GetIcon())
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.Padding(FMargin(4.0, 2.0))
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					.Text(LOCTEXT("DevicesHeaderText", "Devices"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SPositiveActionButton)
					.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
					.Text(LOCTEXT("AddDevice", "Add Device"))
					.ToolTipText(LOCTEXT("AddDevice_Tooltip", "Add a new Live Link device"))
					.OnGetMenuContent_Raw(this, &FLiveLinkDeviceModule::OnGenerateAddDeviceMenu)
				]
			]
		]
		+ SVerticalBox::Slot()
		[
			DeviceTable.ToSharedRef()
		]
	);

	return DockTab;
}


TSharedRef<SDockTab> FLiveLinkDeviceModule::OnSpawnDeviceDetailsTab(const FSpawnTabArgs& InSpawnTabArgs)
{
	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.bShowKeyablePropertiesOption = false;
	DetailsViewArgs.bShowAnimatedPropertiesOption = false;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	if (ULiveLinkDevice* SelectedDevice = WeakSelectedDevice.Get())
	{
		DetailsView->SetObject(SelectedDevice->GetDeviceSettings());
	}

	DetailsView->OnFinishedChangingProperties().AddLambda(
		[this]
		(const FPropertyChangedEvent& InPropertyChangedEvent)
		{
			ULiveLinkDevice* Device = nullptr;

			const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailsView->GetSelectedObjects();
			if (ensure(SelectedObjects.Num() > 0))
			{
				if (UObject* SelectedObject = SelectedObjects[0].Get(); ensure(SelectedObject))
				{
					Device = SelectedObject->GetTypedOuter<ULiveLinkDevice>();
				}
			}

			if (ensure(Device))
			{
				Device->OnSettingChanged(InPropertyChangedEvent);
			}
		}
	);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			DetailsView.ToSharedRef()
		];
}


#undef LOCTEXT_NAMESPACE
