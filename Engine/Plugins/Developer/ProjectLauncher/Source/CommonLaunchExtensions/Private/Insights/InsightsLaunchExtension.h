// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Extension/LaunchExtension.h"



class FInsightsLaunchExtensionInstance : public ProjectLauncher::FLaunchExtensionInstance
{
public:
	FInsightsLaunchExtensionInstance( FArgs& InArgs ) : FLaunchExtensionInstance(InArgs) {};
	virtual ~FInsightsLaunchExtensionInstance() = default;

	virtual bool GetExtensionParameters( TArray<FString>& OutParameters ) const override;
	virtual FText GetExtensionParameterDisplayName( const FString& InParameter ) const override;
	
	virtual void CustomizeParametersSubmenu( FMenuBuilder& MenuBuilder ) override;

	void CacheTraceChannels();
	void ToggleTraceChannel( const FString& InChannel );
	bool IsTraceChannelEnabled( const FString InChannel ) const;

private:
	TArray<FString> TraceChannels;

	static const TCHAR* FileParam;
	static const TCHAR* HostParam;
	static const TCHAR* TraceParam;
	static const TCHAR* StatNamedEventsParam;

};


class FInsightsLaunchExtension : public ProjectLauncher::FLaunchExtension
{
public:
	virtual TSharedPtr<ProjectLauncher::FLaunchExtensionInstance> CreateInstanceForProfile( ProjectLauncher::FLaunchExtensionInstance::FArgs& InArgs ) override;
	virtual const TCHAR* GetInternalName() const override;
	virtual FText GetDisplayName() const override;
};