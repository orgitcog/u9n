// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/************************************************************************/
/* 1 Euro filter smoothing algorithm									*/
/* http://cristal.univ-lille.fr/~casiez/1euro/							*/
/************************************************************************/

// This is a copy of Engine\Source\Editor\ViewportInteraction\Public\ViewportInteractionUtils.h
// but modified to work on single value floats not vectors

class METAHUMANCORETECH_API FMetaHumanOneEuroFilter
{
private:

	class FMetaHumanLowpassFilter
	{
	public:

		/** Default constructor */
		FMetaHumanLowpassFilter();

		/** Calculate */
		double Filter(const double InValue, const double InAlpha);

		/** If the filter was not executed yet */
		bool IsFirstTime() const;

		/** Get the previous filtered value */
		double GetPrevious() const;

	private:

		/** The previous filtered value */
		double Previous;

		/** If this is the first time doing a filter */
		bool bFirstTime;
	};

public:

	/** Default constructor */
	FMetaHumanOneEuroFilter();

	FMetaHumanOneEuroFilter(const double InMinCutoff, const double InCutoffSlope, const double InDeltaCutoff);

	/** Smooth parameter */
	double Filter(const double InRaw, const double InDeltaTime);

	/** Set the minimum cutoff */
	void SetMinCutoff(const double InMinCutoff);

	/** Set the cutoff slope */
	void SetCutoffSlope(const double InCutoffSlope);

	/** Set the delta slope */
	void SetDeltaCutoff(const double InDeltaCutoff);

private:

	const double CalculateCutoff(const double InValue);
	const double CalculateAlpha(const double InCutoff, const double InDeltaTime) const;

	double MinCutoff;
	double CutoffSlope;
	double DeltaCutoff;
	FMetaHumanLowpassFilter RawFilter;
	FMetaHumanLowpassFilter DeltaFilter;
};
