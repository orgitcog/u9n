// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MathUtil.h"
#include "VectorTypes.h"
#include "GeometryTypes.h"

#define UE_API DYNAMICMESH_API

namespace UE
{
namespace Geometry
{

class FDynamicMesh3;

struct FMeshIsoCurveSettings
{
	// Whether to collapse any degenerate edges created by the curve insertion
	bool bCollapseDegenerateEdgesOnCut = true;

	// New edges shorter than this will be considered degenerate, and collapsed if bCollapseDegenerateEdgesOnCut is true
	double DegenerateEdgeTol = FMathd::ZeroTolerance;

	// Distance at which to snap curve vertices to nearby existing vertices
	double SnapToExistingVertexTol = UE_KINDA_SMALL_NUMBER;

	// Tolerance distance (in function domain) to an existing vertex to be 'on curve'
	float CurveIsoValueSnapTolerance = 0.f;
};

/**
 * Insert edges on a mesh along the isocurve where some scalar value function over the mesh surface crosses a specified value
 */
class FMeshIsoCurves
{
public:

	// Input options

	FMeshIsoCurveSettings Settings;

	/**
	 * Insert new edges on the given mesh along the curve where a function over the mesh surface crosses a given isovalue
	 * 
	 * @param Mesh The mesh to cut
	 * @param VertexFn Function from vertex ID to values
	 * @param EdgeCutFn Given the vertices of an edge and their values, return the parameter where the edge should be cut. Only called if IsoValue is crossed between ValueA and ValueB.
	 * @param IsoValue Value at which to insert a new curve on the mesh
	 */
	UE_API void Cut(FDynamicMesh3& Mesh, TFunctionRef<float(int32)> VertexFn, TFunctionRef<float(int32 VertA, int32 VertB, float ValueA, float ValueB)> EdgeCutFn, float IsoValue = 0);

	/**
	 * Insert new edges on the given mesh along the curve where a function over the mesh vertices, and linearly interpolated over edges, crosses a given isovalue
	 *
	 * @param Mesh The mesh to cut
	 * @param VertexFn Function from vertex ID to values
	 * @param IsoValue Value at which to insert a new curve on the mesh
	 */
	void Cut(FDynamicMesh3& Mesh, TFunctionRef<float(int32)> VertexFn, float IsoValue = 0)
	{
		Cut(Mesh, VertexFn, [IsoValue](int32, int32, float ValueA, float ValueB)
			{
				// Note this is only called on crossing edges, where ValueA != ValueB, so there is no divide-by-zero risk here
				return (ValueA - IsoValue) / (ValueA - ValueB);
			}, IsoValue
		);
	}

private:
	UE_API void SplitCrossingEdges(FDynamicMesh3& Mesh, const TArray<float>& VertexValues, TSet<int32>& OnCutEdges,
		TFunctionRef<float(int32 VIDMin, int32 VIDMax, float ValMin, float ValMax)> EdgeCutFn,
		float IsoValue);
};


} // end namespace UE::Geometry
} // end namespace UE

#undef UE_API
