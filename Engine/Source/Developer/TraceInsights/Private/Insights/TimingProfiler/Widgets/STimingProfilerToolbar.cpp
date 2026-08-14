// Copyright Epic Games, Inc. All Rights Reserved.

#include "STimingProfilerToolbar.h"

#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/SBoxPanel.h"

// TraceInsights
#include "Insights/InsightsCommands.h"
#include "Insights/InsightsManager.h"
#include "Insights/InsightsStyle.h"
#include "Insights/TimingProfiler/TimingProfilerCommands.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "UE::Insights::TimingProfiler"

namespace UE::Insights::TimingProfiler
{

////////////////////////////////////////////////////////////////////////////////////////////////////

STimingProfilerToolbar::STimingProfilerToolbar()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

STimingProfilerToolbar::~STimingProfilerToolbar()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimingProfilerToolbar::Construct(const FArguments& InArgs, const FInsightsMajorTabConfig& Config)
{
	struct Local
	{
		static void FillViewToolbar(FToolBarBuilder& ToolbarBuilder, const FArguments &InArgs, const FInsightsMajorTabConfig& Config)
		{
			ToolbarBuilder.BeginSection("View");
			{
				if (Config.ShouldRegisterMinorTab(FTimingProfilerTabs::FramesTrackID))
				{
					ToolbarBuilder.AddToolBarButton(FTimingProfilerCommands::Get().ToggleFramesTrackVisibility,
						NAME_None, TAttribute<FText>(), TAttribute<FText>(),
						FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.FramesTrack.ToolBar"));
				}

				if (Config.ShouldRegisterMinorTab(FTimingProfilerTabs::TimingViewID))
				{
					ToolbarBuilder.AddToolBarButton(FTimingProfilerCommands::Get().ToggleTimingViewVisibility,
						NAME_None, TAttribute<FText>(), TAttribute<FText>(),
						FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.TimingView.ToolBar"));
				}

				if (Config.ShouldRegisterMinorTab(FTimingProfilerTabs::TimersID))
				{
					ToolbarBuilder.AddToolBarButton(FTimingProfilerCommands::Get().ToggleTimersViewVisibility,
						NAME_None, TAttribute<FText>(), TAttribute<FText>(),
						FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.TimersView.ToolBar"));
				}

				if (Config.ShouldRegisterMinorTab(FTimingProfilerTabs::CallersID))
				{
					ToolbarBuilder.AddToolBarButton(FTimingProfilerCommands::Get().ToggleCallersTreeViewVisibility,
						NAME_None, TAttribute<FText>(), TAttribute<FText>(),
						FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.CallersView.ToolBar"));
				}

				if (Config.ShouldRegisterMinorTab(FTimingProfilerTabs::CalleesID))
				{
					ToolbarBuilder.AddToolBarButton(FTimingProfilerCommands::Get().ToggleCalleesTreeViewVisibility,
						NAME_None, TAttribute<FText>(), TAttribute<FText>(),
						FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.CalleesView.ToolBar"));
				}
				if (Config.ShouldRegisterMinorTab(FTimingProfilerTabs::StatsCountersID))
				{
					ToolbarBuilder.AddToolBarButton(FTimingProfilerCommands::Get().ToggleStatsCountersViewVisibility,
						NAME_None, TAttribute<FText>(), TAttribute<FText>(),
						FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.CountersView.ToolBar"));
				}
				if (Config.ShouldRegisterMinorTab(FTimingProfilerTabs::LogViewID))
				{
					ToolbarBuilder.AddToolBarButton(FTimingProfilerCommands::Get().ToggleLogViewVisibility,
						NAME_None, TAttribute<FText>(), TAttribute<FText>(),
						FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.LogView.ToolBar"));
				}
			}
			ToolbarBuilder.EndSection();

			if (InArgs._ToolbarExtender.IsValid())
			{
				InArgs._ToolbarExtender->Apply("MainToolbar", EExtensionHook::First, ToolbarBuilder);
			}
		}

		static void FillRightSideToolbar(FToolBarBuilder& ToolbarBuilder, const FArguments &InArgs)
		{
			ToolbarBuilder.BeginSection("Debug");
			{
				ToolbarBuilder.AddToolBarButton(FInsightsCommands::Get().ToggleDebugInfo,
					NAME_None, FText(), TAttribute<FText>(),
					FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.Debug.ToolBar"));
			}
			ToolbarBuilder.EndSection();

			if (InArgs._ToolbarExtender.IsValid())
			{
				InArgs._ToolbarExtender->Apply("RightSideToolbar", EExtensionHook::First, ToolbarBuilder);
			}
		}
	};

	TSharedPtr<FUICommandList> CommandList = FInsightsManager::Get()->GetCommandList();

	FSlimHorizontalToolBarBuilder ToolbarBuilder(CommandList.ToSharedRef(), FMultiBoxCustomization::None);
	ToolbarBuilder.SetStyle(&FInsightsStyle::Get(), "PrimaryToolbar");
	Local::FillViewToolbar(ToolbarBuilder, InArgs, Config);

	FSlimHorizontalToolBarBuilder RightSideToolbarBuilder(CommandList.ToSharedRef(), FMultiBoxCustomization::None);
	RightSideToolbarBuilder.SetStyle(&FInsightsStyle::Get(), "PrimaryToolbar");
	Local::FillRightSideToolbar(RightSideToolbarBuilder, InArgs);

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.FillWidth(1.0)
		.Padding(0.0f)
		[
			ToolbarBuilder.MakeWidget()
		]

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0.0f)
		[
			RightSideToolbarBuilder.MakeWidget()
		]
	];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace UE::Insights::TimingProfiler

#undef LOCTEXT_NAMESPACE
