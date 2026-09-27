// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Defs.h"

namespace TITAN_API_NAMESPACE
{

struct TITAN_API MeshInputData
{
    const int m_numTriangles;
    const int* m_triangles;
    const int m_numVertices;
    const float* m_vertices;
};

} // namespace TITAN_API_NAMESPACE
