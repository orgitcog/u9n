#include "ScarcityDetector.h"

UScarcityDetector::UScarcityDetector()
{
}

void UScarcityDetector::CheckForScarcity(const UResourceTracker* ResourceTracker)
{
	if (!ResourceTracker) return;

	for (auto const& [Name, Threshold] : ScarcityThresholds)
	{
		if (ResourceTracker->GetResourceLevel(Name) < Threshold)
		{
			OnScarcityDetected.Broadcast(Name);
		}
	}
}
