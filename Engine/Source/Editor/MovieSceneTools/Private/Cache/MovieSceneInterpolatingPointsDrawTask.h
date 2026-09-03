// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cache/MovieSceneCurveCachePool.h"
#include "CurveEditorScreenSpace.h"
#include "HAL/CriticalSection.h"
#include "Misc/FrameRate.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

namespace UE::MovieScene { struct FPiecewiseCurve; }

namespace UE::MovieSceneTools
{
	template <typename ChannelType> class FMovieSceneCachedCurve;

	/** 
	 * A task to draw interpolating points.
	 * Draws interpolation for the visible space when interactive, but also the full range of the curve.
	 */
	template <typename ChannelType>
	struct FMovieSceneInterpolatingPointsDrawTask
		: public IMovieSceneInterpolatingPointsDrawTask
	{		
		/** The type of channel value structs */
		using ChannelValueType = typename ChannelType::ChannelValueType;
		using CurveValueType = typename ChannelType::CurveValueType;

	public:
		FMovieSceneInterpolatingPointsDrawTask(
			const TSharedRef<FMovieSceneCachedCurve<ChannelType>>& CachedCurve,
			const TFunction<void(TArray<FVector2D> /** OutInterpolatedPoints */, TArray<int32> /** OutKeyOffsets */)>& InCallback);

		//~ Begin IMovieSceneInterpolatingPointsDrawTask interface
		virtual void SetFlags(ECurvePainterTaskStateFlags NewFlags) override;
		virtual bool HasAnyFlags(ECurvePainterTaskStateFlags Flags) const override;
		virtual void RefineFullRangeInterpolatingPoints() override;
		//~ End IMovieSceneInterpolatingPointsDrawTask interface

	private:
		/** Refines the full range of the interpolating points. */
		void RefineFullRangeInterpolatingPointsInternal();

		/** Calls back to the cached curve, forwarding interpolated points */
		void InvokeCallback();

		/** The callback to execute on completion */
		const TFunction<void(TArray<FVector2D> /** OutInterpolatedPoints */, TArray<int32> /** OutKeyOffsets */)> Callback;

		/** The screenspace in which we paint */
		FCurveEditorScreenSpace ScreenSpace;

		/** The current tick resolution */
		FFrameRate TickResolution;

		/** Threshold of visible times */
		double TimeThreshold = 0.0;

		/** Threshold of visible values */
		double ValueThreshold = 0.0;

		/** The curve as piecewise curve */
		TSharedPtr<UE::MovieScene::FPiecewiseCurve> PiecewiseCurve;

		/** The initial points that are being interpolated */
		TArray<FVector2D> KeyPoints;

		/** The (possibly not yet fully interpolated) interpolating points */
		TArray<FVector2D> InterpolatingPoints;

		/** Critical section to enter when accessing curve points */
		FCriticalSection AccessCurvePointsMutex;

		/** True if the task needs to iterate again */
		std::atomic<ECurvePainterTaskStateFlags> StateFlags = ECurvePainterTaskStateFlags::None;
	};
}
