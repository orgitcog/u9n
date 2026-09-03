// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGSplineStruct.h"

#include "Metadata/PCGMetadataCommon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSplineStruct)

namespace PCGSplineStruct
{
	static int32 UpperBound(const TArray<FInterpCurvePoint<FVector>>& SplinePoints, float Value)
	{
		int32 Count = SplinePoints.Num();
		int32 First = 0;

		while (Count > 0)
		{
			const int32 Middle = Count / 2;
			if (Value >= SplinePoints[First + Middle].InVal)
			{
				First += Middle + 1;
				Count -= Middle + 1;
			}
			else
			{
				Count = Middle;
			}
		}

		return First;
	}

	// Note: copied verbatim from USplineComponent::CalcBounds
	static FBoxSphereBounds CalcBounds(const FSplineCurves& SplineCurves, bool bClosedLoop, const FTransform& LocalToWorld)
	{
//#if SPLINE_FAST_BOUNDS_CALCULATION
//		FBox BoundingBox(0);
//		for (const auto& InterpPoint : SplineCurves.Position.Points)
//		{
//			BoundingBox += InterpPoint.OutVal;
//		}
//
//		return FBoxSphereBounds(BoundingBox.TransformBy(LocalToWorld));
//#else
		const int32 NumPoints = SplineCurves.Position.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;

		FVector Min(WORLD_MAX);
		FVector Max(-WORLD_MAX);
		if (NumSegments > 0)
		{
			for (int32 Index = 0; Index < NumSegments; Index++)
			{
				const bool bLoopSegment = (Index == NumPoints - 1);
				const int32 NextIndex = bLoopSegment ? 0 : Index + 1;
				const FInterpCurvePoint<FVector>& ThisInterpPoint = SplineCurves.Position.Points[Index];
				FInterpCurvePoint<FVector> NextInterpPoint = SplineCurves.Position.Points[NextIndex];
				if (bLoopSegment)
				{
					NextInterpPoint.InVal = ThisInterpPoint.InVal + SplineCurves.Position.LoopKeyOffset;
				}

				CurveVectorFindIntervalBounds(ThisInterpPoint, NextInterpPoint, Min, Max);
			}
		}
		else if (NumPoints == 1)
		{
			Min = Max = SplineCurves.Position.Points[0].OutVal;
		}
		else
		{
			Min = FVector::ZeroVector;
			Max = FVector::ZeroVector;
		}

		return FBoxSphereBounds(FBox(Min, Max).TransformBy(LocalToWorld));
//#endif
	}
}

void FPCGSplineStruct::Initialize(const USplineComponent* InSplineComponent)
{
	check(InSplineComponent);
	SplineCurves = InSplineComponent->GetSplineCurves();
	Transform = InSplineComponent->GetComponentTransform();
	DefaultUpVector = InSplineComponent->DefaultUpVector;
	ReparamStepsPerSegment = InSplineComponent->ReparamStepsPerSegment;
	bClosedLoop = InSplineComponent->IsClosedLoop();

	Bounds = InSplineComponent->Bounds;
	LocalBounds = InSplineComponent->CalcLocalBounds();

	ControlPointsEntryKeys.Empty();
}

void FPCGSplineStruct::Initialize(const TArray<FSplinePoint>& InSplinePoints, bool bIsClosedLoop, const FTransform& InTransform, TArray<PCGMetadataEntryKey> InOptionalEntryKeys)
{
	Transform = InTransform;
	DefaultUpVector = FVector::ZAxisVector;
	ReparamStepsPerSegment = 10; // default value in USplineComponent

	bClosedLoop = bIsClosedLoop;
	AddPoints(InSplinePoints, true);

	Bounds = PCGSplineStruct::CalcBounds(SplineCurves, bClosedLoop, InTransform);
	LocalBounds = PCGSplineStruct::CalcBounds(SplineCurves, bClosedLoop, FTransform::Identity);

	if (!InOptionalEntryKeys.IsEmpty() && InSplinePoints.Num() == InOptionalEntryKeys.Num())
	{
		ControlPointsEntryKeys = std::move(InOptionalEntryKeys);
	}
	else
	{
		// If we have a mismatch, we can't set the entry keys, so reset it
		ControlPointsEntryKeys.Empty();
	}
}

void FPCGSplineStruct::ApplyTo(USplineComponent* InSplineComponent) const
{
	check(InSplineComponent);

	InSplineComponent->ClearSplinePoints(false);
	InSplineComponent->SetWorldTransform(Transform);
	InSplineComponent->DefaultUpVector = DefaultUpVector;
	InSplineComponent->ReparamStepsPerSegment = ReparamStepsPerSegment;

	InSplineComponent->SetSpline(SplineCurves);
	InSplineComponent->bStationaryEndpoints = false;
	// TODO: metadata? might not be needed
	InSplineComponent->SetClosedLoop(bClosedLoop);
	InSplineComponent->UpdateSpline();
	InSplineComponent->UpdateBounds();
}

void FPCGSplineStruct::AddPoint(const FSplinePoint& InSplinePoint, bool bUpdateSpline)
{
	const int32 Index = PCGSplineStruct::UpperBound(SplineCurves.Position.Points, InSplinePoint.InputKey);

	SplineCurves.Position.Points.Insert(FInterpCurvePoint<FVector>(
		InSplinePoint.InputKey,
		InSplinePoint.Position,
		InSplinePoint.ArriveTangent,
		InSplinePoint.LeaveTangent,
		ConvertSplinePointTypeToInterpCurveMode(InSplinePoint.Type)
		),
		Index);

	SplineCurves.Rotation.Points.Insert(FInterpCurvePoint<FQuat>(
		InSplinePoint.InputKey,
		InSplinePoint.Rotation.Quaternion(),
		FQuat::Identity,
		FQuat::Identity,
		CIM_CurveAuto
		),
		Index);

	SplineCurves.Scale.Points.Insert(FInterpCurvePoint<FVector>(
		InSplinePoint.InputKey,
		InSplinePoint.Scale,
		FVector::ZeroVector,
		FVector::ZeroVector,
		CIM_CurveAuto
		),
		Index);

	if (!ControlPointsEntryKeys.IsEmpty())
	{
		ControlPointsEntryKeys.Insert(PCGInvalidEntryKey, Index);
	}

	if (bUpdateSpline)
	{
		UpdateSpline();
	}
}

void FPCGSplineStruct::AddPoints(const TArray<FSplinePoint>& InSplinePoints, bool bUpdateSpline)
{
	SplineCurves.Position.Points.Reserve(SplineCurves.Position.Points.Num() + InSplinePoints.Num());
	SplineCurves.Rotation.Points.Reserve(SplineCurves.Rotation.Points.Num() + InSplinePoints.Num());
	SplineCurves.Scale.Points.Reserve(SplineCurves.Scale.Points.Num() + InSplinePoints.Num());
	
	if (!ControlPointsEntryKeys.IsEmpty())
	{
		ControlPointsEntryKeys.Reserve(ControlPointsEntryKeys.Num() + InSplinePoints.Num());
	}

	for (const auto& SplinePoint : InSplinePoints)
	{
		AddPoint(SplinePoint, false);
	}

	if (bUpdateSpline)
	{
		UpdateSpline();
	}
}

void FPCGSplineStruct::UpdateSpline()
{
	const bool bLoopPositionOverride = false;
	const bool bStationaryEndpoints = false;
	const float LoopPosition = 0.0f;

	SplineCurves.UpdateSpline(bClosedLoop, bStationaryEndpoints, ReparamStepsPerSegment, bLoopPositionOverride, LoopPosition, Transform.GetScale3D());
}

int FPCGSplineStruct::GetNumberOfSplineSegments() const
{
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	return (bClosedLoop ? NumPoints : NumPoints - 1);
}

int FPCGSplineStruct::GetNumberOfPoints() const
{
	return SplineCurves.Position.Points.Num();
}

FVector::FReal FPCGSplineStruct::GetSplineLength() const
{
	return SplineCurves.GetSplineLength();
}

FBox FPCGSplineStruct::GetBounds() const
{
	// See USplineComponent::CalcBounds
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;

	FVector Min(WORLD_MAX);
	FVector Max(-WORLD_MAX);
	if (NumSegments > 0)
	{
		for (int32 Index = 0; Index < NumSegments; Index++)
		{
			const bool bLoopSegment = (Index == NumPoints - 1);
			const int32 NextIndex = bLoopSegment ? 0 : Index + 1;
			const FInterpCurvePoint<FVector>& ThisInterpPoint = SplineCurves.Position.Points[Index];
			FInterpCurvePoint<FVector> NextInterpPoint = SplineCurves.Position.Points[NextIndex];
			if (bLoopSegment)
			{
				NextInterpPoint.InVal = ThisInterpPoint.InVal /* + SplineCurves.Position.LoopKeyOffset*/;
			}

			CurveVectorFindIntervalBounds(ThisInterpPoint, NextInterpPoint, Min, Max);
		}
	}
	else if (NumPoints == 1)
	{
		Min = Max = SplineCurves.Position.Points[0].OutVal;
	}
	else
	{
		Min = FVector::ZeroVector;
		Max = FVector::ZeroVector;
	}

	return FBox(Min, Max);
}

const FInterpCurveVector& FPCGSplineStruct::GetSplinePointsScale() const
{
	return SplineCurves.Scale;
}

const FInterpCurveVector& FPCGSplineStruct::GetSplinePointsPosition() const
{
	return SplineCurves.Position;
}

const FInterpCurveFloat& FPCGSplineStruct::GetSplineRepramTable() const
{
	return SplineCurves.ReparamTable;
}

const FInterpCurveQuat& FPCGSplineStruct::GetSplinePointsRotation() const
{
	return SplineCurves.Rotation;
}

FVector::FReal FPCGSplineStruct::GetDistanceAlongSplineAtSplinePoint(int32 PointIndex) const
{
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;

	// Ensure that if the reparam table is not prepared yet we don't attempt to access it. This can happen
	// early in the construction of the spline component object.
	if ((PointIndex >= 0) && (PointIndex < NumSegments + 1) && ((PointIndex * ReparamStepsPerSegment) < SplineCurves.ReparamTable.Points.Num()))
	{
		return SplineCurves.ReparamTable.Points[PointIndex * ReparamStepsPerSegment].InVal;
	}

	return 0.0f;
}

FVector FPCGSplineStruct::GetLocationAtDistanceAlongSpline(FVector::FReal Distance, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineCurves.ReparamTable.Eval(Distance, 0.0f);
	return GetLocationAtSplineInputKey(Param, CoordinateSpace);
}

FTransform FPCGSplineStruct::GetTransformAtDistanceAlongSpline(FVector::FReal Distance, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale) const
{
	const float Param = SplineCurves.ReparamTable.Eval(Distance, 0.0f);
	return GetTransformAtSplineInputKey(Param, CoordinateSpace, bUseScale);
}

FVector FPCGSplineStruct::GetRightVectorAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const FQuat Quat = GetQuaternionAtSplineInputKey(InKey, ESplineCoordinateSpace::Local);
	FVector RightVector = Quat.RotateVector(FVector::RightVector);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		RightVector = Transform.TransformVectorNoScale(RightVector);
	}

	return RightVector;
}

FTransform FPCGSplineStruct::GetTransformAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale) const
{
	const FVector Location(GetLocationAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));
	const FQuat Rotation(GetQuaternionAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));
	const FVector Scale = bUseScale ? GetScaleAtSplineInputKey(InKey) : FVector(1.0f);

	FTransform KeyTransform(Rotation, Location, Scale);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		KeyTransform = KeyTransform * Transform;
	}

	return KeyTransform;
}

float FPCGSplineStruct::FindInputKeyClosestToWorldLocation(const FVector& WorldLocation) const
{
	const FVector LocalLocation = Transform.InverseTransformPosition(WorldLocation);
	float Dummy;
	return SplineCurves.Position.InaccurateFindNearest(LocalLocation, Dummy);
}

TTuple<int, float> FPCGSplineStruct::GetSegmentStartIndexAndKeyAtInputKey(float InKey) const
{
	const int32 Index = SplineCurves.Position.GetPointIndexForInputValue(InKey);
	return {Index, GetInputKeyAtSegmentStart(Index)};
}

float FPCGSplineStruct::GetInputKeyAtSegmentStart(int InSegmentIndex) const
{
	if (IsClosedLoop() && !SplineCurves.Position.Points.IsEmpty() && InSegmentIndex >= SplineCurves.Position.Points.Num())
	{
		// In case of a closed loop, the last point is the first point, and the input key is the last point + LoopKeyOffset
		return SplineCurves.Position.Points.Last().InVal + SplineCurves.Position.LoopKeyOffset;
	}
	else
	{
		return SplineCurves.Position.Points.IsValidIndex(InSegmentIndex) ? SplineCurves.Position.Points[InSegmentIndex].InVal : 0.0f;
	}
}

void FPCGSplineStruct::AllocateMetadataEntries()
{
	// Add robustness to cleanup everything if we have a mismatch between the number of points and the number of entry keys
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	if (ControlPointsEntryKeys.Num() != NumPoints)
	{
		ControlPointsEntryKeys.Empty(NumPoints);
	}
	
	if (ControlPointsEntryKeys.IsEmpty())
	{
		ControlPointsEntryKeys.Reserve(NumPoints);
		for (int32 i = 0; i < NumPoints; ++i)
		{
			ControlPointsEntryKeys.Add(PCGInvalidEntryKey);
		}
	}
}

FVector FPCGSplineStruct::GetLocationAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FVector Location = SplineCurves.Position.Eval(InKey, FVector::ZeroVector);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Location = Transform.TransformPosition(Location);
	}

	return Location;
}

FQuat FPCGSplineStruct::GetQuaternionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FQuat Quat = SplineCurves.Rotation.Eval(InKey, FQuat::Identity);
	Quat.Normalize();

	const FVector Direction = SplineCurves.Position.EvalDerivative(InKey, FVector::ZeroVector).GetSafeNormal();
	const FVector UpVector = Quat.RotateVector(DefaultUpVector);

	FQuat Rot = (FRotationMatrix::MakeFromXZ(Direction, UpVector)).ToQuat();

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Rot = Transform.GetRotation() * Rot;
	}

	return Rot;
}

FVector FPCGSplineStruct::GetScaleAtSplineInputKey(float InKey) const
{
	return SplineCurves.Scale.Eval(InKey, FVector(1.0f));
}

/** Taken from USplineComponent::ConvertSplineSegmentToPolyLine. */
bool FPCGSplineStruct::ConvertSplineSegmentToPolyLine(int32 SplinePointStartIndex, ESplineCoordinateSpace::Type CoordinateSpace, const float MaxSquareDistanceFromSpline, TArray<FVector>& OutPoints) const
{
	OutPoints.Empty();
	
	TArray<double> DummyDistances;

    const double StartDist = GetDistanceAlongSplineAtSplinePoint(SplinePointStartIndex);
    const double StopDist = GetDistanceAlongSplineAtSplinePoint(SplinePointStartIndex + 1);

    const int32 NumLines = 2; // Dichotomic subdivision of the spline segment
    double Dist = StopDist - StartDist;
    double SubstepSize = Dist / NumLines;
    if (SubstepSize == 0.0)
    {
    	// There is no distance to cover, so handle the segment with a single point
    	OutPoints.Add(GetLocationAtDistanceAlongSpline(StopDist, CoordinateSpace));
    	return true;
    }

    double SubstepStartDist = StartDist;
    for (int32 i = 0; i < NumLines; ++i)
    {
    	double SubstepEndDist = SubstepStartDist + SubstepSize;
    	TArray<FVector> NewPoints;
    	DummyDistances.Reset();
    	// Recursively sub-divide each segment until the requested precision is reached :
    	if (DivideSplineIntoPolylineRecursiveWithDistancesHelper(SubstepStartDist, SubstepEndDist, CoordinateSpace, MaxSquareDistanceFromSpline, NewPoints, DummyDistances))
    	{
    		if (OutPoints.Num() > 0)
    		{
    			check(OutPoints.Last() == NewPoints[0]); // our last point must be the same as the new segment's first
    			OutPoints.RemoveAt(OutPoints.Num() - 1);
    		}
    		OutPoints.Append(NewPoints);
    	}

    	SubstepStartDist = SubstepEndDist;
    }

    return (OutPoints.Num() > 0);
}

/** Taken from USplineComponent::ConvertSplineToPolyLine. */
bool FPCGSplineStruct::ConvertSplineToPolyLine(ESplineCoordinateSpace::Type CoordinateSpace, const float MaxSquareDistanceFromSpline, TArray<FVector>& OutPoints) const
{
	int32 NumSegments = GetNumberOfSplineSegments();
	OutPoints.Empty();
	OutPoints.Reserve(NumSegments * 2); // We sub-divide each segment in at least 2 sub-segments, so let's start with this amount of points

	TArray<FVector> SegmentPoints;
	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		if (ConvertSplineSegmentToPolyLine(SegmentIndex, CoordinateSpace, MaxSquareDistanceFromSpline, SegmentPoints))
		{
			if (OutPoints.Num() > 0)
			{
				check(OutPoints.Last() == SegmentPoints[0]); // our last point must be the same as the new segment's first
				OutPoints.RemoveAt(OutPoints.Num() - 1);
			}
			OutPoints.Append(SegmentPoints);
		}
	}

	return (OutPoints.Num() > 0);
}

/** Taken from USplineComponent::DivideSplineIntoPolylineRecursiveWithDistancesHelper. */
bool FPCGSplineStruct::DivideSplineIntoPolylineRecursiveWithDistancesHelper(float StartDistanceAlongSpline, float EndDistanceAlongSpline, ESplineCoordinateSpace::Type CoordinateSpace, const float MaxSquareDistanceFromSpline, TArray<FVector>& OutPoints, TArray<double>& OutDistancesAlongSpline) const
{
	double Dist = EndDistanceAlongSpline - StartDistanceAlongSpline;
	if (Dist <= 0.0f)
	{
		return false;
	}
	double MiddlePointDistancAlongSpline = StartDistanceAlongSpline + Dist / 2.0f;
	FVector Samples[3];
	Samples[0] = GetLocationAtDistanceAlongSpline(StartDistanceAlongSpline, CoordinateSpace);
	Samples[1] = GetLocationAtDistanceAlongSpline(MiddlePointDistancAlongSpline, CoordinateSpace);
	Samples[2] = GetLocationAtDistanceAlongSpline(EndDistanceAlongSpline, CoordinateSpace);

	if (FMath::PointDistToSegmentSquared(Samples[1], Samples[0], Samples[2]) > MaxSquareDistanceFromSpline)
	{
		TArray<FVector> NewPoints[2];
		TArray<double> NewDistancesAlongSpline[2];
		DivideSplineIntoPolylineRecursiveWithDistancesHelper(StartDistanceAlongSpline, MiddlePointDistancAlongSpline, CoordinateSpace, MaxSquareDistanceFromSpline, NewPoints[0], NewDistancesAlongSpline[0]);
		DivideSplineIntoPolylineRecursiveWithDistancesHelper(MiddlePointDistancAlongSpline, EndDistanceAlongSpline, CoordinateSpace, MaxSquareDistanceFromSpline, NewPoints[1], NewDistancesAlongSpline[1]);
		if ((NewPoints[0].Num() > 0) && (NewPoints[1].Num() > 0))
		{
			check(NewPoints[0].Last() == NewPoints[1][0]);
			check(NewDistancesAlongSpline[0].Last() == NewDistancesAlongSpline[1][0]);
			NewPoints[0].RemoveAt(NewPoints[0].Num() - 1);
			NewDistancesAlongSpline[0].RemoveAt(NewDistancesAlongSpline[0].Num() - 1);
		}
		NewPoints[0].Append(NewPoints[1]);
		NewDistancesAlongSpline[0].Append(NewDistancesAlongSpline[1]);
		OutPoints.Append(NewPoints[0]);
		OutDistancesAlongSpline.Append(NewDistancesAlongSpline[0]);
	}
	else
	{
		// The middle point is close enough to the other 2 points, let's keep those and stop the recursion :
		OutPoints.Add(Samples[0]);
		OutDistancesAlongSpline.Add(StartDistanceAlongSpline);
		// For a constant spline, the end can be the exact same as the start; in this case, just add the point once
		if (Samples[0] != Samples[2])
		{
			OutPoints.Add(Samples[2]);
			OutDistancesAlongSpline.Add(EndDistanceAlongSpline);
		}
		
	}

	check(OutPoints.Num() == OutDistancesAlongSpline.Num())
	return (OutPoints.Num() > 0);
}