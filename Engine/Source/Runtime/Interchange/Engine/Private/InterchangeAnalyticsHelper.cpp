// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeAnalyticsHelper.h"
#include "AnalyticsEventAttribute.h"
#include "EngineAnalytics.h"

namespace UE::Interchange
{
	void FAnalyticsHelper::AppendThreadSafe(const FString& Identifier, const TArray<FAnalyticsEventAttribute>& ToAdd)
	{
		FScopeLock ScopeLock(&CriticalSection);
		TArray<FAnalyticsEventAttribute>& Analytics = AnalyticsAttributes.FindOrAdd(Identifier);
		Analytics.Append(ToAdd);
	}

	//Make sure it is safe to call this directly.
	void FAnalyticsHelper::Add(const FString& Identifier, const FAnalyticsEventAttribute& Entry)
	{
		TArray<FAnalyticsEventAttribute>& Analytics = AnalyticsAttributes.FindOrAdd(Identifier);
		Analytics.Add(Entry);
	}

	void FAnalyticsHelper::AddThreadSafe(const FString& Identifier, const FAnalyticsEventAttribute& Entry)
	{
		FScopeLock ScopeLock(&CriticalSection);
		TArray<FAnalyticsEventAttribute>& Analytics = AnalyticsAttributes.FindOrAdd(Identifier);
		Analytics.Add(Entry);
	}

	void FAnalyticsHelper::SendAnalytics()
	{
		if (FEngineAnalytics::IsAvailable())
		{
			for (const TPair<FString, TArray<FAnalyticsEventAttribute>>& AnalyticsPerIdentifier : AnalyticsAttributes)
			{
				FEngineAnalytics::GetProvider().RecordEvent(AnalyticsPerIdentifier.Key, AnalyticsPerIdentifier.Value);
			}
		}
	}
};