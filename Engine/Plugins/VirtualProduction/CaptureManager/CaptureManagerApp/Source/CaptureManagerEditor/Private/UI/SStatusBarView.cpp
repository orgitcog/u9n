// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStatusBarView.h"

#include "OutputLogCreationParams.h"
#include "OutputLogModule.h"

#include "EditorFontGlyphs.h"
#include "WidgetDrawerConfig.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "CaptureManager.SStatusBarView"

const FName OutputLogId("OutputLog");

class FStatusBarSingleton
{
	TSharedPtr<SWidget> StatusBarOutputLog;
	TArray<TWeakPtr<SWidgetDrawer>> StatusBars;

	TSharedRef<SWidget> OnGetOutputLog()
	{
		if (!StatusBarOutputLog)
		{
			FOutputLogCreationParams Params;
			Params.bCreateDockInLayoutButton = true;
			Params.SettingsMenuCreationFlags = EOutputLogSettingsMenuFlags::SkipClearOnPie
				| EOutputLogSettingsMenuFlags::SkipOpenSourceButton
				| EOutputLogSettingsMenuFlags::SkipEnableWordWrapping; // Checkbox relies on saving an editor config file and does not work correctly
			StatusBarOutputLog = FOutputLogModule::Get().MakeOutputLogWidget(Params);
		}

		return StatusBarOutputLog.ToSharedRef();
	}

	void OnOutputLogOpened(FName StatusBarWithDrawerName)
	{
		// Dismiss all other open drawers - StatusBarOutputLog is shared and shouldn't be in the layout twice
		for (const TWeakPtr<SWidgetDrawer>& WidgetDrawer : StatusBars)
		{
			if (WidgetDrawer.IsValid())
			{
				TSharedPtr<SWidgetDrawer> PinnedDrawer = WidgetDrawer.Pin();
				if (StatusBarWithDrawerName != PinnedDrawer->GetDrawerName() || PinnedDrawer->IsAnyOtherDrawerOpened(OutputLogId))
				{
					PinnedDrawer->CloseDrawerImmediately();
				}
			}
		}

		FOutputLogModule::Get().FocusOutputLogConsoleBox(StatusBarOutputLog.ToSharedRef());
	}

	void OnOutputLogDismissed(const TSharedPtr<SWidget>& NewlyFocusedWidget)
	{}

	void PreShutdownSlate()
	{
		StatusBarOutputLog.Reset();
	}

public:

	void Init(TSharedRef<SWidgetDrawer> WidgetDrawer, FWidgetDrawerConfig& OutputLogDrawer)
	{
		if (!FSlateApplication::Get().OnPreShutdown().IsBoundToObject(this))
		{
			// Destroying StatusBarOutputLog in ~FStatusBarSingleton is too late: it causes a crash
			FSlateApplication::Get().OnPreShutdown().AddRaw(this, &FStatusBarSingleton::PreShutdownSlate);
		}

		const bool bIsDrawerNameUnique = !StatusBars.ContainsByPredicate([&WidgetDrawer](TWeakPtr<SWidgetDrawer> WeakDrawer)
			{
				return ensure(WeakDrawer.IsValid())
					&& WeakDrawer.Pin()->GetDrawerName() == WidgetDrawer->GetDrawerName();
			});
		checkf(bIsDrawerNameUnique, TEXT("Every widget drawer is expected to have an unique ID"));

		StatusBars.Add(MoveTemp(WidgetDrawer));

		OutputLogDrawer.GetDrawerContentDelegate.BindRaw(this, &FStatusBarSingleton::OnGetOutputLog);
		OutputLogDrawer.OnDrawerOpenedDelegate.BindRaw(this, &FStatusBarSingleton::OnOutputLogOpened);
		OutputLogDrawer.OnDrawerDismissedDelegate.BindRaw(this, &FStatusBarSingleton::OnOutputLogDismissed);
	}

	void Remove(TSharedRef<SWidgetDrawer> WidgetDrawer)
	{
		StatusBars.RemoveSingle(WidgetDrawer);
	}

} GStatusBarManager;

SStatusBarView::~SStatusBarView()
{
	GStatusBarManager.Remove(WidgetDrawer.ToSharedRef());
}

void SStatusBarView::UpdateConnectionState(bool InIsConnected)
{
	IsConnected = InIsConnected;
}

void SStatusBarView::Construct(const FArguments& InArgs, FName InStatusBarId)
{
	ChildSlot
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
						.VAlign(VAlign_Center)
						.HeightOverride(FAppStyle::Get().GetFloat("StatusBar.Height"))
						[
							MakeWidgetDrawer(InStatusBarId)
						]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(FMargin(4.f, 0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 3, 0)
					[
						SNew(STextBlock)
							.Font(FAppStyle::Get().GetFontStyle("FontAwesome.8"))
							.ColorAndOpacity(this, &SStatusBarView::GetStatusColor)
							.Text(FEditorFontGlyphs::Circle)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2, 0, 10, 0)
					[
						SNew(STextBlock)
							.Text(this, &SStatusBarView::GetConnectionStateText)
							.ToolTipText(this, &SStatusBarView::GetStatusToolTip)
					]
					
				]
		];
}

TSharedRef<SWidgetDrawer> SStatusBarView::MakeWidgetDrawer(FName StatusBarId)
{
	WidgetDrawer = SNew(SWidgetDrawer, StatusBarId);

	TSharedPtr<SMultiLineEditableTextBox> ConsoleEditBox;
	FSimpleDelegate OnConsoleClosed;
	FSimpleDelegate OnConsoleCommandExecuted;
	const TSharedRef<SWidget> OutputLog =
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.VAlign(VAlign_Center)
		.Padding(FMargin(6.0f, 0.0f))
		[
			SNew(SBox)
				.WidthOverride(350.f)
				[
					FOutputLogModule::Get().MakeConsoleInputBox(ConsoleEditBox, OnConsoleClosed, OnConsoleCommandExecuted)
				]
		];

	FWidgetDrawerConfig OutputLogDrawer(OutputLogId);
	GStatusBarManager.Init(WidgetDrawer.ToSharedRef(), OutputLogDrawer);
	OutputLogDrawer.CustomWidget = OutputLog;

	OutputLogDrawer.ButtonText = LOCTEXT("StatusBar_OutputLogButton", "Output Log");
	OutputLogDrawer.Icon = FAppStyle::Get().GetBrush("Log.TabIcon");
	WidgetDrawer->RegisterDrawer(MoveTemp(OutputLogDrawer));

	return WidgetDrawer.ToSharedRef();
}

FText SStatusBarView::GetConnectionStateText() const
{
	return IsConnected ? LOCTEXT("SStatusBarView_Connected", "Connected") : LOCTEXT("SStatusBarView_Disconnected", "Disconnected");
}

FText SStatusBarView::GetStatusToolTip() const
{
	return IsConnected ? LOCTEXT("SStatusBarView_ConnectedToolTip", "Connected to local UE client") : 
		LOCTEXT("SStatusBarView_DisconnectedToolTip", "Disconnected. Ensure the Capture Manager Editor plugin is enabled in a local UE client");
}

FSlateColor SStatusBarView::GetStatusColor() const
{
	return IsConnected ? FSlateColor(FColor::Green) : FSlateColor(FColor::Red);
}

#undef LOCTEXT_NAMESPACE
