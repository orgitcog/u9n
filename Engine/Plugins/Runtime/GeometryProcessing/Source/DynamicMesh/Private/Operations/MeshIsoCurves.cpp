// Copyright Epic Games, Inc. All Rights Reserved.

#include "Operations/MeshIsoCurves.h"

#include "Async/ParallelFor.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Operations/LocalPlanarSimplify.h"

using namespace UE::Geometry;

void FMeshIsoCurves::Cut(FDynamicMesh3& Mesh, TFunctionRef<float(int32)> VertexFn, TFunctionRef<float(int32 VIDMin, int32 VIDMax, float ValMin, float ValMax)> EdgeCutFn, float IsoValue)
{
	int MaxVID = Mesh.MaxVertexID();
	TArray<float> VertexValues;
	VertexValues.SetNumUninitialized(MaxVID);

	constexpr bool bNoParallel = false;
	ParallelFor(MaxVID, [this, &VertexValues, &Mesh, &VertexFn, IsoValue](int32 VID)
	{
		if (Mesh.IsVertex(VID))
		{
			VertexValues[VID] = VertexFn(VID);
		}
		else
		{
			// set IsoValue on invalid vertices; any vertex that is later inserted and uses this ID will be on the curve, so should have this value
			VertexValues[VID] = IsoValue;
		}
	}, bNoParallel);

	TSet<int> OnCutEdges;
	SplitCrossingEdges(Mesh, VertexValues, OnCutEdges, EdgeCutFn, IsoValue);

	// collapse degenerate edges
	if (Settings.bCollapseDegenerateEdgesOnCut)
	{
		FLocalPlanarSimplify::CollapseDegenerateEdges(Mesh, OnCutEdges, false, Settings.DegenerateEdgeTol);
	}

}

void FMeshIsoCurves::SplitCrossingEdges(FDynamicMesh3& Mesh, const TArray<float>& VertexValues,
	TSet<int32>& OnCutEdges,
	TFunctionRef<float(int32 VIDMin, int32 VIDMax, float ValMin, float ValMax)> EdgeCutFn,
	float IsoValue)
{
	OnCutEdges.Reset();

	// have to skip processing of new edges. If edge id
	// is > max at start, is new. Otherwise if in NewEdges list, also new.
	int MaxEID = Mesh.MaxEdgeID();
	TSet<int> NewEdgesBeforeMaxID;
	auto AddNewEdge = [&NewEdgesBeforeMaxID, MaxEID](int32 NewEID)
	{
		if (NewEID < MaxEID)
		{
			NewEdgesBeforeMaxID.Add(NewEID);
		}
	};

	double UseSnapVertTol = FMath::Max(0, Settings.SnapToExistingVertexTol);
	const double SnapExistingTolSq = UseSnapVertTol * UseSnapVertTol;

	// split existing edges where the value crosses isovalue
	for (int32 EID = 0; EID < MaxEID; ++EID)
	{
		if (!Mesh.IsEdge(EID) || NewEdgesBeforeMaxID.Contains(EID))
		{
			continue;
		}

		FIndex2i EdgeV = Mesh.GetEdgeV(EID);
		const float ValueA = VertexValues[EdgeV.A];
		const float ValueB = VertexValues[EdgeV.B];
		const float DistA = ValueA - IsoValue;
		const float DistB = ValueB - IsoValue;

		// If both Signs are 0, this edge is on-contour
		// If one sign is 0, that vertex is on-contour
		int AOnCurve = (FMathd::Abs(DistA) <= Settings.CurveIsoValueSnapTolerance) ? 1 : 0;
		int BOnCurve = (FMathd::Abs(DistB) <= Settings.CurveIsoValueSnapTolerance) ? 1 : 0;
		if (AOnCurve || BOnCurve)
		{
			continue;
		}

		// no crossing
		if (DistA * DistB >= 0)
		{
			continue;
		}

		double Param = EdgeCutFn(EdgeV.A, EdgeV.B, ValueA, ValueB);
		// Cut must be within edge
		if (Param <= 0 || Param >= 1)
		{
			continue;
		}
		// Skip the edge split if we're within tolerance of an existing vertex
		if (SnapExistingTolSq > 0)
		{
			FVector3d PosA = Mesh.GetVertex(EdgeV.A);
			FVector3d PosB = Mesh.GetVertex(EdgeV.B);
			FVector3d EdgeVec = PosB - PosA;
			double MinSepSq = (EdgeVec * (Param > .5 ? 1 - Param : Param)).SquaredLength();
			if (MinSepSq <= SnapExistingTolSq)
			{
				continue;
			}
		}

		FDynamicMesh3::FEdgeSplitInfo SplitInfo;
		EMeshResult SplitResult = Mesh.SplitEdge(EID, SplitInfo, Param);
		if (!ensureMsgf(SplitResult == EMeshResult::Ok, TEXT("FMeshIsoCurves::SplitCrossingEdges: failed to SplitEdge")))
		{
			continue; // edge split really shouldn't fail; skip the edge if it somehow does
		}

		AddNewEdge(SplitInfo.NewEdges.A);
		AddNewEdge(SplitInfo.NewEdges.B);

		// We need to check whether the other vertices are on curve to decide if the connected edges are on the curve or not
		int32 OtherVIDA = SplitInfo.OtherVertices.A;
		// Other vertex is on curve if it's newly created or within curve isovalue tolerance
		// (Note a newly-created vertex w/ ID < VertexValues.Num() will also have a VertexValue of IsoValue, since we use this as the default value)
		if (OtherVIDA >= VertexValues.Num() || FMath::Abs(VertexValues[OtherVIDA] - IsoValue) <= Settings.CurveIsoValueSnapTolerance)
		{
			OnCutEdges.Add(SplitInfo.NewEdges.B);
		}
		
		if (SplitInfo.NewEdges.C != FDynamicMesh3::InvalidID)
		{
			AddNewEdge(SplitInfo.NewEdges.C);
			int32 OtherVIDB = SplitInfo.OtherVertices.B;
			if (OtherVIDB >= VertexValues.Num() || FMath::Abs(VertexValues[OtherVIDB] - IsoValue) <= Settings.CurveIsoValueSnapTolerance)
			{
				OnCutEdges.Add(SplitInfo.NewEdges.C);
			}
		}
	}
}
