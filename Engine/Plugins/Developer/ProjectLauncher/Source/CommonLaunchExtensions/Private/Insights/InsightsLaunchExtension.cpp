// Copyright Epic Games, Inc. All Rights Reserved.

#include "Insights/InsightsLaunchExtension.h"
#include "Trace/Trace.h"


#define LOCTEXT_NAMESPACE "FInsightsLaunchExtensionInstance"

const TCHAR* FInsightsLaunchExtensionInstance::FileParam = TEXT("-tracefile");
const TCHAR* FInsightsLaunchExtensionInstance::HostParam = TEXT("-tracehost=$(LocalHost)");
const TCHAR* FInsightsLaunchExtensionInstance::TraceParam = TEXT("-trace=");
const TCHAR* FInsightsLaunchExtensionInstance::StatNamedEventsParam = TEXT("-statnamedevents");



bool FInsightsLaunchExtensionInstance::GetExtensionParameters( TArray<FString>& OutParameters ) const
{
	OutParameters.Add(HostParam);
	OutParameters.Add(FileParam);
	OutParameters.Add(StatNamedEventsParam);

	return true;
}

FText FInsightsLaunchExtensionInstance::GetExtensionParameterDisplayName( const FString& InParameter ) const
{
	if (InParameter == HostParam)
	{
		return LOCTEXT("TraceHostLabel", "Trace to a computer");
	}
	else if (InParameter == FileParam)
	{
		return LOCTEXT("TraceFileLabel", "Trace to a file");
	}
	else if (InParameter == StatNamedEventsParam)
	{
		return LOCTEXT("TraceNamedEventsParam", "Capture named events");
	}

	return FLaunchExtensionInstance::GetExtensionParameterDisplayName(InParameter);

}


void FInsightsLaunchExtensionInstance::CacheTraceChannels()
{
	TraceChannels.Reset();

	FString TraceParamValue = GetParameterValue(TraceParam);
	TraceParamValue.ParseIntoArray(TraceChannels, TEXT(",") );

	TraceChannels.Sort();
}

void FInsightsLaunchExtensionInstance::ToggleTraceChannel( const FString& InChannel )
{
	if (TraceChannels.Contains(InChannel))
	{
		TraceChannels.Remove(InChannel);
	}
	else
	{
		TraceChannels.Add(InChannel);
	}

	if (TraceChannels.IsEmpty())
	{
		RemoveParameter(TraceParam);
	}
	else
	{
		FString TraceParamValue = FString::Join( TraceChannels, TEXT(",") );
		UpdateParameterValue( TraceParam, TraceParamValue );
	}
}

bool FInsightsLaunchExtensionInstance::IsTraceChannelEnabled( const FString InChannel ) const
{
	return TraceChannels.Contains(InChannel);
}


void FInsightsLaunchExtensionInstance::CustomizeParametersSubmenu( FMenuBuilder& MenuBuilder )
{
#if UE_TRACE_ENABLED || UE_TRACE_MINIMAL_ENABLED
	auto ChannelsMenuDelegate = [this]( FMenuBuilder& MenuBuilder )
	{
		CacheTraceChannels();

		// collect all trace channels
		// note that this will enumerate the channels that are available for the editor / UnrealFrontend, not the ones available to the game
		// UnrealFrontend does not have all channels available
		TArray<FString> AllTraceChannels;
		UE::Trace::EnumerateChannels([](const UE::Trace::FChannelInfo& ChannelInfo, void* User)
		{
			TArray<FString>& AllTraceChannels = *static_cast<TArray<FString>*>(User);
			if (!ChannelInfo.bIsReadOnly)
			{
				FString Channel = FString(ChannelInfo.Name).LeftChop(7); // Remove "Channel" suffix
				AllTraceChannels.Add(Channel);
			}
			return true;
		}, &AllTraceChannels );
		AllTraceChannels.Sort();

		// add menu item to clear the selected channels
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ClearTraceChannelsLabel", "None"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction( 
				FExecuteAction::CreateLambda( [this]() { RemoveParameter(TraceParam); TraceChannels.Reset(); } ),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda( [this]() { return (TraceChannels.IsEmpty()); } )
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton);
		MenuBuilder.AddMenuSeparator();

		// add submenu items for each channel
		for ( const FString& Channel : AllTraceChannels )
		{
			MenuBuilder.AddMenuEntry(
				FText::FromString(Channel),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda( [this, Channel]() { ToggleTraceChannel(Channel); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda( [this, Channel]() { return IsTraceChannelEnabled(Channel); } )
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton);
		}
	};



	MenuBuilder.AddSubMenu(
		LOCTEXT("TraceChannelLabels", "Select Channels"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateLambda(ChannelsMenuDelegate),
		true, // bInOpenSubMenuOnClick
		FSlateIcon(),
		false // bInShouldCloseWindowAfterMenuSelection
	);
#endif
}






TSharedPtr<ProjectLauncher::FLaunchExtensionInstance> FInsightsLaunchExtension::CreateInstanceForProfile( ProjectLauncher::FLaunchExtensionInstance::FArgs& InArgs )
{
	return MakeShared<FInsightsLaunchExtensionInstance>(InArgs);
}

const TCHAR* FInsightsLaunchExtension::GetInternalName() const
{
	return TEXT("Insights");
}

FText FInsightsLaunchExtension::GetDisplayName() const
{
	return LOCTEXT("ExtensionName", "Unreal Insights");
}


#undef LOCTEXT_NAMESPACE
