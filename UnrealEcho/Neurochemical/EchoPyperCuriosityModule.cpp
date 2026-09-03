#include "EchoPyperCuriosityModule.h"

UEchoPyperCuriosityModule::UEchoPyperCuriosityModule()
{
	TriggerProbability = 0.1f;
	ChaosFactor = 0.0f;
	TimeSinceLastTrigger = 0.0f;
}

void UEchoPyperCuriosityModule::Update(float DeltaTime)
{
	TimeSinceLastTrigger += DeltaTime;

	float CurrentTriggerProbability = TriggerProbability + ChaosFactor * FMath::FRand();

	if (FMath::FRand() < CurrentTriggerProbability * TimeSinceLastTrigger)
	{
		OnTrigger.Broadcast();
		TimeSinceLastTrigger = 0.0f;
	}
}
