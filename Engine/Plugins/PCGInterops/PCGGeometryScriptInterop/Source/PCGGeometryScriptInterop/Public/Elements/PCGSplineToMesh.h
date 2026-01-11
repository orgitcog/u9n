// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/PCGDynamicMeshBaseElement.h"

#include "CurveOps/TriangulateCurvesOp.h"

#include "PCGSplineToMesh.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGSplineToMeshSettings : public UPCGDynamicMeshBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
    virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
	
public:
	// This is a 1:1 match with the Geometry Script struct options for this operation

	/** How far to allow the triangulation boundary can deviate from the spline curve before we add more vertices */
	UPROPERTY(EditAnywhere, Category = Spline, meta = (ClampMin = 0.001))
	double ErrorTolerance = 1.0;
	
	/** Whether and how to flatten the curves. If curves are flattened, they can also be offset. */
	UPROPERTY(EditAnywhere, Category = Spline)
	EFlattenCurveMethod FlattenMethod = EFlattenCurveMethod::DoNotFlatten;

	/** If > 0, Extrude the triangulation by this amount. */
	UPROPERTY(EditAnywhere, Category = Mesh, meta = (ClampMin = 0.0))
	double Thickness = 0.0;

	/** Whether to flip the facing direction of the generated mesh. */
	UPROPERTY(EditAnywhere, Category = Mesh)
	bool bFlipResult = false;

	/** How to handle open curves: Either offset them, or treat them as closed curves. */
	UPROPERTY(EditAnywhere, Category = Offset, meta = (EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten", EditConditionHides))
	EOffsetOpenCurvesMethod OpenCurves = EOffsetOpenCurvesMethod::TreatAsClosed;

	/** How much offset to apply to curves. */
	UPROPERTY(EditAnywhere, Category = Offset, meta = (EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten", EditConditionHides))
	double CurveOffset = 0.0;

	/** Whether and how to apply offset to closed curves. */
	UPROPERTY(EditAnywhere, Category = Offset, meta = (EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten && CurveOffset != 0", EditConditionHides))
	EOffsetClosedCurvesMethod OffsetClosedCurves = EOffsetClosedCurvesMethod::OffsetOuterSide;

	/** The shape of the ends of offset curves. */
	UPROPERTY(EditAnywhere, Category = Offset, meta = (EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten && OpenCurves != EOffsetOpenCurvesMethod::TreatAsClosed && CurveOffset != 0", EditConditionHides))
	EOpenCurveEndShapes EndShapes = EOpenCurveEndShapes::Square;

	/** The shape of joins between segments of an offset curve. */
	UPROPERTY(EditAnywhere, Category = Offset, meta = (EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten && CurveOffset != 0", EditConditionHides))
	EOffsetJoinMethod JoinMethod = EOffsetJoinMethod::Square;

	/** How far a miter join can extend before it is replaced by a square join. */
	UPROPERTY(EditAnywhere, Category = Offset, meta = (ClampMin = 0.0, EditCondition = "FlattenMethod != EFlattenCurveMethod::DoNotFlatten && CurveOffset != 0 && JoinMethod == EOffsetJoinMethod::Miter", EditConditionHides))
	double MiterLimit = 1.0;
};

class FPCGSplineToMeshElement : public IPCGDynamicMeshBaseElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

