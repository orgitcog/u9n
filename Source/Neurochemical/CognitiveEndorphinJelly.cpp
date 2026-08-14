#include "CognitiveEndorphinJelly.h"

UCognitiveEndorphinJelly::UCognitiveEndorphinJelly()
{
	DopamineLevel = 0.0f;
	SerotoninLevel = 0.0f;
	SpasmThreshold = 100.0f;
}

void UCognitiveEndorphinJelly::Accumulate(float Amount)
{
	DopamineLevel += Amount;
	SerotoninLevel += Amount * 0.5f; // Serotonin accumulates at a slower rate

	if (DopamineLevel >= SpasmThreshold)
	{
		Release();
	}
}

void UCognitiveEndorphinJelly::Release()
{
	OnSpasm.Broadcast();
	Reset();
}

void UCognitiveEndorphinJelly::Reset()
{
	DopamineLevel = 0.0f;
	SerotoninLevel = 0.0f;
}
