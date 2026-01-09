// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanOneEuroFilter.h"

#include "Math/UnrealMathUtility.h"

FMetaHumanOneEuroFilter::FMetaHumanLowpassFilter::FMetaHumanLowpassFilter() :
	Previous(0),
	bFirstTime(true)
{

}

double FMetaHumanOneEuroFilter::FMetaHumanLowpassFilter::Filter(const double InValue, const double InAlpha)
{
	double Result = InValue;
	if (!bFirstTime)
	{
		Result = InAlpha * InValue + (1 - InAlpha) * Previous;
	}

	bFirstTime = false;
	Previous = Result;
	return Result;
}

bool FMetaHumanOneEuroFilter::FMetaHumanLowpassFilter::IsFirstTime() const
{
	return bFirstTime;
}

double FMetaHumanOneEuroFilter::FMetaHumanLowpassFilter::GetPrevious() const
{
	return Previous;
}

FMetaHumanOneEuroFilter::FMetaHumanOneEuroFilter() :
	MinCutoff(1.0f),
	CutoffSlope(0.007f),
	DeltaCutoff(1.0f)
{

}

FMetaHumanOneEuroFilter::FMetaHumanOneEuroFilter(const double InMinCutoff, const double InCutoffSlope, const double InDeltaCutoff) :
	MinCutoff(InMinCutoff),
	CutoffSlope(InCutoffSlope),
	DeltaCutoff(InDeltaCutoff)
{

}

double FMetaHumanOneEuroFilter::Filter(const double InRaw, const double InDeltaTime)
{
	// Calculate the delta, if this is the first time then there is no delta
	const double Delta = RawFilter.IsFirstTime() == true ? 0 : (InRaw - RawFilter.GetPrevious()) * InDeltaTime;

	// Filter the delta to get the estimated
	const double Estimated = DeltaFilter.Filter(Delta, CalculateAlpha(DeltaCutoff, InDeltaTime));

	// Use the estimated to calculate the cutoff
	const double Cutoff = CalculateCutoff(Estimated);

	// Filter passed value 
	return RawFilter.Filter(InRaw, CalculateAlpha(Cutoff, InDeltaTime));
}

void FMetaHumanOneEuroFilter::SetMinCutoff(const double InMinCutoff)
{
	MinCutoff = InMinCutoff;
}

void FMetaHumanOneEuroFilter::SetCutoffSlope(const double InCutoffSlope)
{
	CutoffSlope = InCutoffSlope;
}

void FMetaHumanOneEuroFilter::SetDeltaCutoff(const double InDeltaCutoff)
{
	DeltaCutoff = InDeltaCutoff;
}

const double FMetaHumanOneEuroFilter::CalculateCutoff(const double InValue)
{
	double Result;
	Result = MinCutoff + CutoffSlope * FMath::Abs(InValue);
	return Result;
}

const double FMetaHumanOneEuroFilter::CalculateAlpha(const double InCutoff, const double InDeltaTime) const
{
	const double tau = 1.0 / (2 * UE_DOUBLE_PI * InCutoff);
	return 1.0 / (1.0 + tau / InDeltaTime);
}
