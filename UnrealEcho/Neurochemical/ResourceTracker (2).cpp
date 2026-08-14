#include "ResourceTracker.h"

UResourceTracker::UResourceTracker()
{
}

void UResourceTracker::UpdateResource(const FString& Name, float Level)
{
	ResourceLevels.FindOrAdd(Name) = FMath::Clamp(Level, 0.0f, 1.0f);
}

float UResourceTracker::GetResourceLevel(const FString& Name) const
{
	if (ResourceLevels.Contains(Name))
	{
		return ResourceLevels[Name];
	}
	return 0.0f;
}

const TMap<FString, float>& UResourceTracker::GetResourceLevels() const
{
	return ResourceLevels;
}
