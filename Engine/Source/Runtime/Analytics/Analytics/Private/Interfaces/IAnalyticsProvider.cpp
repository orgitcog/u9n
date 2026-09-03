// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/IAnalyticsProvider.h"
#include "Analytics.h"

void IAnalyticsProvider::CheckForDuplicateAttributes(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	// Expose events that have duplicate attribute names. This is is not handled by the analytics backends in any reliable way.
	for (int32 index0 = 0; index0 < Attributes.Num(); ++index0)
	{
		for (int32 index1 = index0 + 1; index1 < Attributes.Num(); ++index1)
		{
			if (Attributes[index0].GetName() == Attributes[index1].GetName())
			{
				UE_LOG(LogAnalytics, Warning, TEXT("Duplicate Attributes Found For Event %s %s==%s"), *EventName, *Attributes[index0].GetName(), *Attributes[index1].GetName());
			}
		}
	}
#endif
}
