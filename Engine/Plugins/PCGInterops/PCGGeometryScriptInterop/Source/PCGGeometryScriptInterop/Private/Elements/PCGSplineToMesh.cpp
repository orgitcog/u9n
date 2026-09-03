// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSplineToMesh.h"

#include "PCGContext.h"
#include "Data/PCGDynamicMeshData.h"
#include "Data/PCGSplineData.h"

#include "CurveOps/TriangulateCurvesOp.h"

#define LOCTEXT_NAMESPACE "PCGSplineToMeshElement"

#if WITH_EDITOR
FName UPCGSplineToMeshSettings::GetDefaultNodeName() const
{
	return FName(TEXT("SplineToMesh"));
}

FText UPCGSplineToMeshSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Spline To Mesh");
}

FText UPCGSplineToMeshSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Converts a closed spline into a mesh.");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGSplineToMeshSettings::CreateElement() const
{
	return MakeShared<FPCGSplineToMeshElement>();
}

TArray<FPCGPinProperties> UPCGSplineToMeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spline).SetRequiredPin();	
	return Properties;
}

bool FPCGSplineToMeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSplineToMeshElement::Execute);

	check(InContext);

	const UPCGSplineToMeshSettings* Settings = InContext->GetInputSettings<UPCGSplineToMeshSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = InContext->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSplineData* InputSplineData = Cast<const UPCGSplineData>(Input.Data);
		if (!InputSplineData)
		{
			PCGLog::InputOutput::LogTypedDataNotFoundWarning(EPCGDataType::Spline, PCGPinConstants::DefaultInputLabel, InContext);
			continue;
		}

		UE::Geometry::FTriangulateCurvesOp TriangulateCurvesOp;
		TriangulateCurvesOp.Thickness = Settings->Thickness;
		TriangulateCurvesOp.bFlipResult = Settings->bFlipResult;
		TriangulateCurvesOp.FlattenMethod = Settings->FlattenMethod;
		TriangulateCurvesOp.CurveOffset = Settings->CurveOffset;
		TriangulateCurvesOp.OffsetOpenMethod = Settings->OpenCurves;
		TriangulateCurvesOp.OffsetJoinMethod = Settings->JoinMethod;
		TriangulateCurvesOp.OpenEndShape = Settings->EndShapes;
		TriangulateCurvesOp.MiterLimit = Settings->MiterLimit;
		TriangulateCurvesOp.bFlipResult = Settings->bFlipResult;
		
		if (Settings->CurveOffset == 0.0)
		{
			TriangulateCurvesOp.OffsetClosedMethod = EOffsetClosedCurvesMethod::DoNotOffset;
		}
		else
		{
			TriangulateCurvesOp.OffsetClosedMethod = Settings->OffsetClosedCurves;
		}
		
		TArray<FVector> SplinePoints;
		InputSplineData->SplineStruct.ConvertSplineToPolyLine(ESplineCoordinateSpace::World, Settings->ErrorTolerance, SplinePoints);
		TriangulateCurvesOp.AddWorldCurve(SplinePoints, InputSplineData->IsClosed(), InputSplineData->SplineStruct.Transform);
		TriangulateCurvesOp.CalculateResult(nullptr);

		TUniquePtr<UE::Geometry::FDynamicMesh3>	DynamicMesh = TriangulateCurvesOp.ExtractResult();

		if (!DynamicMesh || DynamicMesh->TriangleCount() == 0)
		{
			PCGLog::LogErrorOnGraph(LOCTEXT("TriangulationFailed", "Triangulation failed"), InContext);
			continue;
		}
		
		UE::Geometry::FDynamicMesh3* DynamicMeshPtr = DynamicMesh.Release();
		UPCGDynamicMeshData* DynamicMeshData = FPCGContext::NewObject_AnyThread<UPCGDynamicMeshData>(InContext);
		DynamicMeshData->Initialize(std::move(*DynamicMeshPtr));

		InContext->OutputData.TaggedData.Emplace_GetRef(Input).Data = DynamicMeshData;
	}
	
	return true;
}

#undef LOCTEXT_NAMESPACE
