// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/geometry/TetMesh.h>
#include <unordered_map>
#include <carbon/io/NpyFileFormat.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
void TetMesh<T>::LoadFromNPY(std::string vertices_fname, std::string tets_fname)
{
    Eigen::Matrix<double, 3, -1> vertices;
    TITAN_NAMESPACE::npy::LoadMatrixFromNpy(vertices_fname, vertices);
    m_vertices = vertices.template cast<T>();
    TITAN_NAMESPACE::npy::LoadMatrixFromNpy(tets_fname, m_tets);
}

template <class T>
void TetMesh<T>::SaveToNPY(std::string vertices_fname, std::string tets_fname) const
{
    TITAN_NAMESPACE::npy::SaveMatrixAsNpy(vertices_fname, m_vertices.template cast<double>().eval());
    TITAN_NAMESPACE::npy::SaveMatrixAsNpy(tets_fname, m_tets);
}

template <class T>
void TetMesh<T>::BoundingBox(Eigen::Vector3<T>& bbmin, Eigen::Vector3<T>& bbmax) const
{
    const int numVertices = NumVertices();
    if (!numVertices)
    {
        CARBON_CRITICAL("Bounding box of empty mesh is undefined");
    }

    bbmin = m_vertices.rowwise().minCoeff();
    bbmax = m_vertices.rowwise().maxCoeff();
}

template <class T>
void TetMesh<T>::CropByPlane(std::vector<bool>& tetMask, const Eigen::Vector<T, 3>& normal, T offset) const
{
    const size_t numTets = NumTets();
    if (tetMask.size() != numTets) { CARBON_CRITICAL("Wrong size of tetMask"); }

    for (size_t t = 0; t < numTets; t++)
    {
        const int v[4] = { m_tets(0, t), m_tets(1, t), m_tets(2, t), m_tets(3, t) };

        for (int c = 0; c < 4; c++)
        {
            const T dp = m_vertices.col(v[c]).dot(normal);
            if (dp < offset) { tetMask[t] = false; }
        }
    }
}

template <class T>
void TetMesh<T>::FullVisualizationMesh(Eigen::Matrix<T, 3, -1>& visVertices, Eigen::Matrix<int, 3, -1>& visTriangles) const
{
    const int numTets = NumTets();

    visVertices.resize(3, numTets * 12);
    visTriangles.resize(3, numTets * 4);

    for (int t = 0; t < numTets; t++)
    {
        const int vID0 = m_tets(0, t);
        const int vID1 = m_tets(1, t);
        const int vID2 = m_tets(2, t);
        const int vID3 = m_tets(3, t);

        // create vertices for triangle 1:
        visVertices.col(12 * t + 0) = m_vertices.col(vID0);
        visVertices.col(12 * t + 1) = m_vertices.col(vID2);
        visVertices.col(12 * t + 2) = m_vertices.col(vID1);

        // create vertices for triangle 2:
        visVertices.col(12 * t + 3) = m_vertices.col(vID0);
        visVertices.col(12 * t + 4) = m_vertices.col(vID1);
        visVertices.col(12 * t + 5) = m_vertices.col(vID3);

        // create vertices for triangle 3:
        visVertices.col(12 * t + 6) = m_vertices.col(vID0);
        visVertices.col(12 * t + 7) = m_vertices.col(vID3);
        visVertices.col(12 * t + 8) = m_vertices.col(vID2);

        // create vertices for triangle 4:
        visVertices.col(12 * t + 9) = m_vertices.col(vID1);
        visVertices.col(12 * t + 10) = m_vertices.col(vID2);
        visVertices.col(12 * t + 11) = m_vertices.col(vID3);

        // create 4 triangles
        visTriangles.col(4 * t + 0) = Eigen::Vector3i(12 * t + 0, 12 * t + 1, 12 * t + 2);
        visTriangles.col(4 * t + 1) = Eigen::Vector3i(12 * t + 3, 12 * t + 4, 12 * t + 5);
        visTriangles.col(4 * t + 2) = Eigen::Vector3i(12 * t + 6, 12 * t + 7, 12 * t + 8);
        visTriangles.col(4 * t + 3) = Eigen::Vector3i(12 * t + 9, 12 * t + 10, 12 * t + 11);
    }
}

using Tuple3i = std::tuple<int, int, int>;

struct Tuple3iHash
{
    std::size_t operator()(const Tuple3i& p) const
    {
        auto h1 = std::hash<int>{}(std::get<0>(p));
        auto h2 = std::hash<int>{}(std::get<1>(p));
        auto h3 = std::hash<int>{}(std::get<2>(p));
        return h1 ^ h2 ^ h3;
    }
};

using TriMapType = std::unordered_map<Tuple3i, int, Tuple3iHash>;

template <class T>
std::vector<int> TetMesh<T>::BoundaryTets() const
{
    const size_t numTets = NumTets();
    TriMapType triangleCnt;
    triangleCnt.reserve(numTets * 4);

    auto createSortedTuple = [](int a, int b, int c) {
            if (a > b) { std::swap(a, b); }
            if (a > c) { std::swap(a, c); }
            if (b > c) { std::swap(b, c); }
            return Tuple3i(a, b, c);
        };

    // 1) hash each triangle (side of a tet)
    for (size_t t = 0; t < numTets; t++)
    {
        const int v0 = m_tets(0, t);
        const int v1 = m_tets(1, t);
        const int v2 = m_tets(2, t);
        const int v3 = m_tets(3, t);

        triangleCnt[createSortedTuple(v0, v2, v1)]++;
        triangleCnt[createSortedTuple(v0, v1, v3)]++;
        triangleCnt[createSortedTuple(v0, v3, v2)]++;
        triangleCnt[createSortedTuple(v1, v2, v3)]++;
    }

    // 2) find all triangles that are contained in only 1 tet (-> boundary triangles)
    std::vector<int> boundaryTets;
    for (size_t t = 0; t < numTets; t++)
    {
        bool hasBoundaryTriangle = false;
        const int v0 = m_tets(0, t);
        const int v1 = m_tets(1, t);
        const int v2 = m_tets(2, t);
        const int v3 = m_tets(3, t);

        if (triangleCnt[createSortedTuple(v0, v2, v1)] == 1) { hasBoundaryTriangle = true; }
        if (triangleCnt[createSortedTuple(v0, v1, v3)] == 1) { hasBoundaryTriangle = true; }
        if (triangleCnt[createSortedTuple(v0, v3, v2)] == 1) { hasBoundaryTriangle = true; }
        if (triangleCnt[createSortedTuple(v1, v2, v3)] == 1) { hasBoundaryTriangle = true; }
        if (hasBoundaryTriangle)
        {
            boundaryTets.push_back((int)t);
        }
    }
    return boundaryTets;
}

template <class T>
void TetMesh<T>::BoundaryMesh(Eigen::Matrix<T, 3, -1>& visVertices, Eigen::Matrix<int, 3, -1>& visTriangles, std::vector<bool>* tetPresent) const
{
    const size_t numTets = NumTets();
    if (tetPresent && (tetPresent->size() != numTets)) { CARBON_CRITICAL("Incorrect size of tetPresent"); }
    TriMapType triangleCnt;
    triangleCnt.reserve(numTets * 4);

    auto createSortedTuple = [](int a, int b, int c) {
            if (a > b) { std::swap(a, b); }
            if (a > c) { std::swap(a, c); }
            if (b > c) { std::swap(b, c); }
            return Tuple3i(a, b, c);
        };

    // 1) hash each triangle (side of a tet)
    for (size_t t = 0; t < numTets; t++)
    {
        if (tetPresent && !(*tetPresent)[t]) { continue; }

        const int v0 = m_tets(0, t);
        const int v1 = m_tets(1, t);
        const int v2 = m_tets(2, t);
        const int v3 = m_tets(3, t);

        triangleCnt[createSortedTuple(v0, v2, v1)]++;
        triangleCnt[createSortedTuple(v0, v1, v3)]++;
        triangleCnt[createSortedTuple(v0, v3, v2)]++;
        triangleCnt[createSortedTuple(v1, v2, v3)]++;
    }

    // 2) find all triangles that are contained in only 1 tet (-> boundary triangles)
    std::vector<Tuple3i> boundary_tri;
    for (size_t t = 0; t < numTets; t++)
    {
        if (tetPresent && !(*tetPresent)[t]) { continue; }

        const int v0 = m_tets(0, t);
        const int v1 = m_tets(1, t);
        const int v2 = m_tets(2, t);
        const int v3 = m_tets(3, t);

        if (triangleCnt[createSortedTuple(v0, v2, v1)] == 1) { boundary_tri.push_back(Tuple3i(v0, v2, v1)); }
        if (triangleCnt[createSortedTuple(v0, v1, v3)] == 1) { boundary_tri.push_back(Tuple3i(v0, v1, v3)); }
        if (triangleCnt[createSortedTuple(v0, v3, v2)] == 1) { boundary_tri.push_back(Tuple3i(v0, v3, v2)); }
        if (triangleCnt[createSortedTuple(v1, v2, v3)] == 1) { boundary_tri.push_back(Tuple3i(v1, v2, v3)); }
    }
    const size_t numVisTriangles = boundary_tri.size();

    visVertices.resize(3, numVisTriangles * 3);
    visTriangles.resize(3, numVisTriangles);
    for (size_t t = 0; t < numVisTriangles; t++)
    {
        const int v0 = std::get<0>(boundary_tri[t]);
        const int v1 = std::get<1>(boundary_tri[t]);
        const int v2 = std::get<2>(boundary_tri[t]);

        visVertices.col(3 * t + 0) = m_vertices.col(v0);
        visVertices.col(3 * t + 1) = m_vertices.col(v1);
        visVertices.col(3 * t + 2) = m_vertices.col(v2);

        visTriangles.col(t) = Eigen::Vector3i(int(3 * t), int(3 * t + 1), int(3 * t + 2));
    }
}

template <class T>
void TetMesh<T>::BoundaryMesh(Eigen::Matrix<int, 3, -1>& visTriangles, std::vector<bool>* tetPresent) const
{
    const size_t numTets = NumTets();
    if (tetPresent && (tetPresent->size() != numTets)) { CARBON_CRITICAL("Incorrect size of tetPresent"); }
    TriMapType triangleCnt;
    triangleCnt.reserve(numTets * 4);

    auto createSortedTuple = [](int a, int b, int c) {
            if (a > b) { std::swap(a, b); }
            if (a > c) { std::swap(a, c); }
            if (b > c) { std::swap(b, c); }
            return Tuple3i(a, b, c);
        };

    // 1) hash each triangle (side of a tet)
    for (size_t t = 0; t < numTets; t++)
    {
        if (tetPresent && !(*tetPresent)[t]) { continue; }

        const int v0 = m_tets(0, t);
        const int v1 = m_tets(1, t);
        const int v2 = m_tets(2, t);
        const int v3 = m_tets(3, t);

        triangleCnt[createSortedTuple(v0, v2, v1)]++;
        triangleCnt[createSortedTuple(v0, v1, v3)]++;
        triangleCnt[createSortedTuple(v0, v3, v2)]++;
        triangleCnt[createSortedTuple(v1, v2, v3)]++;
    }

    // 2) find all triangles that are contained in only 1 tet (-> boundary triangles)
    std::vector<Tuple3i> boundary_tri;
    for (size_t t = 0; t < numTets; t++)
    {
        if (tetPresent && !(*tetPresent)[t]) { continue; }

        const int v0 = m_tets(0, t);
        const int v1 = m_tets(1, t);
        const int v2 = m_tets(2, t);
        const int v3 = m_tets(3, t);

        if (triangleCnt[createSortedTuple(v0, v2, v1)] == 1) { boundary_tri.push_back(Tuple3i(v0, v2, v1)); }
        if (triangleCnt[createSortedTuple(v0, v1, v3)] == 1) { boundary_tri.push_back(Tuple3i(v0, v1, v3)); }
        if (triangleCnt[createSortedTuple(v0, v3, v2)] == 1) { boundary_tri.push_back(Tuple3i(v0, v3, v2)); }
        if (triangleCnt[createSortedTuple(v1, v2, v3)] == 1) { boundary_tri.push_back(Tuple3i(v1, v2, v3)); }
    }
    const size_t numVisTriangles = boundary_tri.size();

    visTriangles.resize(3, numVisTriangles);
    for (size_t t = 0; t < numVisTriangles; t++)
    {
        const int v0 = std::get<0>(boundary_tri[t]);
        const int v1 = std::get<1>(boundary_tri[t]);
        const int v2 = std::get<2>(boundary_tri[t]);
        visTriangles.col(t) = Eigen::Vector3i(v0, v1, v2);
    }
}

template <class T>
Eigen::VectorX<T> TetMesh<T>::TetVolumes() const
{
    const size_t numTets = NumTets();
    Eigen::VectorX<T> volumes(numTets);
    for (size_t t = 0; t < numTets; t++)
    {
        const Eigen::Vector3<T>& v0 = m_vertices.col(m_tets(0, t));
        const Eigen::Vector3<T>& v1 = m_vertices.col(m_tets(1, t));
        const Eigen::Vector3<T>& v2 = m_vertices.col(m_tets(2, t));
        const Eigen::Vector3<T>& v3 = m_vertices.col(m_tets(3, t));

        Eigen::Matrix<T, 3, 3> restFrame;
        restFrame.col(0) = v1 - v0;
        restFrame.col(1) = v2 - v0;
        restFrame.col(2) = v3 - v0;

        volumes[t] = restFrame.determinant() / T(6.0);
    }

    return volumes;
}

template <class T>
void TetMesh<T>::TetVolumeStatistics(T& minVol, T& avgVol, T& maxVol, bool absValue) const
{
    Eigen::VectorX<T> volumes = TetVolumes();
    if (absValue) { volumes = volumes.cwiseAbs(); }
    minVol = volumes.minCoeff();
    maxVol = volumes.maxCoeff();
    avgVol = volumes.mean();
}

template class TetMesh<float>;
template class TetMesh<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
