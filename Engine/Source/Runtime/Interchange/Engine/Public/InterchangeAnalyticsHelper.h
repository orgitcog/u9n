// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
struct FAnalyticsEventAttribute;

namespace UE::Interchange
{
	struct FAnalyticsHelper
	{
	private:
		//Analytics Attributes to share with the Translators. (to be sent when Import is finished)
		TMap<FString, TArray<FAnalyticsEventAttribute>> AnalyticsAttributes;
		FCriticalSection CriticalSection;

	public:
		INTERCHANGEENGINE_API void AppendThreadSafe(const FString& Identifier, const TArray<FAnalyticsEventAttribute>& ToAdd);

		//Make sure it is safe to call this directly.
		INTERCHANGEENGINE_API void Add(const FString& Identifier, const FAnalyticsEventAttribute& Entry);

		//It is suggest to use the AppendThreadSafe, if more than one Entry is added as it will Scope Lock for every entry.
		// (where append will Scope Lock only per append)
		INTERCHANGEENGINE_API void AddThreadSafe(const FString& Identifier, const FAnalyticsEventAttribute& Entry);

		INTERCHANGEENGINE_API void SendAnalytics();
	};
};