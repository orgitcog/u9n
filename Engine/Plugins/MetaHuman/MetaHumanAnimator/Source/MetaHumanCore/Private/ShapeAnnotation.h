// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ShapeAnnotation
{
	enum class EVertexStyle
	{
		Smooth,
		Sharp
	};

	enum class ECurveConnectionDirection
	{
		ToStartOfOtherCurve,
		ToEndOfOtherCurve
	};
	
	struct FPoint2D
	{
		FPoint2D()
			: X(0.0)
			, Y(0.0)
		{
		}

		FPoint2D(double InX, double InY)
			: X(InX)
			, Y(InY)
		{
		}

		double Length() const { return FMath::Sqrt(X * X + Y * Y); }
		double LengthSquared() const { return X * X + Y * Y; }
		double X;
		double Y;

		FPoint2D operator+(const FPoint2D& InOther) const
		{
			return FPoint2D(X + InOther.X, Y + InOther.Y);
		}

		FPoint2D operator-(const FPoint2D& InOther) const
		{
			return FPoint2D(X - InOther.X, Y - InOther.Y);
		}

		FPoint2D operator*(double InScalar) const
		{
			return FPoint2D(X * InScalar, Y * InScalar);
		}

		FPoint2D operator/(double InScalar) const
		{
			return FPoint2D(X / InScalar, Y / InScalar);
		}

		friend FPoint2D operator*(double InScalar, const FPoint2D& InPoint);
	};

	inline FPoint2D operator*(double InScalar, const FPoint2D& InPoint)
	{
		return FPoint2D(InPoint.X * InScalar, InPoint.Y * InScalar);
	}

	struct FKeypoint
	{
		FPoint2D Pos{0., 0.};
		EVertexStyle Style = EVertexStyle::Sharp;
		bool bVisible = false;
	};

	struct FKeypointCurve
	{
		FString StarKeypointName;
		FString EndKeypointName;
		TArray<FPoint2D> InternalPoints;
	};
	
	struct FCurveConnection
	{
		FString OtherCurveName;
		ECurveConnectionDirection Direction;
	};
	
	class FShapeAnnotation
	{
	public:

		void Initialize(const TMap<FString, FKeypoint>& InKeyPoints, const TMap<FString, FKeypointCurve>& InKeypointCurves);
		void InsertInternalPoint(const FString& InName, int32 InsertBefore, const FPoint2D InPoint);
		void RemoveInternalPoint(const FString& InName, int32 InIndex);

		TMap<FString, TArray<FPoint2D>> GetDrawingSplines(const TMap<FString, int32>& InPointsPerSpline) const;
		const TMap<FString, FKeypoint>& GetKeypoints() const;
		const TMap<FString, FKeypointCurve>& GetKeypointCurves() const;
		
		TMap<FString, FKeypoint>& GetKeypointsRef();
		TMap<FString, FKeypointCurve>& GetKeypointCurvesRef();

	private:

		TArray<FPoint2D> GetDensePoints(const int32 InImageWidth, const int32 InImageHeight, TArray<TArray<int32>>& OutInboundLinks, TArray<TArray<int32>> &OutOutboundLinks,
			TMap<FString, TArray<int32>>& OutCurveLookup, TMap<FString, int32>& OutKeypointLookup, const TMap<FString, int32>& InInternalDensities) const;

		FPoint2D FirstPointBeforeEnd(const FString& InCurveName) const;
		FPoint2D FirstPointAfterStart(const FString& InCurveName) const;
		FPoint2D DummyFirstPoint(const FString& InCurveName) const;
		FPoint2D DummyLastPoint(const FString& InCurveName) const;

		FCurveConnection IncomingConnection(const FString& InCurveName) const;
		FCurveConnection OutgoingConnection(const FString& InCurveName) const;

		TArray<FPoint2D> ApproximateOpenCatmullromSpline(const TArray<FPoint2D>& InExtendedPoints, int32 InNumOutPoints, int32 InResolution) const;
		FPoint2D CatmullromPointOnCurve(FPoint2D InA, FPoint2D InB, FPoint2D InC, FPoint2D InD, double InT, double InAlpha = 0.5) const;
		TArray<double> LinearRange(double InA, double InB, int32 InN) const;
		TArray<FPoint2D> SpreadPointsEvenly(const TArray<FPoint2D>& InPoints, int32 InNOutPoints) const;

		TMap<FString, FKeypoint> Keypoints;
		TMap<FString, FKeypointCurve> KeypointCurves;
	};
}
