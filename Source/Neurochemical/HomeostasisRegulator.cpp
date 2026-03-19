#include "HomeostasisRegulator.h"
#include "AbundanceMonitor.h"

UHomeostasisRegulator::UHomeostasisRegulator()
{
	DampeningFactor = 0.5f;
}

void UHomeostasisRegulator::Regulate(UEchoPyperCuriosityModule* CuriosityModule, EAbundanceState AbundanceState)
{
	if (!CuriosityModule) return;

	if (AbundanceState == EAbundanceState::Abundant)
	{
		CuriosityModule->TriggerProbability *= (1.0f - DampeningFactor);
	}
	else
	{
		CuriosityModule->TriggerProbability *= (1.0f + DampeningFactor);
	}
}

void UHomeostasisRegulator::RespondToScarcity(const FString& ResourceName)
{
	UE_LOG(LogTemp, Warning, TEXT("Scarcity detected for resource: %s. Initiating response."), *ResourceName);
	// Here you would implement the logic to motivate the agent to seek the scarce resource.
}
