// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCleanSpline.h"

#include "PCGContext.h"
#include "Data/PCGSplineData.h"

#define LOCTEXT_NAMESPACE "PCGCleanSplineElement"

namespace PCGCleanSplineHelpers
{
	bool VectorsAreCollinear(const FVector& FirstVector, const FVector& SecondVector, const double Threshold)
	{
		return FMath::Abs(FirstVector.GetSafeNormal() | SecondVector.GetSafeNormal()) >= Threshold;
	}

	bool ControlPointsAreColocated(const FSplinePoint& Point1, const FSplinePoint& Point2, const FTransform& SplineTransform, const double Threshold, const bool bUseLocalSpace = false)
	{
		const FVector Location1 = bUseLocalSpace ? Point1.Position : SplineTransform.TransformPosition(Point1.Position);
		const FVector Location2 = bUseLocalSpace ? Point2.Position : SplineTransform.TransformPosition(Point2.Position);
		return FVector::DistSquared(Location1, Location2) < Threshold * Threshold;
	}

	// Implementation note: Co-located points will have a vector dot product of zero, regardless of tangents, and will be thus collinear.
	bool ControlPointsAreCollinear(const FSplinePoint& Point1, const FSplinePoint& Point2, const FSplinePoint& Point3, const double Threshold)
	{
		const FVector Segment = Point3.Position - Point1.Position;

		/* Need to check all four tangents against the segment to guarantee collinearity. Note: This works for linear
		 * segments only. It is possible to find more control points on curves that would have no effect on the final
		 * result, but it would be extremely rare for a user to wind up in that situation. It would also provide little
		 * to no benefit to do so as well.
		 */
		return (
			VectorsAreCollinear(Point1.LeaveTangent, Segment, Threshold) &&
			VectorsAreCollinear(Point2.ArriveTangent, Segment, Threshold) &&
			VectorsAreCollinear(Point2.LeaveTangent, Segment, Threshold) &&
			VectorsAreCollinear(Point3.ArriveTangent, Segment, Threshold)
		);
	}
}

FPCGElementPtr UPCGCleanSplineSettings::CreateElement() const
{
	return MakeShared<FPCGCleanSplineElement>();
}

#if WITH_EDITOR
void UPCGCleanSplineSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGCleanSplineSettings, bUseRadians))
		{
			CollinearAngleThreshold = bUseRadians ? FMath::DegreesToRadians(CollinearAngleThreshold) : FMath::RadiansToDegrees(CollinearAngleThreshold);
		}
		else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGCleanSplineSettings, CollinearAngleThreshold))
		{
			static constexpr double MaxCollinearAngleToleranceDegrees = 89;
			CollinearAngleThreshold = FMath::Clamp(CollinearAngleThreshold, 0, bUseRadians ? FMath::DegreesToRadians(MaxCollinearAngleToleranceDegrees) : MaxCollinearAngleToleranceDegrees);
		}
	}
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGCleanSplineSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spline).SetRequiredPin();
	return Properties;
}

TArray<FPCGPinProperties> UPCGCleanSplineSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spline);
	return Properties;
}

bool FPCGCleanSplineElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCleanSplineElement::Execute);

	check(InContext);

	const UPCGCleanSplineSettings* Settings = InContext->GetInputSettings<UPCGCleanSplineSettings>();
	check(Settings);

	// Nothing to do. Forward the output.
	if (!Settings->bFuseColocatedControlPoints && !Settings->bRemoveCollinearControlPoints)
	{
		InContext->OutputData = InContext->InputData;
		PCGLog::LogWarningOnGraph(LOCTEXT("NoOperation", "No Clean Spline operations selected. Input will be forwarded"), InContext);
		return true;
	}

	// Pre-calculate the tolerance value from the user defined tolerance in radians/degrees to cross-product comparable value.
	const double DotProductToleranceRad = Settings->bUseRadians ? Settings->CollinearAngleThreshold : FMath::DegreesToRadians(Settings->CollinearAngleThreshold);
	// Max the tolerance with an epsilon to accomodate rounding errors.
	const double DotProductTolerance = FMath::Max(FMath::Abs(FMath::Cos(DotProductToleranceRad)), UE_DOUBLE_SMALL_NUMBER);

	for (const FPCGTaggedData& InputData : InContext->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		const UPCGSplineData* InputSplineData = Cast<const UPCGSplineData>(InputData.Data);
		if (!InputSplineData || InputSplineData->GetNumSegments() < 1)
		{
			continue;
		}

		const FTransform& SplineTransform = InputSplineData->SplineStruct.Transform;
		const FInterpCurveVector& ControlPointsPosition = InputSplineData->SplineStruct.GetSplinePointsPosition();
		const FInterpCurveQuat& ControlPointsRotation = InputSplineData->SplineStruct.GetSplinePointsRotation();
		const FInterpCurveVector& ControlPointsScale = InputSplineData->SplineStruct.GetSplinePointsScale();
		const TConstArrayView<PCGMetadataEntryKey> ControlPointsMetadataEntry = InputSplineData->SplineStruct.GetConstControlPointsEntryKeys();
		const int32 NumControlPoints = ControlPointsPosition.Points.Num();
		bool bControlPointWasRemoved = false;
		const bool bIsClosed = InputSplineData->IsClosed();

		TArray<FSplinePoint> ControlPoints;
		ControlPoints.Reserve(NumControlPoints);

		// Making sure that the number of metadata entry keys matches, if not, we are in an invalid state and will reset the metadata entry keys.
		TArray<PCGMetadataEntryKey> ControlPointsKeys;
		if (ControlPointsMetadataEntry.Num() == NumControlPoints)
		{
			ControlPointsKeys.Append(ControlPointsMetadataEntry);
		}

		// For code clarity, generate the points first and remove them as needed.
		for (int32 i = 0; i < NumControlPoints; ++i)
		{
			/* Implementation note: Decay to custom tangents. They assist the user in building the spline, but the
			 * interpolation modes will affect the recalculations unpredictably when control points are removed.
			 */
			ControlPoints.Emplace(/*InputKey=*/i,
			   ControlPointsPosition.Points[i].OutVal,
			   ControlPointsPosition.Points[i].ArriveTangent,
			   ControlPointsPosition.Points[i].LeaveTangent,
			   ControlPointsRotation.Points[i].OutVal.Rotator(),
			   ControlPointsScale.Points[i].OutVal,
			   ESplinePointType::CurveCustomTangent);
		}

		auto RemovePoint = [&ControlPoints, &ControlPointsKeys, &bControlPointWasRemoved](int Index)
		{
			if (!ControlPointsKeys.IsEmpty())
			{
				check(ControlPointsKeys.IsValidIndex(Index));
				ControlPointsKeys.RemoveAt(Index, EAllowShrinking::No);
			}

			check(ControlPoints.IsValidIndex(Index));
			ControlPoints.RemoveAt(Index, EAllowShrinking::No);
			bControlPointWasRemoved = true;
		};

		auto GetPreviousIndexFromOffset = [&ControlPoints](int32 Index, int32 Offset)
		{
			if (!ensure(!ControlPoints.IsEmpty()))
			{
				return 0;
			}
			
			int32 Result = Index - Offset;
			while (Result < 0)
			{
				Result += ControlPoints.Num();
			}
			
			return Result;
		};

		if (Settings->bFuseColocatedControlPoints)
		{
			const int32 MinIndex = bIsClosed ? 0 : 1;
			
			// Will evaluate by pairs. Reverse order for optimizing RemoveAt.
			for (int Index = ControlPoints.Num() - 1; Index >= MinIndex; --Index)
			{
				const int32 PreviousPointIndex = GetPreviousIndexFromOffset(Index, 1);
				const int32 CurrentPointIndex = Index;
				
				// Evaluate by pairs for colocated.
				if (PCGCleanSplineHelpers::ControlPointsAreColocated(
						ControlPoints[PreviousPointIndex],
						ControlPoints[CurrentPointIndex],
						SplineTransform,
						Settings->ColocationDistanceThreshold,
						Settings->bUseSplineLocalSpace))
				{
					EPCGControlPointFuseMode FuseMode = Settings->FuseMode;
					// Generally, keep the previous control point (first), but preserve the final control point to maintain the spline's length (second) in non-closed splines.
					if (FuseMode == EPCGControlPointFuseMode::Auto)
					{
						FuseMode = (!bIsClosed && CurrentPointIndex == ControlPoints.Num() - 1) ? EPCGControlPointFuseMode::KeepSecond : EPCGControlPointFuseMode::KeepFirst;
					}

					switch (FuseMode)
					{
						// Keep the first control point
						case EPCGControlPointFuseMode::KeepFirst:
							ControlPoints[PreviousPointIndex].LeaveTangent = ControlPoints[CurrentPointIndex].LeaveTangent;
							RemovePoint(CurrentPointIndex);
							break;
						// Keep the second control point
						case EPCGControlPointFuseMode::KeepSecond:
							ControlPoints[CurrentPointIndex].ArriveTangent = ControlPoints[PreviousPointIndex].ArriveTangent;
							RemovePoint(PreviousPointIndex);
							break;
						// Average the two control points' transforms and update the leave tangent
						case EPCGControlPointFuseMode::Merge:
							ControlPoints[PreviousPointIndex].Position = FMath::Lerp(ControlPoints[PreviousPointIndex].Position, ControlPoints[CurrentPointIndex].Position, 0.5);
							ControlPoints[PreviousPointIndex].Rotation = FMath::Lerp(ControlPoints[PreviousPointIndex].Rotation, ControlPoints[CurrentPointIndex].Rotation, 0.5);
							ControlPoints[PreviousPointIndex].Scale = FMath::Lerp(ControlPoints[PreviousPointIndex].Scale, ControlPoints[CurrentPointIndex].Scale, 0.5);
							ControlPoints[PreviousPointIndex].LeaveTangent = ControlPoints[CurrentPointIndex].LeaveTangent;
							RemovePoint(CurrentPointIndex);
							break;
						case EPCGControlPointFuseMode::Auto: // Should've been picked up by now. Fallthrough...
							checkNoEntry();
						default:
							break;
					}
				}
			}
		}

		if (Settings->bRemoveCollinearControlPoints)
		{
			const int32 MinIndex = bIsClosed ? 0 : 2;
			
			// Will evaluate by triplets. Reverse order for optimizing RemoveAt.
			for (int Index = ControlPoints.Num() - 1; Index >= MinIndex; --Index)
			{
				const int32 SecondPreviousPointIndex = GetPreviousIndexFromOffset(Index, 2);
				const int32 PreviousPointIndex = GetPreviousIndexFromOffset(Index, 1);
				const int32 CurrentPointIndex = Index;
				
				if (PCGCleanSplineHelpers::ControlPointsAreCollinear(
						ControlPoints[SecondPreviousPointIndex],
						ControlPoints[PreviousPointIndex],
						ControlPoints[CurrentPointIndex],
						DotProductTolerance))
				{
					ControlPoints[SecondPreviousPointIndex].LeaveTangent = ControlPoints[CurrentPointIndex].Position - ControlPoints[SecondPreviousPointIndex].Position;
					ControlPoints[CurrentPointIndex].ArriveTangent = ControlPoints[SecondPreviousPointIndex].LeaveTangent;
					RemovePoint(PreviousPointIndex);
				}
			}
		}

		FPCGTaggedData& Output = InContext->OutputData.TaggedData.Emplace_GetRef(InputData);

		// We only need to create a new data if a point was removed.
		if (bControlPointWasRemoved)
		{
			// Update input keys if a point was removed, to keep them monotonically incremental
			for (int i = 0; i < ControlPoints.Num(); ++i)
			{
				ControlPoints[i].InputKey = i;
			}
			
			UPCGSplineData* NewSplineData = FPCGContext::NewObject_AnyThread<UPCGSplineData>(InContext);
			NewSplineData->InitializeFromData(InputSplineData);
			NewSplineData->Initialize(ControlPoints, InputSplineData->IsClosed(), InputSplineData->GetTransform(), std::move(ControlPointsKeys));

			Output.Data = NewSplineData;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
