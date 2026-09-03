// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/geometry/Mesh.h>
#include <nls/geometry/Camera.h>

#include <set>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::geoutils)

/**
 * Calculate vertex adjacency for a given triangle mesh
 */
std::vector<std::vector<int>> VertexAdjacency(const Eigen::Matrix<int, 3, -1>& Triangles);

/**
 * Find the neighboring triangles of each mesh triangle.
 * 
 * @param triangles is the input topology
 * @param adjacency (result) contains the neighbors ordered ordered according to the i-th traingle edges.
 * Edge 0: {triangles(0, i), triangles(1, i)};
 * edge 1: {triangles(1, i), triangles(2, i)};
 * edge 2: {triangles(2, i), triangles(0, i)};
 * @param outerTriangleEdges (result) contains for each i-th triangle the id of the edge of the j-th triangle
 * that is shared by (i, j).
*/
void TriangleAdjacency(
    const Eigen::Matrix<int, 3, -1>& triangles,
    Eigen::Matrix<int, 3, -1>& adjacency, 
    Eigen::Matrix<int, 3, -1>& outerTriangleEdges);

/**
 * Find the neighboring quads of each mesh triangle.
 * 
 * @param quads is the input topology
 * @param adjacency (result) contains the neighbors ordered ordered according to the i-th traingle edges.
 * Edge 0: {quads(0, i), quads(1, i)};
 * edge 1: {quads(1, i), quads(2, i)};
 * edge 2: {quads(2, i), quads(3, i)};
 * edge 3: {quads(3, i), quads(0, i)};
 * @param outerTriangleEdges (result) contains for each i-th quad the id of the edge of the j-th quad
 * that is shared by (i, j).
*/
void QuadAdjacency(
    const Eigen::Matrix<int, 4, -1>& quads,
    Eigen::Matrix<int, 4, -1>& adjacency, 
    Eigen::Matrix<int, 4, -1>& outerQuadEdges);

/**
 * Calculate vertex adjacency for a given tet mesh
 */
std::vector<std::vector<int>> VertexAdjacency(const Eigen::Matrix<int, 4, -1>& Tets);

/**
 * Creates a pyramid using origin as a center of the base and node for the apex
 */
template <class T>
static TITAN_NAMESPACE::Mesh<T> CreatePyramid(const Eigen::Vector3<T>& origin, const Eigen::Vector3<T>& node, T baseDistance)
{
    const Eigen::Vector3<T> direction = node - origin;
    const Eigen::Vector3<T> normal = direction.normalized();

    const Eigen::Vector3<T> basisVector1 = Eigen::Vector3<T>(-normal[1], normal[0], T(0)).normalized();

    const Eigen::Vector3<T> basisVector2 = Eigen::Vector3<T>(normal[1] * basisVector1[2] - normal[2] * basisVector1[1],
                                                             normal[2] * basisVector1[0] - normal[0] * basisVector1[2],
                                                             normal[0] * basisVector1[1] - normal[1] *
                                                             basisVector1[0]).normalized();


    const Eigen::Vector3<T> p1 = origin + baseDistance * basisVector1;
    const Eigen::Vector3<T> p2 = origin - baseDistance * basisVector1;
    const Eigen::Vector3<T> p3 = origin + baseDistance * basisVector2;
    const Eigen::Vector3<T> p4 = origin - baseDistance * basisVector2;

    Eigen::Matrix<T, 3, -1> vertices(3, 5);
    vertices.col(0) = node;
    vertices.col(1) = p1;
    vertices.col(2) = p2;
    vertices.col(3) = p3;
    vertices.col(4) = p4;

    Eigen::Matrix<int, 3, -1> tris(3, 6);
    tris.col(0) = Eigen::Vector3i(2, 4, 0);
    tris.col(1) = Eigen::Vector3i(3, 2, 0);
    tris.col(2) = Eigen::Vector3i(1, 3, 0);
    tris.col(3) = Eigen::Vector3i(4, 1, 0);
    tris.col(4) = Eigen::Vector3i(4, 3, 1);
    tris.col(5) = Eigen::Vector3i(3, 4, 2);

    TITAN_NAMESPACE::Mesh<T> mesh;
    mesh.SetTriangles(tris);
    mesh.SetVertices(vertices);
    return mesh;
}

/**
 * Creates a cylinder by stacking circles from the plane x = 0 y = 0
 */
template <class T>
static TITAN_NAMESPACE::Mesh<T> CreateCylinder(
    const T radius, 
    const T height, 
    const int numSamplesCircle, 
    const int numSamplesHeight)
{

    Mesh<T> mesh;
    const T dTheta = 2 * CARBON_PI / ( (T) numSamplesCircle );
    const T dheight = height / ( (T) numSamplesHeight  - 1);
    Eigen::Matrix<T, 3, -1> vertices(3, numSamplesCircle * numSamplesHeight);

    // Create circle at origin
    Eigen::Matrix<T, 3, -1> circleVertices(3, numSamplesCircle);
    T theta = CARBON_PI * T(0.5);
    for (int i = 0; i < numSamplesCircle; ++i){
        circleVertices(0, i) = radius * cos(theta);
        circleVertices(1, i) = radius * sin(theta);
        circleVertices(2, i) = T(0);
      theta += dTheta;
    }

    // Stack circles along height (from bottom to top)
    const int NumCylinderVertices = numSamplesCircle * numSamplesHeight + 2;
    Eigen::Matrix<T, 3, -1> cylinderVertices(3, NumCylinderVertices);
    T h = T(0);
    for (int i = 0; i < numSamplesHeight; ++i)
    {
        cylinderVertices.block(0, i * circleVertices.cols(), 3, circleVertices.cols()) = 
            circleVertices;
        cylinderVertices.block(0, i * circleVertices.cols(), 3, circleVertices.cols()).row(2).array()
            += h;
        h += dheight;
    }

    // Create center of top and bottom faces
    cylinderVertices.block(0, cylinderVertices.cols() - 2, 3, 2).setZero(); // Base circle is centered at origin
    cylinderVertices(2, cylinderVertices.cols() - 1) = height;

    // Create stencil for lateral faces between two circles
    Eigen::Matrix<int, 3, -1> LStencil(3, numSamplesCircle * 2);
    for (int i = 0; i < numSamplesCircle; ++i)
    {
        LStencil.col(2 * i) << 
            numSamplesCircle + i, 
            i,
            (i + 1) % numSamplesCircle;
        LStencil.col(2 * i + 1) << 
            numSamplesCircle + i, 
            (i + 1) % numSamplesCircle,
            numSamplesCircle + (i + 1) % numSamplesCircle;
    }

    const int NumCylinderTriangles = int(LStencil.cols()) * (numSamplesHeight - 1) + numSamplesCircle * 2;
    Eigen::Matrix<int, 3, -1> cylinderTriangles(3, NumCylinderTriangles);

    // Create topology (triangles) for lateral faces
    for (int i = 0; i < numSamplesHeight - 1; ++i)
    {
        cylinderTriangles.block(0, i * LStencil.cols(), 3, LStencil.cols()) =
            LStencil.array() + i * numSamplesCircle;
    }

    // Create top and bottom faces
    const int offsetTriangles1 = NumCylinderTriangles - numSamplesCircle * 2;
    const int offsetTriangles2 = NumCylinderTriangles - numSamplesCircle;
    const int verticesOffset = NumCylinderVertices - numSamplesCircle - 2;
    for (int i = 0; i < numSamplesCircle; ++i)
    {
        cylinderTriangles.col(offsetTriangles1 + i) <<  
            (i + 1) % numSamplesCircle, 
            i,
            NumCylinderVertices - 2;
        cylinderTriangles.col(offsetTriangles2 + i) << 
                verticesOffset + i, 
                verticesOffset + (i + 1) % numSamplesCircle,
                NumCylinderVertices - 1;
    }
    
    mesh.SetVertices(cylinderVertices);
    mesh.SetTriangles(cylinderTriangles);
    return mesh;
}

/**
 * Creates torus by stacking circles around the origin.
 */
template <class T>
static TITAN_NAMESPACE::Mesh<T> CreateTorus(
    const Eigen::Vector<T, 2>& radiuses,
    const Eigen::Vector2i& numSamplesPhiTheta, 
    bool triangulate = false)
{

    TITAN_NAMESPACE::Mesh<T> mesh;

    const T innerRadius = radiuses[0];
    const T outerRadius = radiuses[1];
    const T circleRadius = T(0.5) * (outerRadius - innerRadius);

    const T dPhi = 2 * T(CARBON_PI) / T(numSamplesPhiTheta[0]);
    const T dTheta = 2 * T(CARBON_PI) / T(numSamplesPhiTheta[1]);

    Eigen::Matrix<T, 3, -1> vertices(3, numSamplesPhiTheta[0] * numSamplesPhiTheta[1]);

    // Sample numSamplesPhiTheta[0] points over circle of circleRadius using displacement dTheta
    // The circle is translated along x of innerRadius
    Eigen::Matrix<T, 3, -1> circleVertices(3, numSamplesPhiTheta[0]);
    T phi = CARBON_PI * T(0.5);
    for(int i = 0; i < numSamplesPhiTheta[0]; ++i){
        circleVertices(0, i) = circleRadius * cos(phi) + innerRadius + circleRadius;
        circleVertices(1, i) = circleRadius * sin(phi);
        circleVertices(2, i) = T(0);
        phi += dPhi;
    }

    // Rotate circle around  y axis wrt origin (0, 0, 0)
    Eigen::Matrix<T, 3, 3> rotation = Eigen::Matrix<T, 3, 3>::Zero();
    T theta = T(0);
    for(int i = 0; i < numSamplesPhiTheta[1]; ++i){
        rotation(0, 0) = cos(theta);
        rotation(0, 2) = sin(theta);
        rotation(1, 1) = T(1);
        rotation(2, 0) = - sin(theta);
        rotation(2, 2) = cos(theta);
        
        vertices.block(0, numSamplesPhiTheta[0] * i, 3, numSamplesPhiTheta[0]) = rotation * circleVertices;
        theta += dTheta;
    }

    mesh.SetVertices(vertices);

    if(triangulate)
    {
        // Create triangles
        Eigen::Matrix<int, 3, -1> triangles(3, 2 * numSamplesPhiTheta[0] * numSamplesPhiTheta[1]);
        int offset = numSamplesPhiTheta[0] * numSamplesPhiTheta[1];
        int i = 0;
        for(; i < numSamplesPhiTheta[1] - 1; ++i)
        {
            int j = 0;
            for(; j < numSamplesPhiTheta[0] - 1; ++j)
            {
                triangles(0, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j + 1;
                triangles(1, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j;
                triangles(2, i * numSamplesPhiTheta[0] + j) = (i + 1) * numSamplesPhiTheta[0] + j;

                triangles(0, offset + i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j + 1;
                triangles(1, offset + i * numSamplesPhiTheta[0] + j) = (i + 1) * numSamplesPhiTheta[0] + j;
                triangles(2, offset + i * numSamplesPhiTheta[0] + j) = (i + 1) * numSamplesPhiTheta[0] + j + 1;
            }

            triangles(0, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0];
            triangles(1, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j;
            triangles(2, i * numSamplesPhiTheta[0] + j) = (i + 1) * numSamplesPhiTheta[0] + j;

            triangles(0, offset + i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0];
            triangles(1, offset + i * numSamplesPhiTheta[0] + j) = (i + 1) * numSamplesPhiTheta[0] + j;
            triangles(2, offset + i * numSamplesPhiTheta[0] + j) = (i + 1) * numSamplesPhiTheta[0];
        }

        int j = 0;
        for(; j < numSamplesPhiTheta[0] - 1; ++j)
        {
            triangles(0, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j + 1;
            triangles(1, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j;
            triangles(2, i * numSamplesPhiTheta[0] + j) = j;

            triangles(0, offset + i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j + 1;
            triangles(1, offset + i * numSamplesPhiTheta[0] + j) = j;
            triangles(2, offset + i * numSamplesPhiTheta[0] + j) = j + 1;
        }

        triangles(0, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0];
        triangles(1, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j;
        triangles(2, i * numSamplesPhiTheta[0] + j) = j;

        triangles(0, offset + i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0];
        triangles(1, offset + i * numSamplesPhiTheta[0] + j) = j;
        triangles(2, offset + i * numSamplesPhiTheta[0] + j) = 0;

        mesh.SetTriangles(triangles);
    }
    else{
        // Create quads
        Eigen::Matrix<int, 4, -1> quads(4, numSamplesPhiTheta[0] * numSamplesPhiTheta[1]);
        int i = 0;
        for(; i < numSamplesPhiTheta[1] - 1; ++i)
        {
            int j = 0;
            for(; j < numSamplesPhiTheta[0] - 1; ++j)
            {
                quads(0, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j + 1;
                quads(1, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j;
                quads(2, i * numSamplesPhiTheta[0] + j) = (i + 1) * numSamplesPhiTheta[0] + j;
                quads(3, i * numSamplesPhiTheta[0] + j) = (i + 1) * numSamplesPhiTheta[0] + j + 1;
            }
            
            quads(0, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0];
            quads(1, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j;
            quads(2, i * numSamplesPhiTheta[0] + j) = (i + 1) * numSamplesPhiTheta[0] + j;
            quads(3, i * numSamplesPhiTheta[0] + j) = (i + 1) * numSamplesPhiTheta[0];
        }

        int j = 0;
        for(; j < numSamplesPhiTheta[0] - 1; ++j)
        {
            quads(0, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j + 1;
            quads(1, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j;
            quads(2, i * numSamplesPhiTheta[0] + j) = j;
            quads(3, i * numSamplesPhiTheta[0] + j) = j + 1;
        }
        
        quads(0, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0];
        quads(1, i * numSamplesPhiTheta[0] + j) = i * numSamplesPhiTheta[0] + j;
        quads(2, i * numSamplesPhiTheta[0] + j) = j;
        quads(3, i * numSamplesPhiTheta[0] + j) = 0;

        mesh.SetQuads(quads);
    }

    return mesh;
}

/**
 * Creates a sphere by subdividing a diamond shape base shape and projecting the subdivision verices to the unit sphere.
 */
template <class T>
static TITAN_NAMESPACE::Mesh<T> CreateSphere(int subdivLevels)
{
    Eigen::Matrix<int, 3, -1> tris(3, 8);
    tris.col(0) = Eigen::Vector3i(0, 1, 4);
    tris.col(1) = Eigen::Vector3i(1, 2, 4);
    tris.col(2) = Eigen::Vector3i(2, 3, 4);
    tris.col(3) = Eigen::Vector3i(3, 0, 4);
    tris.col(4) = Eigen::Vector3i(0, 5, 1);
    tris.col(5) = Eigen::Vector3i(1, 5, 2);
    tris.col(6) = Eigen::Vector3i(2, 5, 3);
    tris.col(7) = Eigen::Vector3i(3, 5, 0);
    Eigen::Matrix<T, 3, -1> vertices(3, 6);
    vertices.col(0) = Eigen::Vector3<T>(0, 0, 1);
    vertices.col(1) = Eigen::Vector3<T>(1, 0, 0);
    vertices.col(2) = Eigen::Vector3<T>(0, 0, -1);
    vertices.col(3) = Eigen::Vector3<T>(-1, 0, 0);
    vertices.col(4) = Eigen::Vector3<T>(0, 1, 0);
    vertices.col(5) = Eigen::Vector3<T>(0, -1, 0);

    for (int i = 0; i < subdivLevels; i++)
    {
        std::set<std::pair<int, int>> edges;
        auto makeEdge = [](int vID0, int vID1) {
                if (vID0 < vID1)
                {
                    return std::pair<int, int>(vID0, vID1);
                }
                else
                {
                    return std::pair<int, int>(vID1, vID0);
                }
            };
        for (int j = 0; j < int(tris.cols()); j++)
        {
            edges.insert(makeEdge(tris(0, j), tris(1, j)));
            edges.insert(makeEdge(tris(1, j), tris(2, j)));
            edges.insert(makeEdge(tris(2, j), tris(0, j)));
        }
        std::map<std::pair<int, int>, int> edgesToNewVIDs;
        int newVerticesSize = int(vertices.cols());
        for (const std::pair<int, int>& edge : edges)
        {
            edgesToNewVIDs[edge] = newVerticesSize++;
        }
        Eigen::Matrix<T, 3, -1> newVertices(3, newVerticesSize);
        newVertices.leftCols(vertices.cols()) = vertices;
        for (const std::pair<int, int>& edge : edges)
        {
            int index = edgesToNewVIDs[edge];
            newVertices.col(index) = (vertices.col(edge.first) + vertices.col(edge.second)).normalized();
        }
        vertices.swap(newVertices);

        Eigen::Matrix<int, 3, -1> newTris(3, tris.cols() * 4);
        for (int j = 0; j < int(tris.cols()); j++)
        {
            const std::pair<int, int> e0 = makeEdge(tris(0, j), tris(1, j));
            const std::pair<int, int> e1 = makeEdge(tris(1, j), tris(2, j));
            const std::pair<int, int> e2 = makeEdge(tris(2, j), tris(0, j));
            newTris.col(4 * j + 0) = Eigen::Vector3i(tris(0, j), edgesToNewVIDs[e0], edgesToNewVIDs[e2]);
            newTris.col(4 * j + 1) = Eigen::Vector3i(edgesToNewVIDs[e0], tris(1, j), edgesToNewVIDs[e1]);
            newTris.col(4 * j + 2) = Eigen::Vector3i(edgesToNewVIDs[e2], edgesToNewVIDs[e0], edgesToNewVIDs[e1]);
            newTris.col(4 * j + 3) = Eigen::Vector3i(edgesToNewVIDs[e2], edgesToNewVIDs[e1], tris(2, j));
        }
        tris.swap(newTris);
    }

    TITAN_NAMESPACE::Mesh<T> mesh;
    mesh.SetTriangles(tris);
    mesh.SetVertices(vertices);
    return mesh;
}

/**
 * Creates a grid with a given size and number of subdivisions
 */
template <class T>
static TITAN_NAMESPACE::Mesh<T> CreateGrid(const T extentX, const T extentY, const int subdivX, const int subdivY) {

    // x = 4, y = 3
    // 0  1  2  3  4
    // 5  6  7  8  9
    // 10 11 12 13 14

    Eigen::Matrix<int, 4, -1> quads(4, subdivX * subdivY);
    int count = 0;
    for (int y = 0; y < subdivY; y++) {
        for (int x = 0; x < subdivX; x++) {
            quads.col(count++) = Eigen::Vector4i(
                y * (subdivX + 1) + x,
                y * (subdivX + 1) + x + 1,
                (y + 1) * (subdivX + 1) + x + 1,
                (y + 1) * (subdivX + 1) + x);
        }
    }

    const T centerX = extentX * 0.5f;
    const T centerY = extentY * 0.5f;
    const T sizeX = extentX / T(subdivX);
    const T sizeY = extentY / T(subdivY);
    Eigen::Matrix<T, 3, -1> vertices(3, (subdivX + 1) * (subdivY + 1));
    Eigen::Matrix<T, 3, -1> normals(3, (subdivX + 1) * (subdivY + 1));
    count = 0;
    for (int y = 0; y < subdivY + 1; y++) {
        for (int x = 0; x < subdivX + 1; x++) {
            normals.col(count) = Eigen::Vector3f::UnitZ();
            vertices.col(count++) = Eigen::Vector3f(
                T(x) * sizeX - centerX,
                T(y) * sizeY - centerY,
                0.0);
        }
    }

    TITAN_NAMESPACE::Mesh<T> mesh;
    mesh.SetQuads(quads);
    mesh.SetVertices(vertices);
    mesh.SetVertexNormals(normals);
    return mesh;
}

template <class T>
static const Mesh<T> ConstructMeshFromDepthStream(const Camera<T>& camera, const T* depthPtr, T distThresh)
{
    const auto width = camera.Width();
    const auto height = camera.Height();

    std::vector<Eigen::Vector3<T>> vertices;
    Eigen::Matrix<int, -1, -1> vertexIndices = Eigen::Matrix<int, -1, -1>::Constant(width, height, -1);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const T depth = depthPtr[y * width + x];
            if ((depth > T(0)) && (depth < distThresh))
            {
                vertexIndices(x, y) = static_cast<int>(vertices.size());
                vertices.push_back(camera.Unproject(Eigen::Vector2<T>(x + T(0.5), y + T(0.5)), depth, /*withExtrinsics=*/true));
            }
        }
    }
    std::vector<Eigen::Vector4i> quads;
    std::vector<Eigen::Vector3i> tris;
    for (int y = 0; y < height - 1; ++y)
    {
        for (int x = 0; x < width - 1; ++x)
        {
            const int vID00 = vertexIndices(x + 0, y + 0);
            const int vID10 = vertexIndices(x + 1, y + 0);
            const int vID01 = vertexIndices(x + 0, y + 1);
            const int vID11 = vertexIndices(x + 1, y + 1);
            if ((vID00 >= 0) && (vID10 >= 0) && (vID01 >= 0) && (vID11 >= 0))
            {
                quads.push_back(Eigen::Vector4i(vID00, vID01, vID11, vID10));
            }
            else if ((vID00 >= 0) && (vID10 >= 0) && (vID01 >= 0))
            {
                tris.push_back(Eigen::Vector3i(vID00, vID01, vID10));
            }
            else if ((vID00 >= 0) && (vID10 >= 0) && (vID11 >= 0))
            {
                tris.push_back(Eigen::Vector3i(vID00, vID11, vID10));
            }
            else if ((vID00 >= 0) && (vID01 >= 0) && (vID11 >= 0))
            {
                tris.push_back(Eigen::Vector3i(vID00, vID01, vID11));
            }
            else if ((vID10 >= 0) && (vID01 >= 0) && (vID11 >= 0))
            {
                tris.push_back(Eigen::Vector3i(vID10, vID01, vID11));
            }
        }
    }
    Mesh<T> mesh;
    mesh.SetVertices(Eigen::Map<const Eigen::Matrix<T, 3, -1>>((const T*)vertices.data(), 3, vertices.size()));
    mesh.SetQuads(Eigen::Map<const Eigen::Matrix<int, 4, -1>>((const int*)quads.data(), 4, quads.size()));
    mesh.SetTriangles(Eigen::Map<const Eigen::Matrix<int, 3, -1>>((const int*)tris.data(), 3, tris.size()));
    mesh.Triangulate();
    mesh.CalculateVertexNormals();

    return mesh;
}

template <class T>
static const Mesh<T> ConstructMeshFromMeshStream(const int numTriangles, const int* trianglesPtr, const int numVertices, const T* verticesPtr)
{
    Eigen::Matrix3Xf verticesMap = Eigen::Map<const Eigen::Matrix<float, 3, -1, Eigen::ColMajor>>(
        (const float*)verticesPtr,
        3,
        numVertices);

    Eigen::Matrix3Xi trianglesMap = Eigen::Map<const Eigen::Matrix<int, 3, -1, Eigen::ColMajor>>(
        (const int*)trianglesPtr,
        3,
        numTriangles);

    // verify that all vertices are valid
    int numInvalidVertices = 0;
    for (int i = 0; i < 3 * numVertices; ++i)
    {
        if (!std::isfinite(verticesPtr[i]))
        {
            numInvalidVertices++;
        }
    }
    if (numInvalidVertices > 0)
    {
        CARBON_CRITICAL("mesh contains {} vertices", numInvalidVertices);
    }

    // verify that all triangles index into valid vertices
    int numInvalidTriangles = 0;
    for (int i = 0; i < 3 * numTriangles; ++i)
    {
        if ((trianglesPtr[i] < 0) || (trianglesPtr[i] >= numVertices))
        {
            numInvalidTriangles++;
        }
    }
    if (numInvalidTriangles > 0)
    {
        CARBON_CRITICAL("mesh contains triangles with invalid vertex IDs (total {} invalid vertex IDs", numInvalidTriangles);
    }

    Mesh<float> mesh;
    mesh.SetVertices(verticesMap);
    mesh.SetTriangles(trianglesMap);
    mesh.CalculateVertexNormals();

    return mesh;
}

template <class T>
static const Eigen::VectorX<T> CalculateMaskBasedOnMeshTopology(const Mesh<T>& inputMesh, bool &bInvalidMeshTopology, T edgeRatioThreshold = T(10))
{
    bInvalidMeshTopology = false;
    Eigen::VectorX<T> scanMask = Eigen::VectorX<T>::Ones(inputMesh.NumVertices());
    const std::vector<int> borderVertices = inputMesh.CalculateBorderVertices();
    for (int vID : borderVertices)
    {
        scanMask[vID] = T(0.0);
    }
    // vertices that have zero vertex normals (either as they are not part of triangles, or the triangles have zero area)
    // should also not be used
    int outlierCounter = 0;
    for (int i = 0; i < inputMesh.NumVertices(); ++i)
    {
        if (inputMesh.VertexNormals().col(i).squaredNorm() < T(0.05))
        {
            scanMask[i] = T(0.0);
            outlierCounter++;
        }
    }

    for (int i = 0; i < inputMesh.NumTriangles(); ++i)
    {
        const int vID1 = inputMesh.Triangles().col(i)[0];
        const int vID2 = inputMesh.Triangles().col(i)[1];
        const int vID3 = inputMesh.Triangles().col(i)[2];

        const Eigen::Vector3<T> v1 = inputMesh.Vertices().col(vID1);
        const Eigen::Vector3<T> v2 = inputMesh.Vertices().col(vID2);
        const Eigen::Vector3<T> v3 = inputMesh.Vertices().col(vID3);

        const Eigen::Vector3<T> a = v1 - v2;
        const Eigen::Vector3<T> b = v2 - v3;
        const Eigen::Vector3<T> c = v3 - v1;

        const T ratio1 = a.norm() / b.norm();
        const T ratio2 = a.norm() / c.norm();
        const T ratio3 = b.norm() / c.norm();

        if ((T(1) / edgeRatioThreshold > ratio1) || (ratio1 > edgeRatioThreshold) || (T(1) / edgeRatioThreshold > ratio2) ||
            (ratio2 > edgeRatioThreshold) || (T(1) / edgeRatioThreshold > ratio3) || (ratio3 > edgeRatioThreshold))
        {
            scanMask[vID1] = T(0.0);
            scanMask[vID2] = T(0.0);
            scanMask[vID3] = T(0.0);

            outlierCounter += 3;
        }
    }

    if ((int(borderVertices.size()) == inputMesh.NumVertices()) || (outlierCounter == inputMesh.NumVertices()))
    {
        bInvalidMeshTopology = true;
    }

    return scanMask;
}

template <class T>
Eigen::VectorX<T> FitEllipse(const Eigen::Matrix2X<T>& points2d)
{
    const int numParams = 5;
    const int numObs = static_cast<int>(points2d.cols());

    Eigen::MatrixX<T> A = Eigen::MatrixX<T>(numObs, numParams);
    Eigen::VectorX<T> b = Eigen::VectorX<T>::Ones(numObs);

    for (int i = 0; i < numObs; ++i)
    {
        const T x = points2d(0, i);
        const T y = points2d(1, i);
        const T xy = x * y;
        const T xx = x * x;
        const T yy = y * y;

        A(i, 0) = x;
        A(i, 1) = y;
        A(i, 2) = xy;
        A(i, 3) = xx;
        A(i, 4) = yy;
    }

    Eigen::VectorX<T> x = (A.transpose() * A).inverse() * (A.transpose() * b);

    return x;
}

template <class T, int D>
static Eigen::Matrix<T, D, -1> CombinePoints(const std::vector<Eigen::Matrix<T, D, -1>>& vertices)
{
    int pointsSum = 0;
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        pointsSum += (int)vertices[i].cols();
    }

    Eigen::Matrix<T, D, -1> verticesCombined(vertices[0].rows(), pointsSum);
    int currentCol = 0;
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        verticesCombined.block(0, currentCol, vertices[i].rows(), vertices[i].cols()) = vertices[i];
        currentCol += (int)vertices[i].cols();
    }

    return verticesCombined;
}

template <class T, int D>
static Eigen::Matrix<T, D, -1> CombineRows(const std::vector<Eigen::Matrix<T, D, -1>>& vertices)
{
    int pointsSum = 0;
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        pointsSum += (int)vertices[i].rows();
    }

    Eigen::Matrix<T, D, -1> verticesCombined(pointsSum, vertices[0].cols());
    int currentRow = 0;
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        verticesCombined.block(currentRow, 0, vertices[i].rows(), vertices[i].cols()) = vertices[i];
        currentRow += (int)vertices[i].rows();
    }

    return verticesCombined;
}

template <class T>
static Eigen::VectorX<T> CombinePoints(const std::vector<Eigen::VectorX<T>>& input)
{
    int pointsSum = 0;
    for (size_t i = 0; i < input.size(); ++i)
    {
        pointsSum += (int)input[i].size();
    }

    Eigen::VectorX<T> combined(pointsSum);
    int currentPos = 0;
    for (size_t i = 0; i < input.size(); ++i)
    {
        combined.segment(currentPos, input[i].size()) = input[i];
        currentPos += (int)input[i].size();
    }

    return combined;
}

template <class T>
static std::pair<std::vector<int>, Mesh<T>> CombineMeshes(const std::vector<Mesh<T>>& meshes)
{
    std::vector<int> vertexOffsets { 0 };
    Eigen::Matrix<int, 4, -1> quads = meshes.front().Quads();
    Eigen::Matrix<int, 3, -1> tris = meshes.front().Triangles();
    Eigen::Matrix<T, 3, -1> vertices = meshes.front().Vertices();
    int vertexOffset = meshes.front().NumVertices();
    for (size_t i = 1; i < meshes.size(); ++i)
    {
        vertexOffsets.push_back(vertexOffset);
        quads.conservativeResize(4, quads.cols() + meshes[i].NumQuads());
        quads.rightCols(meshes[i].NumQuads()) = (meshes[i].Quads().array() + vertexOffset).matrix();
        tris.conservativeResize(3, tris.cols() + meshes[i].NumTriangles());
        tris.rightCols(meshes[i].NumTriangles()) = (meshes[i].Triangles().array() + vertexOffset).matrix();
        vertices.conservativeResize(3, vertices.cols() + meshes[i].NumVertices());
        vertices.rightCols(meshes[i].NumVertices()) = meshes[i].Vertices();
        vertexOffset += meshes[i].NumVertices();
    }
    vertexOffsets.push_back(vertexOffset);
    Mesh<T> combinedMesh;
    combinedMesh.SetQuads(quads);
    combinedMesh.SetTriangles(tris);
    combinedMesh.SetVertices(vertices);
    return { vertexOffsets, combinedMesh };
}

template <class T, int D = 3>
static std::vector<Eigen::Matrix<T, D, -1>> SplitVertices(const Eigen::Matrix<T, D, -1>& combined, const std::vector<int>& offsets)
{
    std::vector<Eigen::Matrix<T, D, -1>> output;
    int offsetsAcc = 0;
    for (size_t i = 0; i < offsets.size(); ++i)
    {
        output.push_back(combined.block(0, offsetsAcc, combined.rows(), offsets[i]));
        offsetsAcc += offsets[i];
    }
    return output;
}

template <class T>
static TITAN_NAMESPACE::Mesh<T> CreateTrapezoidalPrism(T start, T end, T startSize, T endSize)
{
    Eigen::Matrix<T, 3, -1> vertices(3, 6 * 4);
    int count = 0;
    vertices.col(count++) = Eigen::Vector3<T>(-startSize,  startSize, start);
    vertices.col(count++) = Eigen::Vector3<T>( startSize,  startSize, start);
    vertices.col(count++) = Eigen::Vector3<T>( startSize, -startSize, start);
    vertices.col(count++) = Eigen::Vector3<T>(-startSize, -startSize, start);

    vertices.col(count++) = Eigen::Vector3<T>(-endSize,  -endSize, end);
    vertices.col(count++) = Eigen::Vector3<T>( endSize,  -endSize, end);
    vertices.col(count++) = Eigen::Vector3<T>( endSize,   endSize, end);
    vertices.col(count++) = Eigen::Vector3<T>(-endSize,   endSize, end);

    vertices.col(count++) = Eigen::Vector3<T>(-startSize, -startSize, start);
    vertices.col(count++) = Eigen::Vector3<T>(-endSize, -endSize, end);
    vertices.col(count++) = Eigen::Vector3<T>(-endSize,  endSize, end);
    vertices.col(count++) = Eigen::Vector3<T>(-startSize,  startSize, start);

    vertices.col(count++) = Eigen::Vector3<T>(startSize, startSize, start);
    vertices.col(count++) = Eigen::Vector3<T>(endSize, endSize, end);
    vertices.col(count++) = Eigen::Vector3<T>(endSize,  -endSize, end);
    vertices.col(count++) = Eigen::Vector3<T>(startSize,  -startSize, start);

    vertices.col(count++) = Eigen::Vector3<T>(-startSize,-startSize, start);
    vertices.col(count++) = Eigen::Vector3<T>( startSize,-startSize, start);
    vertices.col(count++) = Eigen::Vector3<T>( endSize,  -endSize, end);
    vertices.col(count++) = Eigen::Vector3<T>(-endSize,  -endSize, end);

    vertices.col(count++) = Eigen::Vector3<T>( startSize, startSize, start);
    vertices.col(count++) = Eigen::Vector3<T>(-startSize, startSize, start);
    vertices.col(count++) = Eigen::Vector3<T>(-endSize,   endSize, end);
    vertices.col(count++) = Eigen::Vector3<T>(endSize,    endSize, end);

    Eigen::Matrix<int, 4, -1> quads(4, 6);
    for (int k = 0; k < 6; ++k)
    {
        quads.col(k) = Eigen::Vector4i(4 * k + 0, 4 * k + 1, 4 * k + 2, 4 * k + 3);
    }

    TITAN_NAMESPACE::Mesh<T> mesh;
    mesh.SetQuads(quads);
    mesh.SetVertices(vertices);
    return mesh;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::geoutils)
