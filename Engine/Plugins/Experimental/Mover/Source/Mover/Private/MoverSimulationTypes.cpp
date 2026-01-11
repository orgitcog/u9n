// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoverSimulationTypes.h"

UScriptStruct* FMoverSimulationEventData::GetScriptStruct() const
{
	checkf(false, TEXT("%hs is being called erroneously. This must be overridden in derived types!"), __FUNCTION__);
	return FMoverSimulationEventData::StaticStruct();
}

namespace UE::Mover
{
	void FSimulationOutputData::Reset()
	{
		SyncState.Reset();
		LastUsedInputCmd.Reset();
		AdditionalOutputData.Empty();
		Events.Empty();
	}

	void FSimulationOutputData::Interpolate(const FSimulationOutputData& From, const FSimulationOutputData& To, float Alpha, float SimTimeMs)
	{
		SyncState.Interpolate(&From.SyncState, &To.SyncState, Alpha);
		LastUsedInputCmd.Interpolate(&From.LastUsedInputCmd, &To.LastUsedInputCmd, Alpha);
		AdditionalOutputData.Interpolate(From.AdditionalOutputData, To.AdditionalOutputData, Alpha);

		for (const TSharedPtr<FMoverSimulationEventData>& EventData : From.Events)
		{
			if (const FMoverSimulationEventData* DataPtr = EventData.Get())
			{
				if (DataPtr->EventTimeMs <= SimTimeMs)
				{
					Events.Add(EventData);
				}
			}
		}
		for (const TSharedPtr<FMoverSimulationEventData>& EventData : To.Events)
		{
			if (const FMoverSimulationEventData* DataPtr = EventData.Get())
			{
				if (DataPtr->EventTimeMs <= SimTimeMs)
				{
					Events.Add(EventData);
				}
			}
		}
	}

	void FSimulationOutputRecord::FData::Reset()
	{
		TimeStep = FMoverTimeStep();
		SimOutputData.Reset();
	}

	void FSimulationOutputRecord::Add(const FMoverTimeStep& InTimeStep, const FSimulationOutputData& InData)
	{
		CurrentIndex = (CurrentIndex + 1) % 2;
		Data[CurrentIndex] = { InTimeStep, InData };
	}

	const FSimulationOutputData& FSimulationOutputRecord::GetLatest() const
	{
		return Data[CurrentIndex].SimOutputData;
	}

	void FSimulationOutputRecord::GetInterpolated(float AtBaseTimeMs, FMoverTimeStep& OutTimeStep, FSimulationOutputData& OutData) const
	{
		const uint8 PrevIndex = (CurrentIndex + 1) % 2;
		const float PrevTimeMs = Data[PrevIndex].TimeStep.BaseSimTimeMs;
		const float CurrTimeMs = Data[CurrentIndex].TimeStep.BaseSimTimeMs;

		if (FMath::IsNearlyEqual(PrevTimeMs, CurrTimeMs) || (AtBaseTimeMs >= CurrTimeMs))
		{
			OutData = Data[CurrentIndex].SimOutputData;
			OutTimeStep = Data[CurrentIndex].TimeStep;
		}
		else if (AtBaseTimeMs <= PrevTimeMs)
		{
			OutData = Data[PrevIndex].SimOutputData;
			OutTimeStep = Data[PrevIndex].TimeStep;
		}
		else
		{
			const float Alpha = FMath::Clamp((AtBaseTimeMs - PrevTimeMs) / (CurrTimeMs - PrevTimeMs), 0.0f, 1.0f);
			OutData.Interpolate(Data[PrevIndex].SimOutputData, Data[CurrentIndex].SimOutputData, Alpha, AtBaseTimeMs);
			OutTimeStep = Data[PrevIndex].TimeStep;
		}

		OutTimeStep.BaseSimTimeMs = AtBaseTimeMs;
	}

	void FSimulationOutputRecord::Clear()
	{
		CurrentIndex = 1;
		Data[0].Reset();
		Data[1].Reset();
	}

} // namespace UE::Mover