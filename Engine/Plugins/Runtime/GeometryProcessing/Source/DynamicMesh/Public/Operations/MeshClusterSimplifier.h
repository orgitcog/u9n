// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryTypes.h"
#include "IndexTypes.h"
#include "Clustering/FaceNormalClustering.h"

class FProgressCancel;

namespace UE::Geometry
{

class FDynamicMesh3;

namespace MeshClusterSimplify
{

struct FSimplifyOptions
{
	// Vertices within this distance are allowed to be replaced with a single vertex
	double TargetEdgeLength = 1;

	// If > 0, boundary vertices w/ incident boundary edge angle greater than this (in degrees) will be kept in the output
	double FixBoundaryAngleTolerance = 45;

	// Constraint options control what simplifications are allowed
	enum class EConstraintLevel : uint8
	{
		// Fixed vertices/edges will generally be preserved in the output, as they will each be given their own cluster
		Fixed,
		// Constrained vertices/edges may be simplified, but the edge flow should be preserved
		// A vertex at an intersection of more than two constrained edges will be automatically preserved as 'Fixed'
		Constrained,
		// No constraints / ok to simplify as much as possible
		Free
	};

	struct FPreserveFeatures
	{
		// Mesh boundaries
		EConstraintLevel Boundary = EConstraintLevel::Constrained;
		// Seam types
		EConstraintLevel UVSeam = EConstraintLevel::Constrained;
		EConstraintLevel NormalSeam = EConstraintLevel::Constrained;
		EConstraintLevel TangentSeam = EConstraintLevel::Free;
		EConstraintLevel ColorSeam = EConstraintLevel::Constrained;
		// Material ID boundaries
		EConstraintLevel Material = EConstraintLevel::Constrained;
		// PolyGroup ID boundaries
		EConstraintLevel PolyGroup = EConstraintLevel::Constrained;

		// Helper to set all seam types to the same constraint level
		void SetSeamConstraints(EConstraintLevel Level)
		{
			UVSeam = Level;
			NormalSeam = Level;
			TangentSeam = Level;
			ColorSeam = Level;
		}
	};

	// Manage which feature edge types we try to retain in the simplified result
	FPreserveFeatures PreserveEdges{};

	// Whether to attempt to transfer attributes to the result mesh
	bool bTransferAttributes = true;

	// Whether to attempt to transfer triangle groups (PolyGroups) to the result mesh
	bool bTransferGroups = true;


};

/**
 * Makes a simplified copy of the input mesh
 * 
 * This cluster simplify method first clusters vertices locally by distance (calculated along mesh edges), and creates new triangles
 * from the connectivity of the clusters. i.e., it is a triangulation of the dual of the graph voronoi diagram over mesh edges.
 * 
 * To preserve feature edges:
 * (1) constrained / feature-edge vertices are prioritized as cluster 'seeds,' and 
 * (2) clusters are grown along feature edges first, then free edges after -- and growth over 'free' edges cannot claim 'constrained' vertices.
 * This locks in clusters along 'constrained' feature edges.
 * 
 * Note that mesh features can be lost if the clusters are large enough that the graph becomes degenerate
 *  -- e.g., if a mesh island has so few clusters that the graph connectivity does not contain triangles.
 * 
 * 
 * @param InMesh The mesh to simplify
 * @param OutSimplifiedMesh This mesh will store the simplified result mesh
 * @param SimplifyOptions Options controlling simplification
 * @return true on success
 */
bool DYNAMICMESH_API Simplify(const FDynamicMesh3& InMesh, FDynamicMesh3& OutSimplifiedMesh, const FSimplifyOptions& SimplifyOptions);

} // end namespace UE::Geometry
} // end namespace UE
