#include "AbundanceMonitor.h"

UAbundanceMonitor::UAbundanceMonitor()
{
	AbundanceLevel = 1.0f;
	ScarcityThreshold = 0.2f;
}

void UAbundanceMonitor::Update(const TMap<FString, float>& ResourceLevels)
{
	float TotalResources = 0.0f;
	for (auto const& [Name, Level] : ResourceLevels)
	{
		TotalResources += Level;
	}

	AbundanceLevel = TotalResources / ResourceLevels.Num();
}

EAbundanceState UAbundanceMonitor::GetAbundanceState() const
{
	return AbundanceLevel < ScarcityThreshold ? EAbundanceState::Scarce : EAbundanceState::Abundant;
}
