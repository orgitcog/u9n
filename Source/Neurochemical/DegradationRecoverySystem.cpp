#include "DegradationRecoverySystem.h"

UDegradationRecoverySystem::UDegradationRecoverySystem()
{
	CurrentState = ESystemState::Stable;
	StabilityThreshold = 0.8f;
}

void UDegradationRecoverySystem::Monitor(float ChaosLevel)
{
	if (ChaosLevel > StabilityThreshold)
	{
		CurrentState = ESystemState::Unstable;
	}

	if (CurrentState == ESystemState::Unstable)
	{
		// Add a chance to automatically recover
		if (FMath::FRand() < 0.1f)
		{
			Recover();
		}
	}
}

void UDegradationRecoverySystem::Recover()
{
	CurrentState = ESystemState::Recovering;
	OnRecovery.Broadcast();
	CurrentState = ESystemState::Stable;
}
