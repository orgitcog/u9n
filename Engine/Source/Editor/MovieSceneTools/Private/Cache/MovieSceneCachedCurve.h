// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cache/MovieSceneCurveCachePool.h"
#include "Cache/MovieSceneInterpolatingPointsDrawTask.h"
#include "Cache/MovieSceneUpdateCachedCurveData.h"
#include "Channels/MovieSceneInterpolation.h"
#include "CurveDataAbstraction.h"
#include "CurveEditor.h"
#include "CurveEditorCurveDrawParamsHandle.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditorSettings.h"

namespace UE::MovieSceneTools
{
	template <typename ChannelType> struct FMovieSceneUpdateCachedCurveData;

	/** Flags defining how the cache changed when it was last updated */
	enum class EMovieSceneCurveCacheChangeFlags : uint32
	{
		None = 0,
		ChangedPosition = 1 << 0,
		ChangedSize = 1 << 1,
		ChangedKeyIndices = 1 << 2,
		ChangedTangentVisibility = 1 << 3,
		ChangedSelection = 1 << 4,
		ChangedCurveData = 1 << 5,
		ChangedInterpolatingPoints = 1 << 6,
	};
	ENUM_CLASS_FLAGS(EMovieSceneCurveCacheChangeFlags);

	/** Class holding cached curve data to speed up drawing the curve */
	template <typename ChannelType>
	class FMovieSceneCachedCurve
		: public IMovieSceneCachedCurve
	{
	private:
		/** The type of channel value structs */
		using ChannelValueType = typename ChannelType::ChannelValueType;

	public:
		FMovieSceneCachedCurve(const FCurveModelID& InCurveModelID);
		virtual ~FMovieSceneCachedCurve();

		/** Initializes the cached curve */
		void Initialize(TWeakPtr<const FCurveEditor> WeakCurveEditor);

		/** Returns true when the curve changed */
		bool HasChanged() const { return Flags != EMovieSceneCurveCacheChangeFlags::None; }

		/** Updates the cached curve data. Doesn't draw the curve */
		void UpdateCachedCurve(const FMovieSceneUpdateCachedCurveData<ChannelType>& UpdateData, const UE::CurveEditor::FCurveDrawParamsHandle& CurveDrawParamsHandle);

		/** Draws the cached curve to the actual draw params */
		virtual void DrawCachedCurve() override;

		/** Returns the curve model ID that corresponds to the cached curve */
		virtual const FCurveModelID& GetID() const override { return CurveModelID; }

		/** Returns the cached screen space */
		const FCurveEditorScreenSpace& GetScreenSpace() const { return ScreenSpace; }

		/** Returns the cached tick resolution */
		const FFrameRate& GetTickResolution() const { return TickResolution; }

		/** Cached first index of the visible space in keys / values */
		int32 GetStartingIndex() const { return StartingIndex; }

		/** Cached last index of the visible space in keys / values */
		int32 GetEndingIndex() const { return EndingIndex; }

		/** Returns cached curve times */
		TArray<const FFrameNumber> GetTimes() const { return Times; }

		/** Returns cached curve values */
		TArray<const ChannelValueType> GetValues() const { return Values; }

		/** Returns the threshold of visible pixels per time */
		double GetTimeThreshold() const { return FMath::Max(0.0001, 1.0 / ScreenSpace.PixelsPerInput()); }

		/** Returns the threshold of visible pixels per value */
		double GetValueThreshold() const { return FMath::Max(0.0001, 1.0 / ScreenSpace.PixelsPerOutput()); }

		/** Returns the cached piecewise curve */
		const TSharedPtr<UE::MovieScene::FPiecewiseCurve>& GetPiecewiseCurve() const { return PiecewiseCurve; }

		// IMovieSceneCachedCurve interface
		virtual uint32 GetInterpolatingPointsHash() const override;
		virtual const UE::CurveEditor::FCurveDrawParamsHandle& GetDrawParamsHandle() const override;
		// ~IMovieSceneCachedCurve interface

		/** The curve model ID that corresponds to the cached curve */
		const FCurveModelID CurveModelID;

	private:
		/** Updates the cached screen space */
		void UpdateScreenSpace(const FCurveEditorScreenSpace& NewScreenSpace);

		/** Updates the cached tick resolution */
		void UpdateTickResolution(const FFrameRate& NewTickResolution);

		/** Updates the cached input display offset */
		void UpdateInputDisplayOffset(const double NewInputDisplayOffset);

		/** Updates the cached selection */
		void UpdateSelection(const FMovieSceneUpdateCachedCurveData<ChannelType>& UpdateData);

		/** Updates the tangent visiblity */
		void UpdateTangentVisibility(const ECurveEditorTangentVisibility NewTangentVisibility);

		/** Updates cached curve data such as keys, values, key handles and key attributes */
		void UpdateCurveData(const FMovieSceneUpdateCachedCurveData<ChannelType>& UpdateData, const FFrameNumber& StartFrame, const FFrameNumber& EndFrame);

		/** Updates the cached pre and post inifinity extrapolation */
		void UpdatePrePostInfinityExtrapolation(const FMovieSceneUpdateCachedCurveData<ChannelType>& UpdateData, const FFrameNumber& StartFrame, const FFrameNumber& EndFrame);

		/** Updates key draw info depending on flags */
		void ConditionallyUpdateKeyDrawInfos(const FMovieSceneUpdateCachedCurveData<ChannelType>& UpdateData);

		/** Updates the piecewise cuve depending on flags */
		void ConditionallyUpdatePiecewiseCurve(const FMovieSceneUpdateCachedCurveData<ChannelType>& Data);

		/** 
		 * When the curve is being edited interactively, paints the visible part synchronous. 
		 * When curve data changed in any way, paints the whole range of the curve async.
		 */
		void ConditionallyPaintCurve(const ChannelType& Channel);

		/** Draws the interpolating points from the currently cached full range interpolating points */
		void DrawInterpolatingPointsFromFullRange();

		/** Draws the pre-infinity interpolating points */
		void DrawPreInfinityInterpolatingPoints();

		/** Draws the post-infinity interpolating points */
		void DrawPostInfinityInterpolatingPoints();

		/** Tries to draw a single pre-infinity interpolating point fast */
		[[nodiscard]] bool TryDrawPreInfinityExtentFast();

		/** Tries to draw a single post-infinity interpolating point fast */
		[[nodiscard]] bool TryDrawPostInfinityExtentFast();

		/** Draws the keys of the curve */
		void DrawKeys();

		/** Applies the cached draw params to the actual draw params */
		void ApplyDrawParams();

		/** Cached screen space */
		FCurveEditorScreenSpace ScreenSpace;

		/** Cached tick resolution */
		FFrameRate TickResolution;

		/** Cached input display offset */
		double InputDisplayOffset = TNumericLimits<double>::Max();

		/** Cached first index of the visible space in keys / values */
		int32 StartingIndex = INDEX_NONE;

		/** Cached last index of the visible space in keys / values */
		int32 EndingIndex = INDEX_NONE;

		/** Cached selection */
		FKeyHandleSet Selection;

		/** Cached tangent visibility. Optional soley to detect the initial change */
		TOptional<ECurveEditorTangentVisibility> TangentVisibility;

		/** Cached default value */
		double DefaultValue = 0.0;

		/** Cached curve times */
		TArray<const FFrameNumber> Times;

		/** Cached curve values */
		TArray<const ChannelValueType> Values;

		/** Cached key handles */
		TArray<FKeyHandle> KeyHandles;

		/** Cached key attributes */
		TArray<FKeyAttributes> KeyAttributes;

		/** Key draw info as a tuple of the key, an optional arrive tangent and an optional leave tangent */
		TArray<TTuple<FKeyDrawInfo, TOptional<FKeyDrawInfo>, TOptional<FKeyDrawInfo>>> KeyDrawInfos;

		/** The curve as piecewise curve. */
		TSharedPtr<UE::MovieScene::FPiecewiseCurve> PiecewiseCurve;

		/** Struct holding the whole range of interpolating points. */
		struct FFullRangeinterpolatingPoints
		{
			/** The interpolating points in the finite curve range */
			TArray<FVector2D> Points;

			/** Offsets of keys in the points array */
			TArray<int32> KeyOffsets;

			/** Hash of the full range interpolating points */
			std::atomic<uint32> Hash = 0;

			/** How pre-infinity should be extrapolated */
			ERichCurveExtrapolation PreInfinityExtrapolation = ERichCurveExtrapolation::RCCE_None;

			/** How post-infinity should be extrapolated */
			ERichCurveExtrapolation PostInfinityExtrapolation = ERichCurveExtrapolation::RCCE_None;

		};

		/** The whole range of interpolating points. Useful to avoid drawing the visible range when curve and zoom don't change. */
		FFullRangeinterpolatingPoints FullRangeInterpolation;

		/** Critical section to enter when accessing the whole range or the cached draw params interpolating points */
		mutable FCriticalSection LockInterpolatingPoints;

		/** Cached curve draw params */
		FCurveDrawParams CachedDrawParams;

		/** Handle to the actual draw params we're drawing to */
		UE::CurveEditor::FCurveDrawParamsHandle DrawParamsHandle;

		/** Flags defining how the cache changed since it last was updated */
		std::atomic<EMovieSceneCurveCacheChangeFlags> Flags = EMovieSceneCurveCacheChangeFlags::ChangedCurveData;
	};

	template <typename ChannelType>
	FMovieSceneCachedCurve<ChannelType>::FMovieSceneCachedCurve(const FCurveModelID& InCurveModelID)
		: CurveModelID(InCurveModelID)
		, ScreenSpace(FVector2D::ZeroVector, 0.0, 1.0, 0.0, 1.0)
		, CachedDrawParams(InCurveModelID)
	{}

	template <typename ChannelType>
	FMovieSceneCachedCurve<ChannelType>::~FMovieSceneCachedCurve()
	{
		FMovieSceneCurveCachePool::Get().Leave(*this);
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::Initialize(TWeakPtr<const FCurveEditor> WeakCurveEditor)
	{
		FMovieSceneCurveCachePool::Get().Join(WeakCurveEditor, AsShared());
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::UpdateCachedCurve(const FMovieSceneUpdateCachedCurveData<ChannelType>& UpdateData, const UE::CurveEditor::FCurveDrawParamsHandle& CurveDrawParamsHandle)
	{
		DrawParamsHandle = CurveDrawParamsHandle;

		CachedDrawParams.Color = UpdateData.CurveModel.GetColor();
		CachedDrawParams.Thickness = UpdateData.CurveModel.GetThickness();
		CachedDrawParams.DashLengthPx = UpdateData.CurveModel.GetDashLength();
		CachedDrawParams.bKeyDrawEnabled = UpdateData.CurveModel.IsKeyDrawEnabled();

		/** Cached first fame which needs to be updated  */
		const FFrameNumber StartFrame = (UpdateData.ScreenSpace.GetInputMin() * UpdateData.TickResolution).FloorToFrame();
		const FFrameNumber EndFrame = (UpdateData.ScreenSpace.GetInputMax() * UpdateData.TickResolution).CeilToFrame();

		UpdateScreenSpace(UpdateData.ScreenSpace);
		UpdateTickResolution(UpdateData.TickResolution);
		UpdateTangentVisibility(UpdateData.CurveEditor.GetSettings()->GetTangentVisibility());
		UpdateInputDisplayOffset(UpdateData.CurveModel.GetInputDisplayOffset());
		UpdateSelection(UpdateData);
		UpdateCurveData(UpdateData, StartFrame, EndFrame);
		UpdatePrePostInfinityExtrapolation(UpdateData, StartFrame, EndFrame);

		// Update depending on above data
		ConditionallyUpdateKeyDrawInfos(UpdateData);
		ConditionallyUpdatePiecewiseCurve(UpdateData);
		ConditionallyPaintCurve(UpdateData.Channel);
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::DrawCachedCurve()
	{
		if (Flags == EMovieSceneCurveCacheChangeFlags::None)
		{
			// Only apply cached data if nothing changed.
			ApplyDrawParams();
		}
		else
		{
			DrawKeys();
			DrawInterpolatingPointsFromFullRange();

			ApplyDrawParams();

			Flags = EMovieSceneCurveCacheChangeFlags::None;
		}
	}

	template <typename ChannelType>
	uint32 FMovieSceneCachedCurve<ChannelType>::GetInterpolatingPointsHash() const
	{
		return FullRangeInterpolation.Hash.load();
	}

	template <typename ChannelType>
	const UE::CurveEditor::FCurveDrawParamsHandle& FMovieSceneCachedCurve<ChannelType>::GetDrawParamsHandle() const
	{
		check(IsInGameThread());
		return DrawParamsHandle;
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::UpdateScreenSpace(const FCurveEditorScreenSpace& NewScreenSpace)
	{
		const bool bScreenPositionChanged =
			!FMath::IsNearlyEqual(ScreenSpace.GetInputMin(), NewScreenSpace.GetInputMin()) ||
			!FMath::IsNearlyEqual(ScreenSpace.GetInputMax(), NewScreenSpace.GetInputMax()) ||
			!FMath::IsNearlyEqual(ScreenSpace.GetOutputMin(), NewScreenSpace.GetOutputMin()) ||
			!FMath::IsNearlyEqual(ScreenSpace.GetOutputMax(), NewScreenSpace.GetOutputMax());

		const EMovieSceneCurveCacheChangeFlags ScreenPositionChangedFlag = bScreenPositionChanged ? EMovieSceneCurveCacheChangeFlags::ChangedPosition : EMovieSceneCurveCacheChangeFlags::None;

		const bool bScreenSizeChanged =
			!FMath::IsNearlyEqual(ScreenSpace.PixelsPerInput(), NewScreenSpace.PixelsPerInput()) ||
			!FMath::IsNearlyEqual(ScreenSpace.PixelsPerOutput(), NewScreenSpace.PixelsPerOutput());

		const EMovieSceneCurveCacheChangeFlags ScreenSizeChangedFlag = bScreenSizeChanged ? EMovieSceneCurveCacheChangeFlags::ChangedSize : EMovieSceneCurveCacheChangeFlags::None;

		if (bScreenPositionChanged || bScreenSizeChanged)
		{
			ScreenSpace = NewScreenSpace;
			Flags.store(Flags.load() | ScreenPositionChangedFlag | ScreenSizeChangedFlag);
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::UpdateTickResolution(const FFrameRate& NewTickResolution)
	{
		if (TickResolution != NewTickResolution)
		{
			TickResolution = NewTickResolution;
			Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedPosition | EMovieSceneCurveCacheChangeFlags::ChangedSize);
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::UpdateInputDisplayOffset(const double NewInputDisplayOffset)
	{
		if (InputDisplayOffset != NewInputDisplayOffset)
		{
			InputDisplayOffset = NewInputDisplayOffset;
			Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedPosition);
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::UpdateSelection(const FMovieSceneUpdateCachedCurveData<ChannelType>& UpdateData)
	{
		const FKeyHandleSet* SelectionPtr = UpdateData.CurveEditor.GetSelection().FindForCurve(CurveModelID);
		const FKeyHandleSet	NewSelection = SelectionPtr ? *SelectionPtr : FKeyHandleSet();

		const TArrayView<const FKeyHandle> SelectedKeyHandles = Selection.AsArray();
		const TArrayView<const FKeyHandle> NewSelectedKeyHandles = NewSelection.AsArray();
		if (SelectedKeyHandles.Num() != NewSelectedKeyHandles.Num() ||
			FMemory::Memcmp(SelectedKeyHandles.GetData(), NewSelectedKeyHandles.GetData(), SelectedKeyHandles.Num()) != 0)
		{
			Selection = NewSelection;
			Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedSelection);
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::UpdateTangentVisibility(const ECurveEditorTangentVisibility NewTangentVisibility)
	{
		if (!TangentVisibility.IsSet() || TangentVisibility.GetValue() != NewTangentVisibility)
		{
			TangentVisibility = NewTangentVisibility;

			Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedTangentVisibility);
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::UpdateCurveData(const FMovieSceneUpdateCachedCurveData<ChannelType>& UpdateData, const FFrameNumber& StartFrame, const FFrameNumber& EndFrame)
	{
		// DefaultValue
		const double NewDefaultValue = UpdateData.Channel.GetDefault().IsSet() ? UpdateData.Channel.GetDefault().GetValue() : 0.0;
		if (DefaultValue != NewDefaultValue)
		{
			DefaultValue = NewDefaultValue;

			Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedCurveData);
		}

		// Times
		if (Times != UpdateData.Times)
		{
			Times = UpdateData.Times;

			Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedCurveData);
		}

		// Values
		if (Values != UpdateData.Values)
		{
			Values = TArray<const ChannelValueType>(UpdateData.Values.GetData(), UpdateData.Values.Num());

			Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedCurveData);
		}

		// Key Handles
		TArray<FKeyHandle> NewKeyHandles;
		NewKeyHandles.Reserve(Times.Num());

		UpdateData.CurveModel.GetKeys(TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), NewKeyHandles);
		if (KeyHandles != NewKeyHandles)
		{
			KeyHandles = NewKeyHandles;

			Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedCurveData);
		}

		// Key Attributes
		TArray<FKeyAttributes> NewKeyAttributes;
		NewKeyAttributes.SetNum(KeyHandles.Num());

		UpdateData.CurveModel.GetKeyAttributes(KeyHandles, NewKeyAttributes);

		if (KeyAttributes != NewKeyAttributes)
		{
			KeyAttributes = NewKeyAttributes;

			Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedCurveData);
		}

		// Indices
		const int32 NewStartingIndex = UpdateData.Times.IsEmpty() ? INDEX_NONE : Algo::LowerBound(UpdateData.Times, StartFrame);
		const int32 NewEndingIndex = [&UpdateData, &EndFrame, this]()
			{
				int32 UpperBoundIndex = UpdateData.Times.IsEmpty() ? INDEX_NONE : Algo::UpperBound(UpdateData.Times, EndFrame) - 1;
				UpperBoundIndex = FMath::Min(UpperBoundIndex, Times.Num() - 1);
				UpperBoundIndex = FMath::Min(UpperBoundIndex, Values.Num() - 1);
				UpperBoundIndex = FMath::Min(UpperBoundIndex, KeyHandles.Num() - 1);
				UpperBoundIndex = FMath::Min(UpperBoundIndex, KeyAttributes.Num() - 1);
				
				return UpperBoundIndex;
			}();

		if (StartingIndex != NewStartingIndex ||
			EndingIndex != NewEndingIndex)
		{
			StartingIndex = NewStartingIndex;
			EndingIndex = NewEndingIndex;

			Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedKeyIndices);
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::UpdatePrePostInfinityExtrapolation(const FMovieSceneUpdateCachedCurveData<ChannelType>& UpdateData, const FFrameNumber& StartFrame, const FFrameNumber& EndFrame)
	{
		const ERichCurveExtrapolation NewPreInfinityExtrapolation = UpdateData.Channel.PreInfinityExtrap;
		const ERichCurveExtrapolation NewPostInfinityExtrapolation = UpdateData.Channel.PostInfinityExtrap;

		if (FullRangeInterpolation.PreInfinityExtrapolation != NewPreInfinityExtrapolation ||
			FullRangeInterpolation.PostInfinityExtrapolation != NewPostInfinityExtrapolation)
		{
			FullRangeInterpolation.PreInfinityExtrapolation = NewPreInfinityExtrapolation;
			FullRangeInterpolation.PostInfinityExtrapolation = NewPostInfinityExtrapolation;

			Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedCurveData);
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::ConditionallyUpdateKeyDrawInfos(const FMovieSceneUpdateCachedCurveData<ChannelType>& UpdateData)
	{
		const bool bNeedsUpdate = EnumHasAnyFlags(Flags.load(),
			EMovieSceneCurveCacheChangeFlags::ChangedCurveData |
			EMovieSceneCurveCacheChangeFlags::ChangedSelection |
			EMovieSceneCurveCacheChangeFlags::ChangedTangentVisibility);

		if (bNeedsUpdate)
		{
			UpdateData.CurveModel.GetKeyDrawInfo(ECurvePointType::ArriveTangent, FKeyHandle::Invalid(), CachedDrawParams.ArriveTangentDrawInfo);
			UpdateData.CurveModel.GetKeyDrawInfo(ECurvePointType::LeaveTangent, FKeyHandle::Invalid(), CachedDrawParams.LeaveTangentDrawInfo);

			KeyDrawInfos.Reset(KeyHandles.Num());
			for (int32 DataIndex = 0; DataIndex < KeyHandles.Num(); DataIndex++)
			{
				const FKeyHandle& KeyHandle = KeyHandles[DataIndex];

				// Key draw info
				FKeyDrawInfo KeyDrawInfo;
				UpdateData.CurveModel.GetKeyDrawInfo(ECurvePointType::Key, KeyHandle, KeyDrawInfo);

				// Tangent draw info
				const FKeyAttributes* KeyAttributesPtr = nullptr;
				if (KeyAttributes.IsValidIndex(DataIndex))
				{
					if (TangentVisibility == ECurveEditorTangentVisibility::SelectedKeys)
					{
						if (Selection.Contains(KeyHandle, ECurvePointType::Any))
						{
							KeyAttributesPtr = &KeyAttributes[DataIndex];
						}
					}
					else if (TangentVisibility == ECurveEditorTangentVisibility::AllTangents)
					{
						KeyAttributesPtr = &KeyAttributes[DataIndex];
					}
				}

				TOptional<FKeyDrawInfo> ArriveTangentDrawInfo;
				if (KeyAttributesPtr && KeyAttributesPtr->HasArriveTangent())
				{
					FKeyDrawInfo NewArriveTangentDrawInfo;
					UpdateData.CurveModel.GetKeyDrawInfo(ECurvePointType::ArriveTangent, KeyHandle, NewArriveTangentDrawInfo);
					ArriveTangentDrawInfo = NewArriveTangentDrawInfo;
				}

				TOptional<FKeyDrawInfo> LeaveTangentDrawInfo;
				if (KeyAttributesPtr && KeyAttributesPtr->HasLeaveTangent())
				{
					FKeyDrawInfo NewLeaveTangentDrawInfo;
					UpdateData.CurveModel.GetKeyDrawInfo(ECurvePointType::ArriveTangent, KeyHandle, NewLeaveTangentDrawInfo);
					LeaveTangentDrawInfo = NewLeaveTangentDrawInfo;
				}

				KeyDrawInfos.Emplace(KeyDrawInfo, ArriveTangentDrawInfo, LeaveTangentDrawInfo);
			}
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::ConditionallyUpdatePiecewiseCurve(const FMovieSceneUpdateCachedCurveData<ChannelType>& Data)
	{
		if (EnumHasAnyFlags(Flags.load(), EMovieSceneCurveCacheChangeFlags::ChangedCurveData))
		{
			constexpr bool bWithPreAndPostInfinityExtrap = false;
			PiecewiseCurve = MakeShared<UE::MovieScene::FPiecewiseCurve>(Data.Channel.AsPiecewiseCurve(bWithPreAndPostInfinityExtrap));
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::ConditionallyPaintCurve(const ChannelType& Channel)
	{		
		// Only draw the full range interpolating points when the curve was edited or when zooming
		const bool bCurveIsBeingEdited = EnumHasAnyFlags(Flags.load(), EMovieSceneCurveCacheChangeFlags::ChangedCurveData);
		const bool bZooming = EnumHasAnyFlags(Flags.load(), EMovieSceneCurveCacheChangeFlags::ChangedSize);
		const bool bInteractive = bCurveIsBeingEdited || bZooming;
		if (!bInteractive)
		{
			return;
		}

		// Optimization: If there's less than two keys there's nothing to interpolate. 
		// FMovieSceneInterpolatingPointsDrawTask ensures no such tasks are created to avoid needless overhead. 
		// Adopt this principle here.
		if (Times.IsEmpty() || Values.IsEmpty())
		{
			FullRangeInterpolation.KeyOffsets.Reset();
			FullRangeInterpolation.Points.Reset();
			FullRangeInterpolation.Hash = 0;

			return;
		}
		else if (Times.Num() == 1 || Values.Num() == 1)
		{
			FullRangeInterpolation.KeyOffsets = { 0 };

			FullRangeInterpolation.Points.Reset();
			FullRangeInterpolation.Points.Emplace(Times[0].Value, Values[0].Value);

			const int32 X = Times[0].Value;
			const int32 Y = FMath::RoundToInt32(Values[0].Value);
			FullRangeInterpolation.Hash = HashCombine(X, Y);

			return;
		}

		// Draw interactive changes to the draw params directly
		const double StartTimeSeconds = ScreenSpace.GetInputMin();
		const double EndTimeSeconds = ScreenSpace.GetInputMax();
		const double TimeThreshold = GetTimeThreshold();
		const double ValueThreshold = GetValueThreshold();

		TArray<TTuple<double, double>> InteractiveInterpolatingPoints;
		Channel.PopulateCurvePoints(StartTimeSeconds, EndTimeSeconds, TimeThreshold, ValueThreshold, TickResolution, InteractiveInterpolatingPoints);

		CachedDrawParams.InterpolatingPoints.Reset();
		CachedDrawParams.InterpolatingPoints.Reserve(InteractiveInterpolatingPoints.Num());

		// Convert the interpolating points to screen space
		for (const TTuple<double, double>& Point : InteractiveInterpolatingPoints)
		{
			CachedDrawParams.InterpolatingPoints.Emplace(
				ScreenSpace.SecondsToScreen(Point.Get<0>() + InputDisplayOffset),
				ScreenSpace.ValueToScreen(Point.Get<1>())
			);
		}

		DrawKeys();
		ApplyDrawParams();

		Flags = EMovieSceneCurveCacheChangeFlags::None;

		// Create a draw task to draw interpolating points for the full range of the curve
		const TSharedRef<FMovieSceneInterpolatingPointsDrawTask<ChannelType>> Task = MakeShared<FMovieSceneInterpolatingPointsDrawTask<ChannelType>>(
			SharedThis(this),
			[WeakThis = AsWeak(), this](TArray<FVector2D> NewInterpolatingPoints, TArray<int32> NewKeyOffsets)
			{
				if (!WeakThis.IsValid())
				{
					return;
				}

				const FScopeLock Lock(&LockInterpolatingPoints);

				FullRangeInterpolation.Points = NewInterpolatingPoints;
				FullRangeInterpolation.KeyOffsets = NewKeyOffsets;

				uint32 Hash = 0;
				for (FVector2D& InterpolatingPoint : FullRangeInterpolation.Points)
				{
					const int32 X = FMath::RoundToInt32(InterpolatingPoint.X);
					const int32 Y = FMath::RoundToInt32(InterpolatingPoint.Y);
					Hash = HashCombine(FullRangeInterpolation.Hash, HashCombine(X, Y));
				}
				FullRangeInterpolation.Hash.store(Hash);

				Flags.store(Flags.load() | EMovieSceneCurveCacheChangeFlags::ChangedInterpolatingPoints);
			});

		FMovieSceneCurveCachePool::Get().AddTask(SharedThis(this), Task);
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::DrawInterpolatingPointsFromFullRange()
	{
		const FScopeLock Lock(&LockInterpolatingPoints);

		CachedDrawParams.InterpolatingPoints.Reset(FullRangeInterpolation.Points.Num() + 2);

		if (FullRangeInterpolation.Points.Num() < 2)
		{
			DrawPreInfinityInterpolatingPoints();
			DrawPostInfinityInterpolatingPoints();
		}
		else if (!FullRangeInterpolation.KeyOffsets.IsValidIndex(StartingIndex) &&
			!FullRangeInterpolation.KeyOffsets.IsValidIndex(EndingIndex))
		{
			const bool bHasKeyBefore = FullRangeInterpolation.KeyOffsets.IsValidIndex(StartingIndex - 1);
			if (!bHasKeyBefore)
			{
				DrawPreInfinityInterpolatingPoints();
			}

			const bool bHasKeyAfter = FullRangeInterpolation.KeyOffsets.IsValidIndex(EndingIndex + 1);

			if (!bHasKeyAfter)
			{
				DrawPostInfinityInterpolatingPoints();
			}
		}
		else
		{
			const bool bHasKeyBefore = FullRangeInterpolation.KeyOffsets.IsValidIndex(StartingIndex - 1);
			const bool bHasKeyAfter = FullRangeInterpolation.KeyOffsets.IsValidIndex(EndingIndex + 1);

			// Add the lower bound of the visible space if there is no key before
			if (!bHasKeyBefore)
			{
				DrawPreInfinityInterpolatingPoints();
			}

			if (FullRangeInterpolation.KeyOffsets.IsValidIndex(StartingIndex) && 
				FullRangeInterpolation.KeyOffsets.IsValidIndex(EndingIndex))
			{
				// Overdraw to the previous key if possible
				const int32 DataIndex = bHasKeyBefore ?
					FullRangeInterpolation.KeyOffsets[StartingIndex - 1] :
					FullRangeInterpolation.KeyOffsets[StartingIndex];

				// Overdraw to the next key if possible
				const int32 InterpolatingPointsDataSize = bHasKeyAfter ?
					FullRangeInterpolation.KeyOffsets[EndingIndex + 1] - DataIndex + 1 :
					FullRangeInterpolation.KeyOffsets[EndingIndex] - DataIndex + 1;

				// Add points in between
				check(FullRangeInterpolation.Points.IsValidIndex(DataIndex) && FullRangeInterpolation.Points.Num() >= DataIndex + InterpolatingPointsDataSize);

				const TArrayView<FVector2D> InterpolatingPointsView(FullRangeInterpolation.Points.GetData() + DataIndex, InterpolatingPointsDataSize);
				for (const FVector2D& InterpolatingPoint : InterpolatingPointsView)
				{
					CachedDrawParams.InterpolatingPoints.Emplace(
						ScreenSpace.SecondsToScreen(InterpolatingPoint.X + InputDisplayOffset),
						ScreenSpace.ValueToScreen(InterpolatingPoint.Y));
				}
			}

			// Add the upper bound of the visible space if there is no key after
			if (!bHasKeyAfter)
			{
				DrawPostInfinityInterpolatingPoints();
			}
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::DrawPreInfinityInterpolatingPoints()
	{
		if (TryDrawPreInfinityExtentFast())
		{
			return;
		}

		const FScopeLock Lock(&LockInterpolatingPoints);

		if (FullRangeInterpolation.Points.IsEmpty())
		{
			return;
		}

		if (FullRangeInterpolation.PreInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Cycle ||
			FullRangeInterpolation.PreInfinityExtrapolation == ERichCurveExtrapolation::RCCE_CycleWithOffset ||
			FullRangeInterpolation.PreInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Oscillate)
		{
			const double InfinityOffset = FullRangeInterpolation.Points[0].X + InputDisplayOffset - ScreenSpace.GetInputMin();
			if (InfinityOffset < 0.0)
			{
				// Don't draw if pre-infinity is not visible
				return;
			}

			const double Duration = FullRangeInterpolation.Points.Last().X - FullRangeInterpolation.Points[0].X;
			if (FMath::IsNearlyEqual(Duration, 0.0))
			{
				// Draw a single line in the odd case where there are many keys with a nearly zero duration.
				CachedDrawParams.InterpolatingPoints.Emplace(
					ScreenSpace.SecondsToScreen(ScreenSpace.GetInputMin()),
					ScreenSpace.ValueToScreen(FullRangeInterpolation.Points[0].Y));

				CachedDrawParams.InterpolatingPoints.Emplace(
					ScreenSpace.SecondsToScreen(FullRangeInterpolation.Points[0].X),
					ScreenSpace.ValueToScreen(FullRangeInterpolation.Points[0].Y));
			}
			else
			{
				// Cycle or oscillate
				const double StartTime = FullRangeInterpolation.Points[0].X;
				const double ValueOffset = FullRangeInterpolation.PreInfinityExtrapolation == ERichCurveExtrapolation::RCCE_CycleWithOffset ?
					FullRangeInterpolation.Points.Last().Y - FullRangeInterpolation.Points[0].Y :
					0.0;

				const int32 NumIterations = static_cast<int32>(InfinityOffset / Duration) + 1;
				for (int32 Iteration = NumIterations; Iteration > 0; Iteration--)
				{
					const bool bReverse = FullRangeInterpolation.PreInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Oscillate && Iteration % 2 != 0;

					if (bReverse)
					{
						for (int32 PointIndex = FullRangeInterpolation.Points.Num() - 1; PointIndex >= 0; PointIndex--)
						{
							const FVector2D& Point = FullRangeInterpolation.Points[PointIndex];

							// Mirror around start time
							const double Time = 2 * StartTime + Duration - Point.X - Duration * Iteration + InputDisplayOffset;
							const double Value = Point.Y - ValueOffset * Iteration;

							CachedDrawParams.InterpolatingPoints.Emplace(ScreenSpace.SecondsToScreen(Time), ScreenSpace.ValueToScreen(Value));
						}
					}
					else
					{
						for (const FVector2D& Point : FullRangeInterpolation.Points)
						{
							const double Time = Point.X - Duration * Iteration + InputDisplayOffset;
							const double Value = Point.Y - ValueOffset * Iteration;

							CachedDrawParams.InterpolatingPoints.Emplace(ScreenSpace.SecondsToScreen(Time), ScreenSpace.ValueToScreen(Value));
						}
					}
				}
			}
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::DrawPostInfinityInterpolatingPoints()
	{
		if (TryDrawPostInfinityExtentFast())
		{
			return;
		}

		const FScopeLock Lock(&LockInterpolatingPoints);

		if (FullRangeInterpolation.Points.IsEmpty())
		{
			return;
		}

		if (FullRangeInterpolation.PostInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Cycle ||
			FullRangeInterpolation.PostInfinityExtrapolation == ERichCurveExtrapolation::RCCE_CycleWithOffset ||
			FullRangeInterpolation.PostInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Oscillate)
		{
			const double InfinityOffset = ScreenSpace.GetInputMax() + InputDisplayOffset - FullRangeInterpolation.Points.Last().X;

			if (InfinityOffset < 0.0)
			{
				// Don't draw if post-infinity is not visible
				return;
			}

			const double Duration = FullRangeInterpolation.Points.Last().X - FullRangeInterpolation.Points[0].X;
			if (FMath::IsNearlyEqual(Duration, 0.0))
			{
				// Draw a single line in the odd case where there are many keys with a nearly zero duration.
				CachedDrawParams.InterpolatingPoints.Emplace(
					ScreenSpace.SecondsToScreen(FullRangeInterpolation.Points.Last().X),
					ScreenSpace.ValueToScreen(FullRangeInterpolation.Points.Last().Y));

				CachedDrawParams.InterpolatingPoints.Emplace(
					ScreenSpace.SecondsToScreen(ScreenSpace.GetInputMax()),
					ScreenSpace.ValueToScreen(FullRangeInterpolation.Points.Last().Y));
			}
			else
			{
				// Cycle or oscillate
				const double StartTime = FullRangeInterpolation.Points[0].X;
				const double ValueOffset = FullRangeInterpolation.PostInfinityExtrapolation == ERichCurveExtrapolation::RCCE_CycleWithOffset ?
					FullRangeInterpolation.Points.Last().Y - FullRangeInterpolation.Points[0].Y :
					0.0;

				const int32 NumIterations = InfinityOffset / Duration + 1;
				for (int32 Iteration = 1; Iteration <= NumIterations; Iteration++)
				{
					const bool bReverse = FullRangeInterpolation.PostInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Oscillate && Iteration % 2 != 0;

					if (bReverse)
					{
						for (int32 PointIndex = FullRangeInterpolation.Points.Num() - 1; PointIndex >= 0; PointIndex--)
						{
							const FVector2D& Point = FullRangeInterpolation.Points[PointIndex];

							// Mirror around start time
							const double Time = 2 * StartTime + Duration - Point.X + Duration * Iteration + InputDisplayOffset;
							const double Value = Point.Y - ValueOffset * Iteration;

							CachedDrawParams.InterpolatingPoints.Emplace(ScreenSpace.SecondsToScreen(Time), ScreenSpace.ValueToScreen(Value));
						}
					}
					else
					{
						for (const FVector2D& Point : FullRangeInterpolation.Points)
						{
							const double Time = Point.X + Duration * Iteration + InputDisplayOffset;
							const double Value = Point.Y + ValueOffset * Iteration;

							CachedDrawParams.InterpolatingPoints.Emplace(ScreenSpace.SecondsToScreen(Time), ScreenSpace.ValueToScreen(Value));
						}
					}
				}
			}
		}
	}

	template <typename ChannelType>
	bool FMovieSceneCachedCurve<ChannelType>::TryDrawPreInfinityExtentFast()
	{
		if (Times.IsEmpty() || Values.IsEmpty())
		{
			CachedDrawParams.InterpolatingPoints.Emplace(
				ScreenSpace.SecondsToScreen(ScreenSpace.GetInputMin()), 
				ScreenSpace.ValueToScreen(DefaultValue));

			return true;
		}
		else if (Times.Num() == 1 || Values.Num() == 1)
		{
			const double SingleValue = Values[0].Value;
			CachedDrawParams.InterpolatingPoints.Emplace(
				ScreenSpace.SecondsToScreen(ScreenSpace.GetInputMin()), 
				ScreenSpace.ValueToScreen(SingleValue));

			return true;
		}
		else
		{
			const bool bPreinfinityVisible = Times[0].Value > ScreenSpace.GetInputMin() * TickResolution;
			const bool bLinearWithoutArriveTangent =
				FullRangeInterpolation.PreInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Linear &&
				(KeyAttributes.IsEmpty() || !KeyAttributes[0].HasArriveTangent());

			if (bPreinfinityVisible &&
				(FullRangeInterpolation.PreInfinityExtrapolation == ERichCurveExtrapolation::RCCE_None ||
				FullRangeInterpolation.PreInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Constant ||
				bLinearWithoutArriveTangent))
			{
				const double ExtentValue = Values[0].Value;
				CachedDrawParams.InterpolatingPoints.Emplace(
					ScreenSpace.SecondsToScreen(ScreenSpace.GetInputMin()),
					ScreenSpace.ValueToScreen(ExtentValue));

				return true;
			}
			else if (bPreinfinityVisible &&
				FullRangeInterpolation.PreInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Linear)
			{
				const double ExtentValue = Values[0].Value + KeyAttributes[0].GetArriveTangent() * (ScreenSpace.GetInputMin() - Times[0] / TickResolution);
				CachedDrawParams.InterpolatingPoints.Emplace(
					ScreenSpace.SecondsToScreen(ScreenSpace.GetInputMin()),
					ScreenSpace.ValueToScreen(ExtentValue));

				return true;
			}
		}
		
		return false;
	}

	template <typename ChannelType>
	bool FMovieSceneCachedCurve<ChannelType>::TryDrawPostInfinityExtentFast()
	{
		if (Times.IsEmpty() || Values.IsEmpty())
		{
			CachedDrawParams.InterpolatingPoints.Emplace(
				ScreenSpace.SecondsToScreen(ScreenSpace.GetInputMax()), 
				ScreenSpace.ValueToScreen(DefaultValue));

			return true;
		}
		else if (Times.Num() == 1 || Values.Num() == 1)
		{
			const double SingleValue = Values[0].Value;
			CachedDrawParams.InterpolatingPoints.Emplace(
				ScreenSpace.SecondsToScreen(ScreenSpace.GetInputMax()),
				ScreenSpace.ValueToScreen(SingleValue));

			return true;
		}
		else
		{
			const bool bPostinfinityVisible = Times.Last().Value < ScreenSpace.GetInputMax() * TickResolution;
			const bool bLinearWithoutLeaveTangent =
				FullRangeInterpolation.PostInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Linear &&
				(KeyAttributes.IsEmpty() || !KeyAttributes.Last().HasLeaveTangent());

			if (bPostinfinityVisible &&
				(FullRangeInterpolation.PostInfinityExtrapolation == ERichCurveExtrapolation::RCCE_None ||
				FullRangeInterpolation.PostInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Constant ||
				bLinearWithoutLeaveTangent))
			{
				const double ExtentValue = Values.Last().Value;
				CachedDrawParams.InterpolatingPoints.Emplace(
					ScreenSpace.SecondsToScreen(ScreenSpace.GetInputMax()), 
					ScreenSpace.ValueToScreen(ExtentValue));

				return true;
			}
			else if (bPostinfinityVisible &&
				FullRangeInterpolation.PostInfinityExtrapolation == ERichCurveExtrapolation::RCCE_Linear)
			{
				const double ExtentValue = Values.Last().Value + KeyAttributes.Last().GetLeaveTangent() * (ScreenSpace.GetInputMax() - Times.Last() / TickResolution);
				CachedDrawParams.InterpolatingPoints.Emplace(
					ScreenSpace.SecondsToScreen(ScreenSpace.GetInputMax()),
					ScreenSpace.ValueToScreen(ExtentValue));

				return true;
			}
		}

		return false;
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::DrawKeys()
	{
		if (!Times.IsValidIndex(StartingIndex) ||
			!Times.IsValidIndex(EndingIndex) ||
			!Values.IsValidIndex(StartingIndex) ||
			!Values.IsValidIndex(EndingIndex) ||
			!KeyHandles.IsValidIndex(StartingIndex) ||
			!KeyHandles.IsValidIndex(EndingIndex) ||
			!KeyAttributes.IsValidIndex(StartingIndex) ||
			!KeyAttributes.IsValidIndex(EndingIndex) ||
			!KeyDrawInfos.IsValidIndex(StartingIndex) ||
			!KeyDrawInfos.IsValidIndex(EndingIndex))
		{
			CachedDrawParams.Points.Reset();
			return;
		}

		const int32 DataSize = EndingIndex - StartingIndex + 1;
		CachedDrawParams.Points.Reset(DataSize);

		for (int32 DataIndex = StartingIndex; DataIndex <= EndingIndex; DataIndex++)
		{
			const FKeyHandle& KeyHandle = KeyHandles[DataIndex];
			const FKeyAttributes& KeyAttribute = KeyAttributes[DataIndex];

			// Key
			FCurvePointInfo CurvePointInfo(KeyHandles[DataIndex]);

			CurvePointInfo.Type = ECurvePointType::Key;
			CurvePointInfo.LayerBias = 2;

			CurvePointInfo.ScreenPosition.X = ScreenSpace.SecondsToScreen(Times[DataIndex] / TickResolution + InputDisplayOffset);
			CurvePointInfo.ScreenPosition.Y = ScreenSpace.ValueToScreen(Values[DataIndex].Value);
			CurvePointInfo.DrawInfo = KeyDrawInfos[DataIndex].template Get<0>();
			CurvePointInfo.LineDelta = FVector2D::ZeroVector;

			CachedDrawParams.Points.Add(CurvePointInfo);

			// Arrive Tangent
			{
				if (KeyAttribute.HasArriveTangent() &&
					KeyDrawInfos[DataIndex].template Get<1>().IsSet())
				{
					FCurvePointInfo ArriveTangentInfo(KeyHandle);

					ArriveTangentInfo.Type = ECurvePointType::ArriveTangent;
					ArriveTangentInfo.LayerBias = 1;

					const float ArriveTangent = KeyAttribute.GetArriveTangent();

					if (KeyAttribute.HasTangentWeightMode() && KeyAttribute.HasArriveTangentWeight() &&
						(KeyAttribute.GetTangentWeightMode() == RCTWM_WeightedBoth || KeyAttribute.GetTangentWeightMode() == RCTWM_WeightedArrive))
					{
						FVector2D TangentOffset = ::CurveEditor::ComputeScreenSpaceTangentOffset(ScreenSpace, ArriveTangent, -KeyAttribute.GetArriveTangentWeight());
						ArriveTangentInfo.ScreenPosition = CurvePointInfo.ScreenPosition + TangentOffset;
					}
					else
					{
						const double DisplayRatio = (ScreenSpace.PixelsPerOutput() / ScreenSpace.PixelsPerInput());

						float PixelLength = 60.0f;
						ArriveTangentInfo.ScreenPosition = CurvePointInfo.ScreenPosition + ::CurveEditor::GetVectorFromSlopeAndLength(ArriveTangent * -DisplayRatio, -PixelLength);
					}

					ArriveTangentInfo.DrawInfo = KeyDrawInfos[DataIndex].template Get<1>().GetValue();
					ArriveTangentInfo.LineDelta = CurvePointInfo.ScreenPosition - ArriveTangentInfo.ScreenPosition;

					CachedDrawParams.Points.Add(ArriveTangentInfo);
				}
			}

			// Leave Tangent
			{
				if (KeyAttribute.HasLeaveTangent() &&
					KeyDrawInfos[DataIndex].template Get<2>().IsSet())
				{
					FCurvePointInfo LeaveTangentInfo(KeyHandle);

					LeaveTangentInfo.Type = ECurvePointType::LeaveTangent;
					LeaveTangentInfo.LayerBias = 1;

					const float LeaveTangent = KeyAttribute.GetLeaveTangent();

					if (KeyAttribute.HasTangentWeightMode() && KeyAttribute.HasLeaveTangentWeight() &&
						(KeyAttribute.GetTangentWeightMode() == RCTWM_WeightedBoth || KeyAttribute.GetTangentWeightMode() == RCTWM_WeightedLeave))
					{
						FVector2D TangentOffset = ::CurveEditor::ComputeScreenSpaceTangentOffset(ScreenSpace, LeaveTangent, KeyAttribute.GetLeaveTangentWeight());
						LeaveTangentInfo.ScreenPosition = CurvePointInfo.ScreenPosition + TangentOffset;
					}
					else
					{
						const double DisplayRatio = (ScreenSpace.PixelsPerOutput() / ScreenSpace.PixelsPerInput());

						float PixelLength = 60.0f;
						LeaveTangentInfo.ScreenPosition = CurvePointInfo.ScreenPosition + ::CurveEditor::GetVectorFromSlopeAndLength(LeaveTangent * -DisplayRatio, PixelLength);
					}

					LeaveTangentInfo.DrawInfo = KeyDrawInfos[DataIndex].template Get<2>().GetValue();
					LeaveTangentInfo.LineDelta = CurvePointInfo.ScreenPosition - LeaveTangentInfo.ScreenPosition;

					CachedDrawParams.Points.Add(LeaveTangentInfo);
				}
			}

			if (DataIndex == EndingIndex)
			{
				break;
			}
		}
	}

	template <typename ChannelType>
	void FMovieSceneCachedCurve<ChannelType>::ApplyDrawParams()
	{
		if (FCurveDrawParams* CurveDrawParamsPtr = DrawParamsHandle.Get())
		{
			*CurveDrawParamsPtr = CachedDrawParams;
		}
	}
}
