// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneInterpolatingPointsDrawTask.h"

#include "Cache/MovieSceneCachedCurve.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieScenePiecewiseCurve.h"

namespace UE::MovieSceneTools
{
	template struct FMovieSceneInterpolatingPointsDrawTask<FMovieSceneFloatChannel>;
	template struct FMovieSceneInterpolatingPointsDrawTask<FMovieSceneDoubleChannel>;

	template <typename ChannelType>
	FMovieSceneInterpolatingPointsDrawTask<ChannelType>::FMovieSceneInterpolatingPointsDrawTask(
		const TSharedRef<FMovieSceneCachedCurve<ChannelType>>& CachedCurve,
		const TFunction<void(TArray<FVector2D>, TArray<int32>)>& InCallback)
		: Callback(InCallback)
		, ScreenSpace(CachedCurve->GetScreenSpace())
		, TickResolution(CachedCurve->GetTickResolution())
		, TimeThreshold(CachedCurve->GetTimeThreshold())
		, ValueThreshold(CachedCurve->GetValueThreshold())
		, PiecewiseCurve(CachedCurve->GetPiecewiseCurve())
	{
		const TArray<const FFrameNumber>& Times = CachedCurve->GetTimes();
		const TArray<const ChannelValueType>& Values = CachedCurve->GetValues();
		if (!ensureMsgf(Times.Num() > 1, TEXT("Curve paint tasks should only be created for curves with more than one key")))
		{
			SetFlags(ECurvePainterTaskStateFlags::Completed);
			return;
		}

		// Remember key points
		KeyPoints.Reserve(Times.Num());
		for (int32 DataIndex = 0; DataIndex < Times.Num(); DataIndex++)
		{
			KeyPoints.Emplace(Times[DataIndex] / TickResolution, double(Values[DataIndex].Value));
		}

		InterpolatingPoints = KeyPoints;
	}

	template <typename ChannelType>
	void FMovieSceneInterpolatingPointsDrawTask<ChannelType>::RefineFullRangeInterpolatingPoints()
	{		
		// Make sure there's no concurrency
		if (!AccessCurvePointsMutex.TryLock())
		{
			return;
		}

		const int32 OldSize = InterpolatingPoints.Num();
		RefineFullRangeInterpolatingPointsInternal();

		const int32 NewSize = InterpolatingPoints.Num();
		if (OldSize == NewSize)
		{
			InvokeCallback();
			SetFlags(ECurvePainterTaskStateFlags::Completed);
		}

		AccessCurvePointsMutex.Unlock();
	}

	template <typename ChannelType>
	void FMovieSceneInterpolatingPointsDrawTask<ChannelType>::SetFlags(ECurvePainterTaskStateFlags NewFlags)
	{
		StateFlags.store(NewFlags);
	}

	template <typename ChannelType>
	bool FMovieSceneInterpolatingPointsDrawTask<ChannelType>::HasAnyFlags(ECurvePainterTaskStateFlags Flags) const
	{
		return EnumHasAnyFlags(StateFlags.load(), Flags);
	}

	template<typename ChannelType>
	void FMovieSceneInterpolatingPointsDrawTask<ChannelType>::RefineFullRangeInterpolatingPointsInternal()
	{			
		constexpr float InterpTimes[] = { 0.25f, 0.5f, 0.6f };
		for (int32 Index = 0; Index < InterpolatingPoints.Num() - 1; Index++)
		{
			const FVector2D& Lower = InterpolatingPoints[Index];
			const FVector2D& Upper = InterpolatingPoints[Index + 1];

			if ((Upper.X - Lower.X) >= TimeThreshold)
			{
				bool bSegmentIsLinear = true;

				FVector2D Evaluated[UE_ARRAY_COUNT(InterpTimes)] = { FVector2D::ZeroVector };
				for (int32 InterpIndex = 0; InterpIndex < UE_ARRAY_COUNT(InterpTimes); ++InterpIndex)
				{
					double& EvalTime = Evaluated[InterpIndex].X;
					EvalTime = FMath::Lerp(Lower.X, Upper.X, InterpTimes[InterpIndex]);
					double Value = 0.0;

					PiecewiseCurve->Evaluate(EvalTime * TickResolution, Value);
					const double LinearValue = FMath::Lerp(Lower.Y, Upper.Y, InterpTimes[InterpIndex]);
					if (bSegmentIsLinear)
					{
						bSegmentIsLinear = FMath::IsNearlyEqual(Value, LinearValue, ValueThreshold);
					}

					Evaluated[InterpIndex].Y = Value;
				}

				if (!bSegmentIsLinear)
				{
					// Add the point
					InterpolatingPoints.Insert(Evaluated, UE_ARRAY_COUNT(Evaluated), Index + 1);
					--Index;
				}
			}
		}
	}

	template<typename ChannelType>
	void FMovieSceneInterpolatingPointsDrawTask<ChannelType>::InvokeCallback()
	{
		TArray<int32> KeyOffsets;

		int32 Offset = 0;
		for (int32 InterpPointIndex = 0; InterpPointIndex < InterpolatingPoints.Num(); InterpPointIndex++)
		{
			if (KeyPoints.Num() <= Offset)
			{
				KeyOffsets.Add(InterpPointIndex);
				break;
			}
			else if (InterpolatingPoints[InterpPointIndex].X >= KeyPoints[Offset].X)
			{
				KeyOffsets.Add(InterpPointIndex);
				Offset++;
			}
		}

		Callback(InterpolatingPoints, KeyOffsets);
	}
}
