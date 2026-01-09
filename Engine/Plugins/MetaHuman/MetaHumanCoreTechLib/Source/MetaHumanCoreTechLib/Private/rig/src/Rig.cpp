// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/Rig.h>

#include <rig/RigLogicDNAResource.h>
#include <carbon/io/Utils.h>
#include <pma/PolyAllocator.h>
#include <carbon/common/External.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)



template <class T>
Rig<T>::Rig()
{}

template <class T>
Rig<T>::~Rig()
{}

template <class T>
bool Rig<T>::VerifyTopology(const Mesh<T>& topology) const
{
    if (GetBaseMesh().NumVertices() != topology.NumVertices())
    {
        LOG_ERROR("tracking rig mesh does not have same number of vertices as the requested topology: {} vs {}",
                  GetBaseMesh().NumVertices(),
                  topology.NumVertices());
        return false;
    }
    if ((GetBaseMesh().NumQuads() != topology.NumQuads()) || (GetBaseMesh().Quads() != topology.Quads()))
    {
        LOG_ERROR("tracking rig has different quad topology compared to the requested topology: {} vs {}", GetBaseMesh().NumQuads(), topology.NumQuads());
        return false;
    }
    if ((GetBaseMesh().NumTriangles() != topology.NumTriangles()) || (GetBaseMesh().Triangles() != topology.Triangles()))
    {
        LOG_ERROR("tracking rig has different triangle topology compared to the requested topology: {} vs {}",
                  GetBaseMesh().NumTriangles(),
                  topology.NumTriangles());
        return false;
    }
    if (GetBaseMesh().NumTexQuads() != topology.NumTexQuads())
    {
        LOG_ERROR("tracking rig has different texture quad topology compared to the requested topology: {} vs {}",
                  GetBaseMesh().NumTexQuads(),
                  topology.NumTexQuads());
        return false;
    }
    if (GetBaseMesh().NumTexTriangles() != topology.NumTexTriangles())
    {
        LOG_ERROR("tracking rig has different texture triangle topology compared to the requested topology: {} vs {}",
                  GetBaseMesh().NumTexTriangles(),
                  topology.NumTexTriangles());
        return false;
    }
    if (GetBaseMesh().NumTexcoords() != topology.NumTexcoords())
    {
        LOG_ERROR("tracking rig has different texcoordinates compared to the requested topology: {} vs {}",
                  GetBaseMesh().NumTexcoords(),
                  topology.NumTexcoords());
        return false;
    }
    // texture coordinates may have a different order, so we need to compare the coordinates per texture quad/triangle
    T maxUVDiff = 0;
    for (int i = 0; i < GetBaseMesh().NumTexQuads(); ++i)
    {
        Eigen::Matrix<T, 2, 4> uvs1, uvs2;
        for (int k = 0; k < 4; ++k)
        {
            uvs1.col(k) = GetBaseMesh().Texcoords().col(GetBaseMesh().TexQuads()(k, i));
        }
        for (int k = 0; k < 4; ++k)
        {
            uvs2.col(k) = topology.Texcoords().col(topology.TexQuads()(k, i));
        }
        maxUVDiff = std::max<T>(maxUVDiff, (uvs1 - uvs2).cwiseAbs().maxCoeff());
    }
    for (int i = 0; i < GetBaseMesh().NumTexTriangles(); ++i)
    {
        Eigen::Matrix<T, 2, 3> uvs1, uvs2;
        for (int k = 0; k < 3; ++k)
        {
            uvs1.col(k) = GetBaseMesh().Texcoords().col(GetBaseMesh().TexTriangles()(k, i));
        }
        for (int k = 0; k < 3; ++k)
        {
            uvs2.col(k) = topology.Texcoords().col(topology.TexTriangles()(k, i));
        }
        maxUVDiff = std::max<T>(maxUVDiff, (uvs1 - uvs2).cwiseAbs().maxCoeff());
    }
    if (maxUVDiff > T(1e-6))
    {
        LOG_ERROR("tracking rig has different texcoordinate values compared to the requested topology: difference {}", maxUVDiff);
        return false;
    }
    return true;
}

template <class T>
bool Rig<T>::LoadRig(const std::string& DNAFilename, bool withJointScaling, std::map<std::string, typename RigGeometry<T>::ErrorInfo>* errorInfo)
{
    std::shared_ptr<const RigLogicDNAResource> dnaResource = RigLogicDNAResource::LoadDNA(DNAFilename, /*retain=*/false);
    if (!dnaResource)
    {
        LOG_ERROR("failed to open dnafile {}", DNAFilename);
        return false;
    }

    return LoadRig(dnaResource->Stream(), withJointScaling, errorInfo);
}

template <class T>
bool Rig<T>::LoadRig(const dna::Reader* DNAStream, bool withJointScaling, std::map<std::string, typename RigGeometry<T>::ErrorInfo>* errorInfo)
{
    auto memoryResource = MEM_RESOURCE;
    pma::PolyAllocator<RigLogic<T>> rlPolyAlloc{ memoryResource };
    pma::PolyAllocator<RigGeometry<T>> rgPolyAlloc{ memoryResource };
    std::shared_ptr<RigLogic<T>> rigLogic = std::allocate_shared<RigLogic<T>>(rlPolyAlloc);
    std::shared_ptr<RigGeometry<T>> rigGeometry = std::allocate_shared<RigGeometry<T>>(rgPolyAlloc);
    if (!rigLogic->Init(DNAStream, withJointScaling))
    {
        LOG_ERROR("failed to load riglogic from dnastream");
        return false;
    }
    if (!rigGeometry->Init(DNAStream, rigLogic->WithJointScaling(), errorInfo))
    {
        LOG_ERROR("failed to load riggeometry from dnastream");
        return false;
    }
    m_rigLogic = rigLogic;
    m_rigGeometry = rigGeometry;

    // get the neutral mesh geometry
    m_baseMesh = m_rigGeometry->GetMesh(0);
    m_baseMeshTriangulated = m_baseMesh;
    m_baseMeshTriangulated.Triangulate();
    m_baseMeshTriangulated.CalculateVertexNormals();

    return true;
}

template <class T>
Rig<T>::Rig(const std::shared_ptr<const RigLogic<T>>& rigLogic, const std::shared_ptr<const RigGeometry<T>>& rigGeometry):
    m_rigLogic(rigLogic), 
    m_rigGeometry(rigGeometry)
{
    // get the neutral mesh geometry
    m_baseMesh = m_rigGeometry->GetMesh(0);
    m_baseMeshTriangulated = m_baseMesh;
    m_baseMeshTriangulated.Triangulate();
    m_baseMeshTriangulated.CalculateVertexNormals();
}


template <class T>
void Rig<T>::UpdateBaseMeshTriangulation(const Mesh<T>& triangulatedMesh)
{
    if ((triangulatedMesh.NumTriangles() != m_baseMeshTriangulated.NumTriangles()) ||
        (triangulatedMesh.NumTexTriangles() != m_baseMeshTriangulated.NumTexTriangles()))
    {
        CARBON_CRITICAL("triangulated mesh does not match base mesh");
    }

    m_baseMeshTriangulated.SetTriangles(triangulatedMesh.Triangles());
    m_baseMeshTriangulated.SetTexTriangles(triangulatedMesh.TexTriangles());
}

template <class T>
const std::vector<std::string>& Rig<T>::GetGuiControlNames() const
{
    if (!m_rigLogic)
    {
        CARBON_CRITICAL("rig is not valid");
    }
    return m_rigLogic->GuiControlNames();
}

template <class T>
const std::vector<std::string>& Rig<T>::GetRawControlNames() const
{
    if (!m_rigLogic)
    {
        CARBON_CRITICAL("rig is not valid");
    }
    return m_rigLogic->RawControlNames();
}

template <class T>
void Rig<T>::EvaluateVertices(const Eigen::VectorX<T>& controls,
                              int lod,
                              const std::vector<int>& meshIndices,
                              typename RigGeometry<T>::State& state,
                              ControlsType controlsType) const
{
    const DiffData<T> rawControls = (controlsType == ControlsType::GuiControls) ? m_rigLogic->EvaluateRawControls(controls) : DiffData<T>(controls);
    const DiffData<T> psd = m_rigLogic->EvaluatePSD(rawControls);
    const DiffData<T> joints = m_rigLogic->EvaluateJoints(psd, lod);
    const DiffDataAffine<T, 3, 3> rigid;
    m_rigGeometry->EvaluateRigGeometry(rigid, joints, psd, meshIndices, state);
}

template <class T>
std::vector<Eigen::Matrix<T, 3, -1>> Rig<T>::EvaluateVertices(const Eigen::VectorX<T>& controls,
                                                              int lod,
                                                              const std::vector<int>& meshIndices,
                                                              ControlsType controlsType) const
{
    typename RigGeometry<T>::State state;
    EvaluateVertices(controls, lod, meshIndices, state, controlsType);

    std::vector<Eigen::Matrix<T, 3, -1>> vectorOfVertices;
    for (const auto& result : state.Vertices())
    {
        vectorOfVertices.emplace_back(result.Matrix());
    }
    return vectorOfVertices;
}

template <class T>
void Rig<T>::ReduceToLOD0Only()
{
    auto newRigLogic = m_rigLogic->Clone();
    auto newRigGeometry = m_rigGeometry->Clone();

    newRigLogic->ReduceToLOD0Only();
    newRigGeometry->ReduceToLOD0Only();

    newRigGeometry->RemoveUnusedJoints(*newRigLogic);

    m_rigLogic = newRigLogic;
    m_rigGeometry = newRigGeometry;
}

template <class T>
void Rig<T>::MakeBlendshapeOnly()
{
    auto newRigGeometry = m_rigGeometry->Clone();
    auto newRigLogic = m_rigLogic->Clone();
    newRigGeometry->MakeBlendshapeOnly(*newRigLogic);
    newRigGeometry->RemoveUnusedJoints(*newRigLogic);
    m_rigGeometry = newRigGeometry;
    m_rigLogic = newRigLogic;
}

template <class T>
void Rig<T>::ReduceToMeshes(const std::vector<int>& meshIndices)
{
    auto newRigGeometry = m_rigGeometry->Clone();
    auto newRigLogic = m_rigLogic->Clone();
    newRigGeometry->ReduceToMeshes(meshIndices);
    newRigGeometry->RemoveUnusedJoints(*newRigLogic);
    m_rigGeometry = newRigGeometry;
    m_rigLogic = newRigLogic;
}

template <class T>
void Rig<T>::RemoveBlendshapes(const std::vector<int>& meshIndices)
{
    auto newRigGeometry = m_rigGeometry->Clone();
    newRigGeometry->RemoveBlendshapes(meshIndices);
    m_rigGeometry = newRigGeometry;
}

template <class T>
void Rig<T>::ReduceToGuiControls(const std::vector<int>& guiControls)
{
    auto newRigLogic = m_rigLogic->Clone();
    newRigLogic->ReduceToGuiControls(guiControls);
    m_rigLogic = newRigLogic;
}

template <class T>
void Rig<T>::Resample(const std::map<int, std::vector<int>>& vertexIndicesMap)
{
    if (vertexIndicesMap.empty()) { return; }

    auto newRigGeometry = m_rigGeometry->Clone();
    for (const auto& [meshIndex, vertexIndices] : vertexIndicesMap)
    {
        newRigGeometry->Resample(meshIndex, vertexIndices);
        if (meshIndex == 0)
        {
            m_baseMesh.Resample(vertexIndices);
            m_baseMeshTriangulated.Resample(vertexIndices);
        }
    }
    m_rigGeometry = newRigGeometry;
}

template <class T>
void Rig<T>::SetMesh(int meshIndex, const Eigen::Matrix<T, 3, -1>& vertices)
{
    auto newRigGeometry = m_rigGeometry->Clone();
    newRigGeometry->SetMesh(meshIndex, vertices);
    m_rigGeometry = newRigGeometry;
}

template <class T>
void Rig<T>::Translate(const Eigen::Vector3<T>& translation)
{
    auto newRigGeometry = m_rigGeometry->Clone();
    newRigGeometry->Translate(translation);
    m_rigGeometry = newRigGeometry;

    m_baseMesh.SetVertices(m_baseMesh.Vertices().colwise() - translation);
    m_baseMeshTriangulated.SetVertices(m_baseMesh.Vertices());
}

template <class T>
void Rig<T>::Mirror(const std::map<std::string, std::vector<int>>& symmetries, const std::map<std::string, std::vector<std::pair<int, T>>>& meshRoots)
{
    auto newRigGeometry = m_rigGeometry->Clone();
    auto newRigLogic = m_rigLogic->Clone();
    newRigGeometry->Mirror(symmetries, meshRoots, *newRigLogic);
    m_rigGeometry = newRigGeometry;
    m_rigLogic = newRigLogic;

    m_baseMesh.SetVertices(m_rigGeometry->GetMesh(m_rigGeometry->HeadMeshIndex(/*lod=*/0)).Vertices());
    m_baseMeshTriangulated.SetVertices(m_baseMesh.Vertices());
}

template <class T>
void Rig<T>::UpdateRestOrientationEuler(const Eigen::Matrix<T, 3, -1>& restOrientationEuler)
{
    auto newRigGeometry = m_rigGeometry->Clone();
    auto newRigLogic = m_rigLogic->Clone();
    newRigGeometry->UpdateRestOrientationEuler(restOrientationEuler, *newRigLogic);
    m_rigGeometry = newRigGeometry;
    m_rigLogic = newRigLogic;
}

template <class T>
void Rig<T>::UpdateRestPose(const Eigen::Matrix<T, 3, -1>& restPose, CoordinateSystem coordinateSystem)
{
    auto newRigGeometry = m_rigGeometry->Clone();
    newRigGeometry->SetRestPose(restPose, coordinateSystem);
    m_rigGeometry = newRigGeometry;
}


template <class T>
void Rig<T>::CreateSubdivision(const Mesh<T>& subdivisionTopology, const std::vector<std::tuple<int, int, T>>& stencilWeights, int meshIndex)
{
    auto newRigGeometry = m_rigGeometry->Clone();
    newRigGeometry->CreateSubdivision(subdivisionTopology, stencilWeights, meshIndex);
    m_rigGeometry = newRigGeometry;
}

// explicitly instantiate the Rig classes
template class Rig<float>;
template class Rig<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
