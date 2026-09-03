#include "EchoPulseResonanceChamber.h"

UEchoPulseResonanceChamber::UEchoPulseResonanceChamber()
{
	ResonanceLevel = 0.0f;
	IntensityFactor = 1.0f;
}

void UEchoPulseResonanceChamber::UpdateResonance(float Signal)
{
	ResonanceLevel += Signal * IntensityFactor;
	ResonanceLevel = FMath::Clamp(ResonanceLevel, 0.0f, 100.0f);
}

void UEchoPulseResonanceChamber::Reset()
{
	ResonanceLevel = 0.0f;
}
