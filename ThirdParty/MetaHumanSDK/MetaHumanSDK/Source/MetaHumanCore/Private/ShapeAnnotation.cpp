// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShapeAnnotation.h"

#include "Engine/InterpCurveEdSetup.h"
#include "Math/UnrealMathUtility.h"

using namespace ShapeAnnotation;

void FShapeAnnotation::Initialize(const TMap<FString, FKeypoint>& InKeyPoints, const TMap<FString, FKeypointCurve>& InKeypointCurves)
{
	Keypoints = InKeyPoints;
	KeypointCurves = InKeypointCurves;
}

void FShapeAnnotation::InsertInternalPoint(const FString& InName, int32 InsertBefore, const FPoint2D InPoint)
{
	TArray<FPoint2D>& Pts = KeypointCurves[InName].InternalPoints;
	if (InsertBefore > Pts.Num())
	{
		Pts.Add(InPoint);
	}
	else
	{
		Pts.Insert(InPoint, InsertBefore);
	}
}

void FShapeAnnotation::RemoveInternalPoint(const FString& InName, int32 InIndex)
{
	TArray<FPoint2D>& Points = KeypointCurves[InName].InternalPoints;
	Points.RemoveAt(InIndex);
}

TMap<FString, TArray<FPoint2D>> FShapeAnnotation::GetDrawingSplines(const TMap<FString, int32>& InPointsPerSpline) const
{
	TMap<FString, TArray<FPoint2D>> Result;
	TArray<TArray<int32>> InboundLinks;
	TArray<TArray<int32>> OutboundLinks;
	TMap<FString, TArray<int32>> CurveLookup;
	TMap<FString, int32> KeypointLookup;
	TArray<FPoint2D> Splines = GetDensePoints(1, 1, InboundLinks, OutboundLinks, CurveLookup, KeypointLookup, InPointsPerSpline);

	int32 Count = Keypoints.Num();

	for (const TPair<FString, FKeypointCurve>& Item : KeypointCurves)
	{
		TArray<FPoint2D> ThisSplineInternals;
		ThisSplineInternals.Reserve(InPointsPerSpline[Item.Key]);
		for (int32 i = Count; i < Count + InPointsPerSpline[Item.Key]; ++i)
		{
			ThisSplineInternals.EmplaceAt((i - Count), Splines[i]);
		}

		Result.FindOrAdd(Item.Key).Add(Keypoints[Item.Value.StarKeypointName].Pos);
		Result[Item.Key].Append(ThisSplineInternals);
		Result[Item.Key].Add(Keypoints[Item.Value.EndKeypointName].Pos);

		Count += InPointsPerSpline[Item.Key];
	}

	return Result;
}

const TMap<FString, FKeypoint>& FShapeAnnotation::GetKeypoints() const
{
	return Keypoints;
}

const TMap<FString, FKeypointCurve>& FShapeAnnotation::GetKeypointCurves() const
{
	return KeypointCurves;
}

TMap<FString, FKeypoint>& FShapeAnnotation::GetKeypointsRef()
{
	return Keypoints;
}

TMap<FString, FKeypointCurve>& FShapeAnnotation::GetKeypointCurvesRef()
{
	return KeypointCurves;
}

TArray<FPoint2D> FShapeAnnotation::GetDensePoints(const int32 InImageWidth, const int32 InImageHeight,
	TArray<TArray<int32>>& OutInboundLinks, TArray<TArray<int32>>& OutOutboundLinks, TMap<FString, TArray<int32>>& OutCurveLookup,
	TMap<FString, int32>& OutKeypointLookup, const TMap<FString, int32>& InInternalDensities) const
{
	TArray<FPoint2D> DenseShape;
    int32 SumDensities = 0;
	
    // Calculate the sum of densities
    for (const TPair<FString, int32>& Item : InInternalDensities)
    {
        SumDensities += Item.Value;
    }

    DenseShape.Reserve(SumDensities + Keypoints.Num());  // This might be too big, but won't be too small
    TMap<FString, int32> KeypointIndsInDense;
    TMap<FString, TArray<int32>> InternalPointIndsInDense;

    int32 DensePointCount = 0;

    // Add keypoints to DenseShape
    for (const TTuple<FString, FKeypoint>& Item : Keypoints)
    {
        DenseShape.Emplace(Item.Value.Pos);
        KeypointIndsInDense.Add(Item.Key, DensePointCount);
        OutKeypointLookup.Add(Item.Key, DensePointCount);
        DensePointCount++;
    }

    // Add keypoint curves
    for (const TPair<FString, FKeypointCurve>& Curve : KeypointCurves)
    {
	    int32 NumInternals = InInternalDensities[Curve.Key];
    	if (NumInternals == Curve.Value.InternalPoints.Num())
    	{
    		TArray<int32> TheseInds;
    		TheseInds.SetNum(NumInternals);
    		for (int32 P = 0; P < NumInternals; ++P)
    		{
    			DenseShape.Emplace(Curve.Value.InternalPoints[P]);
    			TheseInds[P] = DensePointCount++;
    		}
    		InternalPointIndsInDense.Add(Curve.Key, TheseInds);
    	}
    	else
    	{
    		TArray<FPoint2D> ExtendedControlPoints;
    		ExtendedControlPoints.Reserve(Curve.Value.InternalPoints.Num() + 4);

    		FPoint2D StartExtension;

    		if (Keypoints.Find(Curve.Value.StarKeypointName)->Style == EVertexStyle::Smooth)
    		{
    			if (Curve.Value.StarKeypointName == Curve.Value.EndKeypointName) // Closed curve
    			{
    				StartExtension = FirstPointBeforeEnd(Curve.Key);
    			}
    			else
    			{
    				FCurveConnection Incoming = IncomingConnection(Curve.Key);
    				if (!Incoming.OtherCurveName.IsEmpty())
    				{
    					bool bConnectedToEnd = (Incoming.Direction == ECurveConnectionDirection::ToEndOfOtherCurve);
    					StartExtension = bConnectedToEnd ? FirstPointBeforeEnd(Incoming.OtherCurveName) : FirstPointAfterStart(Incoming.OtherCurveName);
    				}
    				else
    				{
    					StartExtension = DummyFirstPoint(Curve.Key);
    				}
    			}
    		}
    		else
    		{
    			StartExtension = DummyFirstPoint(Curve.Key);
    		}

    		FPoint2D EndExtension;

    		if (Keypoints.Find(Curve.Value.EndKeypointName)->Style == EVertexStyle::Smooth)
    		{
    			if (Curve.Value.StarKeypointName == Curve.Value.EndKeypointName) // Closed curve
    			{
    				EndExtension = FirstPointAfterStart(Curve.Key);
    			}
    			else
    			{
    				FCurveConnection Outgoing = OutgoingConnection(Curve.Key);
    				if (!Outgoing.OtherCurveName.IsEmpty())
    				{
    					bool ConnectedToStart = (Outgoing.Direction == ECurveConnectionDirection::ToStartOfOtherCurve);
    					EndExtension = ConnectedToStart ? FirstPointAfterStart(Outgoing.OtherCurveName) : FirstPointBeforeEnd(Outgoing.OtherCurveName);
    				}
    				else
    				{
    					EndExtension = DummyLastPoint(Curve.Key);
    				}
    			}
    		}
    		else
    		{
    			EndExtension = DummyLastPoint(Curve.Key);
    		}

    		ExtendedControlPoints.Emplace(StartExtension);
    		ExtendedControlPoints.Emplace(Keypoints.Find(Curve.Value.StarKeypointName)->Pos);
    		for (const FPoint2D& Pt : Curve.Value.InternalPoints)
    		{
    			ExtendedControlPoints.Emplace(Pt);
    		}
    		ExtendedControlPoints.Emplace(Keypoints.Find(Curve.Value.EndKeypointName)->Pos);
    		ExtendedControlPoints.Emplace(EndExtension);

    		auto DenseWithEnds = ApproximateOpenCatmullromSpline(ExtendedControlPoints, NumInternals + 2, 5);
    		TArray<int32> TheseInds;
    		TheseInds.Reserve(DenseWithEnds.Num() - 2);

    		for (int32 i = 1; i < DenseWithEnds.Num() - 1; ++i)
    		{
    			DenseShape.Emplace(DenseWithEnds[i]);
    			TheseInds.EmplaceAt(i-1, DensePointCount++);
    		}

    		InternalPointIndsInDense.FindOrAdd(Curve.Key, TheseInds);
    	}
    }
	
    // Set the links for each point
    OutInboundLinks.Empty();
    OutInboundLinks.Reserve(SumDensities + Keypoints.Num());
    OutOutboundLinks.Empty();
    OutOutboundLinks.Reserve(SumDensities + Keypoints.Num());

    // Add inbound and outbound links for each keypoint
    for (const auto& Item : Keypoints)
    {
        TArray<int32> TheseInboundLinks;
        TArray<int32> TheseOutboundLinks;

        for (const TPair<FString, FKeypointCurve>& CurveItem : KeypointCurves)
        {
            const TArray<int32>& InternalInds = InternalPointIndsInDense[CurveItem.Key];
            if (CurveItem.Value.StarKeypointName == Item.Key)
            {
                TheseOutboundLinks.Emplace(InternalInds[0]);
            }
            if (CurveItem.Value.EndKeypointName == Item.Key)
            {
                TheseInboundLinks.Emplace(InternalInds.Last());
            }
        }
        OutInboundLinks.Emplace(TheseInboundLinks);
        OutOutboundLinks.Emplace(TheseOutboundLinks);
    }

    // Add the curve links
    for (const auto& CurveItem : KeypointCurves)
    {
        const auto& InternalInds = InternalPointIndsInDense[CurveItem.Key];
        for (int32 I = 0; I < InternalInds.Num(); ++I)
        {
            if (I == 0)
            {
                OutInboundLinks.Emplace(TArray<int32>{ KeypointIndsInDense[CurveItem.Value.StarKeypointName] });
                OutOutboundLinks.Emplace(TArray<int32>{ InternalInds[1] });
            }
            else if (I == InternalInds.Num() - 1)
            {
                OutInboundLinks.Emplace(TArray<int32>{ InternalInds[I - 1] });
                OutOutboundLinks.Emplace(TArray<int32>{ KeypointIndsInDense[CurveItem.Value.EndKeypointName] });
            }
            else
            {
                OutInboundLinks.Emplace(TArray<int32>{ InternalInds[I - 1] });
                OutOutboundLinks.Emplace(TArray<int32>{ InternalInds[I + 1] });
            }
        }

        TArray<int32> ThisCurveLookup;
        ThisCurveLookup.Emplace(KeypointIndsInDense[CurveItem.Value.StarKeypointName]);
        ThisCurveLookup.Append(InternalInds);
        ThisCurveLookup.Emplace(KeypointIndsInDense[CurveItem.Value.EndKeypointName]);
        OutCurveLookup.Add(CurveItem.Key, ThisCurveLookup);
    }

    // Scale the points by the image dimensions
    for (auto& Pt : DenseShape)
    {
        Pt.X *= InImageWidth;
        Pt.Y *= InImageHeight;
    }

    return DenseShape;
}

FPoint2D FShapeAnnotation::FirstPointBeforeEnd(const FString& InCurveName) const
{
	FPoint2D Result = {};
	if (KeypointCurves.Contains(InCurveName))
	{
		FKeypointCurve Curve = KeypointCurves[InCurveName];
		if (Curve.InternalPoints.IsEmpty())
		{
			FString StartPointName = KeypointCurves[InCurveName].StarKeypointName;
			Result = Keypoints[StartPointName].Pos;
		}
		else
		{
			Result = Curve.InternalPoints.Last();
		}
	}

	return Result;
}

FPoint2D FShapeAnnotation::FirstPointAfterStart(const FString& InCurveName) const
{
	FPoint2D Result = {};
	if (KeypointCurves.Contains(InCurveName))
	{
		FKeypointCurve Curve = KeypointCurves[InCurveName];
		if (Curve.InternalPoints.IsEmpty())
		{
			FString StartPointName = KeypointCurves[InCurveName].EndKeypointName;
			Result = Keypoints[StartPointName].Pos;
		}
		else
		{
			Result = Curve.InternalPoints[0];
		}
	}

	return Result;
}

FPoint2D FShapeAnnotation::DummyFirstPoint(const FString& InCurveName) const
{
	FString StartKeyPointName = KeypointCurves[InCurveName].StarKeypointName;
	FPoint2D A = Keypoints[StartKeyPointName].Pos;
	FPoint2D B = FirstPointAfterStart(InCurveName);
	
	return A - (B - A);
}

FPoint2D FShapeAnnotation::DummyLastPoint(const FString& InCurveName) const
{
	const FString EndKeyPointName = KeypointCurves[InCurveName].EndKeypointName;
	FPoint2D A = Keypoints[EndKeyPointName].Pos;
	FPoint2D B = FirstPointBeforeEnd(InCurveName);
	
	return A + (A - B);
}

FCurveConnection FShapeAnnotation::IncomingConnection(const FString& InCurveName) const
{
	FCurveConnection Result = {};

	const FString IncomingKeypointName = KeypointCurves[InCurveName].StarKeypointName;
	for (const TPair<FString, FKeypointCurve>& OtherKeypointCurve : KeypointCurves)
	{
		if (OtherKeypointCurve.Key == InCurveName)
		{
			continue;
		}
		if (OtherKeypointCurve.Value.EndKeypointName == IncomingKeypointName)
		{
			Result.OtherCurveName = OtherKeypointCurve.Key;
			Result.Direction = ECurveConnectionDirection::ToEndOfOtherCurve;
			break;
		}
		else if (OtherKeypointCurve.Value.StarKeypointName == IncomingKeypointName)
		{
			Result.OtherCurveName = OtherKeypointCurve.Key;
			Result.Direction = ECurveConnectionDirection::ToStartOfOtherCurve;
			break;
		}
	}

	return Result;
}

FCurveConnection FShapeAnnotation::OutgoingConnection(const FString& InCurveName) const
{
	FCurveConnection Result = {};

	const FString OutgoingKeypointName = KeypointCurves[InCurveName].EndKeypointName;
	for (const TPair<FString, FKeypointCurve>& OtherKeypointCurve : KeypointCurves)
	{
		if (OtherKeypointCurve.Key == InCurveName)
		{
			continue;
		}
		if (OtherKeypointCurve.Value.StarKeypointName == OutgoingKeypointName)
		{
			Result.OtherCurveName = OtherKeypointCurve.Key;
			Result.Direction = ECurveConnectionDirection::ToStartOfOtherCurve;
			break;
		}
		if (OtherKeypointCurve.Value.StarKeypointName == OutgoingKeypointName)
		{
			Result.OtherCurveName = OtherKeypointCurve.Key;
			Result.Direction = ECurveConnectionDirection::ToEndOfOtherCurve;
			break;
		}
	}

	return Result;
}

TArray<FPoint2D> FShapeAnnotation::ApproximateOpenCatmullromSpline(const TArray<FPoint2D>& InExtendedPoints, int32 InNumOutPoints, int32 InResolution) const
{
	int32 ApproxResolution = InNumOutPoints * InResolution;
	int32 NRealControlPoints = InExtendedPoints.Num() - 2;
	int32 NRealSections = InExtendedPoints.Num() - 3;
	
	TArray<double> P2PDistance;
	P2PDistance.SetNum(NRealSections);

	for (int32 Section = 0; Section < NRealSections; ++Section)
	{
		double Distance = (InExtendedPoints[Section + 2] - InExtendedPoints[Section + 1]).Length();

		if (Distance == 0)
		{
			Distance = UE_SMALL_NUMBER;
		}
		if (Section == 0)
		{
			P2PDistance[Section] = Distance;
		}
		else
		{
			P2PDistance[Section] = Distance + P2PDistance[Section - 1];
		}
	}

	TArray<int32> NInternalDensePtsPerSection;
	NInternalDensePtsPerSection.SetNum(NRealSections);
	int32 TotalInternalPoints = 0;

	for (int32 Section = 0; Section < NRealSections; ++Section)
	{
		NInternalDensePtsPerSection[Section] = static_cast<int32>(ApproxResolution * P2PDistance[Section] / P2PDistance.Last());
		TotalInternalPoints += NInternalDensePtsPerSection[Section];
	}

	int32 PtCount = 0;
	TArray<FPoint2D> DenseSpline;
	DenseSpline.SetNum(TotalInternalPoints + NRealControlPoints);
	for (int32 Section = 0; Section < NRealSections; ++Section)
	{
		FPoint2D Pt_A = InExtendedPoints[Section];
		FPoint2D Pt_B = InExtendedPoints[Section + 1];
		FPoint2D Pt_C = InExtendedPoints[Section + 2];
		FPoint2D Pt_D = InExtendedPoints[Section + 3];

		TArray<double> Values = LinearRange(0.0, 1.0, NInternalDensePtsPerSection[Section] + 2);
		if (Section != NRealSections - 1)
		{
			Values.Pop(); //Unless we're on the last section, don't use t=1.0 because this will
			//be the first point of the next section
		}
		for (const double& Pt : Values)
		{
			auto Point = CatmullromPointOnCurve(Pt_A, Pt_B, Pt_C, Pt_D, Pt);
			DenseSpline[PtCount++] = Point;
		}
	}

	return SpreadPointsEvenly(DenseSpline, InNumOutPoints);
}

FPoint2D FShapeAnnotation::CatmullromPointOnCurve(FPoint2D InA, FPoint2D InB, FPoint2D InC, FPoint2D InD, double InT, double InAlpha) const
{
	const double T0 = 0.0;
	const double T1 = T0 + FMath::Pow((InB - InA).LengthSquared(), 0.5f * InAlpha);
	const double T2 = T1 + FMath::Pow((InC - InB).LengthSquared(), 0.5f * InAlpha);
	const double T3 = T2 + FMath::Pow((InD - InC).LengthSquared(), 0.5f * InAlpha);

	if (FMath::Abs(T1 - T0) < UE_SMALL_NUMBER || FMath::Abs(T2 - T1) < UE_SMALL_NUMBER || 
		FMath::Abs(T3 - T2) < UE_SMALL_NUMBER || FMath::Abs(T2 - T0) < UE_SMALL_NUMBER || 
		FMath::Abs(T3 - T1) < UE_SMALL_NUMBER)
	{
		return InB;
	}

	InT = T1 + InT * (T2 - T1);

	FPoint2D A1 = (T1 - InT) / (T1 - T0) * InA + (InT - T0) / (T1 - T0) * InB;
	FPoint2D A2 = (T2 - InT) / (T2 - T1) * InB + (InT - T1) / (T2 - T1) * InC;
	FPoint2D A3 = (T3 - InT) / (T3 - T2) * InC + (InT - T2) / (T3 - T2) * InD;
	FPoint2D B1 = (T2 - InT) / (T2 - T0) * A1 + (InT - T0) / (T2 - T0) * A2;
	FPoint2D B2 = (T3 - InT) / (T3 - T1) * A2 + (InT - T1) / (T3 - T1) * A3;

	return (T2 - InT) / (T2 - T1) * B1 + (InT - T1) / (T2 - T1) * B2;
}

TArray<double> FShapeAnnotation::LinearRange(double InA, double InB, int32 InN) const
{
	TArray<double> Result;
	Result.SetNum(InN);

	const double StepSize = (InB - InA) / (InN - 1);
	Result[0] = InA;

	for (int32 Ct = 1; Ct < InN; ++Ct)
	{
		Result[Ct] = Result[Ct - 1] + StepSize;
	}

	return Result;
}

TArray<FPoint2D> FShapeAnnotation::SpreadPointsEvenly(const TArray<FPoint2D>& InPoints, int32 InNOutPoints) const
{
	TArray<FPoint2D> Result;

	if (!InPoints.IsEmpty())
	{
		TArray<double> CombinedDistance;
		CombinedDistance.SetNum(InPoints.Num());
        
		// Compute cumulative distances
		for (int32 P = 1; P < InPoints.Num(); P++)
		{
			CombinedDistance[P] = CombinedDistance[P - 1] + (InPoints[P] - InPoints[P - 1]).Length();
		}

		// Get target distances
		TArray<double> TargetDistances = LinearRange(0.0, CombinedDistance.Last(), InNOutPoints);

		int32 IndexOfLastUpperBound = 0;
		Result.SetNum(TargetDistances.Num());
		Result[0] = InPoints[0];
		Result[InNOutPoints - 1] = InPoints.Last();

		for (int32 Op = 1; Op < InNOutPoints - 1; ++Op)
		{
			int32 IndexOfUpperBound = IndexOfLastUpperBound;
			while (IndexOfUpperBound < InPoints.Num())
			{
				if (CombinedDistance[IndexOfUpperBound] >= TargetDistances[Op])
					break;
				++IndexOfUpperBound;
			}

			int32 IndexOfLowerBound = IndexOfUpperBound - 1;

			// Handle case of only 1 input point
			if (IndexOfLowerBound < 0)
			{
				Result[Op] = InPoints[IndexOfUpperBound];
			}
			else
			{
				const double GapToLowerBound = TargetDistances[Op] - CombinedDistance[IndexOfLowerBound];
				const double GapToUpperBound = CombinedDistance[IndexOfUpperBound] - TargetDistances[Op];
				const double LowerWeight = GapToUpperBound / (GapToLowerBound + GapToUpperBound);
				const double UpperWeight = 1.0f - LowerWeight;

				Result[Op] = LowerWeight * InPoints[IndexOfLowerBound] + UpperWeight * InPoints[IndexOfUpperBound];
			}

			IndexOfLastUpperBound = IndexOfUpperBound;
		}
	}

	return Result;
}
