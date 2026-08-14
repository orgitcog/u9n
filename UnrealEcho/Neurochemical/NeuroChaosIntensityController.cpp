#include "NeuroChaosIntensityController.h"

UNeuroChaosIntensityController::UNeuroChaosIntensityController()
{
	ChaosLevel = 0.0f;
}

void UNeuroChaosIntensityController::SetChaosLevel(float Level)
{
	ChaosLevel = FMath::Clamp(Level, 0.0f, 1.0f);
}
