// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/EyeAssetGenerator.h>
#include <nls/VertexOptimization.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/functions/PointPointConstraintFunction.h>
#include <nls/geometry/MeshCorrespondenceSearch.h>
#include <nrr/EyeballConstraints.h>
#include <nrr/deformation_models/DeformationModelVertex.h>
#include <carbon/utils/Timer.h>
#include <set>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)


template <class T>
bool EyeAssetGeneratorParams<T>::ReadJson(const JsonElement& element)
{
    if (!element.IsObject())
    {
        LOG_ERROR("params json is not an object");
        return false;
    }

    if (element.Contains("wrap_deformer_params") && element["wrap_deformer_params"].IsObject())
    {
        const auto& paramsJson = element["wrap_deformer_params"];
        const bool bLoadedParams = wrapDeformerParams.ReadJson(paramsJson);
        if (!bLoadedParams)
        {
            LOG_ERROR("failed to load wrap deformer params from eye asset generation configuration");
            return false;
        }
    }
    else
    {
        LOG_ERROR("wrap_deformer_params missing from eye asset generation configuration");
        return false;
    }

    if (element.Contains("eyeball_normal_displacement") && element["eyeball_normal_displacement"].IsNumber())
    {
        eyeballNormalDisplacement = T(element["eyeball_normal_displacement"].Get<T>());
    }
    else
    {
        LOG_ERROR("eyeball_normal_displacement missing from eye asset generation configuration");
        return false;
    }

    if (element.Contains("face_lock_distance_threshold") && element["face_lock_distance_threshold"].IsNumber())
    {
        faceLockDistanceThreshold = T(element["face_lock_distance_threshold"].Get<T>());
    }
    else
    {
        LOG_ERROR("face_lock_distance_threshold missing from eye asset generation configuration");
        return false;
    }

    if (element.Contains("eye_distance_threshold") && element["eye_distance_threshold"].IsNumber())
    {
        eyeDistanceThreshold = T(element["eye_distance_threshold"].Get<T>());
    }
    else
    {
        LOG_ERROR("eye_distance_threshold missing from eye asset generation configuration");
        return false;
    }

    if (element.Contains("num_solver_iterations") && element["num_solver_iterations"].IsInt())
    {
        numSolverIterations = element["num_solver_iterations"].Get<int>();
    }
    else
    {
        LOG_ERROR("num_solver_iterations missing from eye asset generation configuration");
        return false;
    }

    if (element.Contains("num_solver_cg_iterations") && element["num_solver_cg_iterations"].IsInt())
    {
        numSolverCGIterations = element["num_solver_cg_iterations"].Get<int>();
    }
    else
    {
        LOG_ERROR("num_solver_cg_iterations missing from eye asset generation configuration");
        return false;
    }

    if (element.Contains("left_right_split_index") && element["left_right_split_index"].IsInt() &&
        element["left_right_split_index"].Get<int>() >=0 && element["left_right_split_index"].Get<int>() < 3)
    {
        leftRightSplitIndex = element["left_right_split_index"].Get<int>();
    }
    else
    {
        LOG_ERROR("left_right_split_index missing from eye asset generation configuration");
        return false;
    }

    if (element.Contains("optimize_pose"))
    {
        optimizePose = element["optimize_pose"].Boolean();
    }
    else
    {
        LOG_ERROR("optimize_pose missing from eye asset generation configuration");
        return false;
    }

        
    if (element.Contains("wrap_deform_only_vertex_indices") && element["wrap_deform_only_vertex_indices"].IsArray())
    {
        std::vector<int> wrapDeformOnlyVertexIndicesVec = element["wrap_deform_only_vertex_indices"].Get<std::vector<int>>();
        wrapDeformOnlyVertexIndices = Eigen::VectorXi(int(wrapDeformOnlyVertexIndicesVec.size()));
        for (size_t i = 0; i < wrapDeformOnlyVertexIndicesVec.size(); ++i)
        {
            wrapDeformOnlyVertexIndices(static_cast<int>(i)) = wrapDeformOnlyVertexIndicesVec[i];
        }
    }
    else
    {
        LOG_INFO("wrap_deform_only_vertex_indices parameter not present");
        wrapDeformOnlyVertexIndices = Eigen::VectorXi();
    }

    
    if (element.Contains("deformation_model_vertex_weight") && element["deformation_model_vertex_weight"].IsNumber())
    {
        deformationModelVertexWeight = T(element["deformation_model_vertex_weight"].Get<T>());
    }
    else
    {
        LOG_ERROR("deformation_model_vertex_weight missing from eye asset generation configuration");
        return false;
    }

    if (element.Contains("point_to_point_constraint_weight") && element["point_to_point_constraint_weight"].IsNumber())
    {
        pointToPointConstraintWeight = T(element["point_to_point_constraint_weight"].Get<T>());
    }
    else
    {
        LOG_ERROR("point_to_point_constraint_weight missing from eye asset generation configuration");
        return false;
    }

    if (element.Contains("eye_constraint_weight") && element["eye_constraint_weight"].IsNumber())
    {
        eyeConstraintWeight = T(element["eye_constraint_weight"].Get<T>());
    }
    else
    {
        LOG_ERROR("eye_constraint_weight missing from eye asset generation configuration");
        return false;
    }

    if (element.Contains("caruncle_vertex_indices") && element["caruncle_vertex_indices"].IsArray())
    {
        std::vector<int> caruncleVertexIndicesVec = element["caruncle_vertex_indices"].Get<std::vector<int>>();
        caruncleVertexIndices = Eigen::VectorXi(int(caruncleVertexIndicesVec.size()));
        for (size_t i = 0; i < caruncleVertexIndicesVec.size(); ++i)
        {
            caruncleVertexIndices(static_cast<int>(i)) = caruncleVertexIndicesVec[i];
        }
    }
    else
    {
        LOG_INFO("caruncle_vertex_indices parameter not present");
        caruncleVertexIndices = Eigen::VectorXi();
    }

        
    if (element.Contains("caruncle_multiplier") && element["caruncle_multiplier"].IsNumber())
    {
        caruncleMultiplier = T(element["caruncle_multiplier"].Get<T>());
    }
    else
    {
        LOG_ERROR("caruncle_multiplier missing from eye asset generation configuration");
        return false;
    }

    return true;
}

template <class T>
bool ToBinaryFile(FILE* pFile, const EyeAssetGeneratorParams<T>& params)
{
    bool success = true;
    success &= io::ToBinaryFile(pFile, params.version);
    success &= ToBinaryFile(pFile, params.wrapDeformerParams);
    success &= io::ToBinaryFile(pFile, params.eyeballNormalDisplacement);
    success &= io::ToBinaryFile(pFile, params.faceLockDistanceThreshold);
    success &= io::ToBinaryFile(pFile, params.faceLockDistanceThreshold);
    success &= io::ToBinaryFile(pFile, params.eyeDistanceThreshold);
    success &= io::ToBinaryFile(pFile, params.numSolverIterations);
    success &= io::ToBinaryFile(pFile, params.numSolverCGIterations);
    success &= io::ToBinaryFile(pFile, params.leftRightSplitIndex);
    success &= io::ToBinaryFile(pFile, params.optimizePose);
    success &= io::ToBinaryFile(pFile, params.wrapDeformOnlyVertexIndices);
    success &= io::ToBinaryFile(pFile, params.deformationModelVertexWeight);
    success &= io::ToBinaryFile(pFile, params.pointToPointConstraintWeight);
    success &= io::ToBinaryFile(pFile, params.eyeConstraintWeight);
    success &= io::ToBinaryFile(pFile, params.caruncleVertexIndices);
    success &= io::ToBinaryFile(pFile, params.caruncleMultiplier);

    return success;
}

template <class T>
bool FromBinaryFile(FILE* pFile, EyeAssetGeneratorParams<T>& params) 
{
    bool success = true;
    int32_t version;
    success &= io::FromBinaryFile(pFile, version);
    if (success && version == 2)
    {
        success &= FromBinaryFile(pFile, params.wrapDeformerParams);
        success &= io::FromBinaryFile(pFile, params.eyeballNormalDisplacement);
        success &= io::FromBinaryFile(pFile, params.faceLockDistanceThreshold);
        success &= io::FromBinaryFile(pFile, params.faceLockDistanceThreshold);
        success &= io::FromBinaryFile(pFile, params.eyeDistanceThreshold);
        success &= io::FromBinaryFile(pFile, params.numSolverIterations);
        success &= io::FromBinaryFile(pFile, params.numSolverCGIterations);
        success &= io::FromBinaryFile(pFile, params.leftRightSplitIndex);
        success &= io::FromBinaryFile(pFile, params.optimizePose);
        success &= io::FromBinaryFile(pFile, params.wrapDeformOnlyVertexIndices);
        success &= io::FromBinaryFile(pFile, params.deformationModelVertexWeight);
        success &= io::FromBinaryFile(pFile, params.pointToPointConstraintWeight);
        success &= io::FromBinaryFile(pFile, params.eyeConstraintWeight);
        success &= io::FromBinaryFile(pFile, params.caruncleVertexIndices);
        success &= io::FromBinaryFile(pFile, params.caruncleMultiplier);
    }
    else if(success && version == 1)
    {
        success &= FromBinaryFile(pFile, params.wrapDeformerParams);
        success &= io::FromBinaryFile(pFile, params.eyeballNormalDisplacement);
        success &= io::FromBinaryFile(pFile, params.faceLockDistanceThreshold);
        success &= io::FromBinaryFile(pFile, params.faceLockDistanceThreshold);
        success &= io::FromBinaryFile(pFile, params.eyeDistanceThreshold);
        success &= io::FromBinaryFile(pFile, params.numSolverIterations);
        success &= io::FromBinaryFile(pFile, params.numSolverCGIterations);
        success &= io::FromBinaryFile(pFile, params.leftRightSplitIndex);
        success &= io::FromBinaryFile(pFile, params.optimizePose);
        success &= io::FromBinaryFile(pFile, params.wrapDeformOnlyVertexIndices);
        success &= io::FromBinaryFile(pFile, params.deformationModelVertexWeight);
        success &= io::FromBinaryFile(pFile, params.pointToPointConstraintWeight);
        success &= io::FromBinaryFile(pFile, params.eyeConstraintWeight);
        params.caruncleVertexIndices = {};
        params.caruncleMultiplier = 0.5;
    }
    else
    {
        success = false;
    }
    return success;
}


template <class T>
EyeAssetGenerator<T>::EyeAssetGenerator()
{
    m_taskThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true);
}

template <class T>
void EyeAssetGenerator<T>::SetThreadPool(const std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool>& taskThreadPool)
{
    m_taskThreadPool = taskThreadPool;
}

template <class T>
void EyeAssetGenerator<T>::FixCaruncleIntersection(const EyeAssetGenerator<T>& eyeshellAsset, const EyeAssetGenerator<T>& eyeEdgeAsset,
    Eigen::Matrix<T, 3, -1>& deformedEyeshellAssetMeshVertices, Eigen::Matrix<T, 3, -1>& deformedEyeEdgeAssetMeshVertices)
{
    // simple and fast heuristic to improve the caruncle region, where there are often small intersections
    // improves the results, but minimizes the risk of introducing artifacts. Could probably be done in a more sophisticated way.

    if (!eyeshellAsset.m_eyeAssetMesh.get() || deformedEyeshellAssetMeshVertices.cols() != eyeshellAsset.m_eyeAssetMesh->Vertices().cols())
    {
        CARBON_CRITICAL("supplied eyeshell vertices are not consistent with those in the supplied eyeshell asset");
    }

    if (!eyeEdgeAsset.m_eyeAssetMesh.get() || deformedEyeEdgeAssetMeshVertices.cols() != eyeEdgeAsset.m_eyeAssetMesh->Vertices().cols())
    {
        CARBON_CRITICAL("supplied eyeEdge vertices are not consistent with those in the supplied eyeEdge asset");
    }

    if (eyeEdgeAsset.m_params.caruncleVertexIndices.size() != eyeshellAsset.m_params.caruncleVertexIndices.size())
    {
        CARBON_CRITICAL("caruncle indices must be the same length in the eyeshell and eyeEdge assets");
    }

    if (eyeshellAsset.m_params.caruncleVertexIndices.size() > 0)
    {
        Eigen::Matrix<T, 3, -1> normalsEyeshell;
        eyeshellAsset.m_eyeAssetMesh->CalculateVertexNormals(deformedEyeshellAssetMeshVertices, normalsEyeshell, VertexNormalComputationType::VoronoiAreaWeighted, true);

        auto correctCaruncle = [&](int start, int end)
        {
            for (int i = start; i < end; ++i)
            {
                int indexEyeEdge = eyeEdgeAsset.m_params.caruncleVertexIndices(i);
                int indexEyeshell = eyeshellAsset.m_params.caruncleVertexIndices(i);
                Eigen::Vector<T, 3> eyeEdgeVert = deformedEyeEdgeAssetMeshVertices.col(indexEyeEdge);
                Eigen::Vector<T, 3> eyeshellVert = deformedEyeshellAssetMeshVertices.col(indexEyeshell);
                Eigen::Vector<T, 3> delta = eyeshellVert - eyeEdgeVert;
                T dotProd = delta.dot(normalsEyeshell.col(indexEyeshell));
                if (dotProd <= 0)
                {
                    deformedEyeEdgeAssetMeshVertices.col(indexEyeEdge) += normalsEyeshell.col(indexEyeshell) * dotProd * eyeEdgeAsset.m_params.caruncleMultiplier;
                    deformedEyeshellAssetMeshVertices.col(indexEyeshell) -= normalsEyeshell.col(indexEyeshell) * dotProd * eyeshellAsset.m_params.caruncleMultiplier;
                }
            }
        };


        eyeshellAsset.m_taskThreadPool->AddTaskRangeAndWait((int)eyeEdgeAsset.m_params.caruncleVertexIndices.size(), correctCaruncle);
    }
}


template <class T>
void EyeAssetGenerator<T>::InitializeAsset()
{     
     
    // calculate eyeball distances for the eyeball constraints for the eye asset; we will set these up fully when we generate the assets
    const T faceLockDistThreshold = m_params.faceLockDistanceThreshold;
    const T eyeDistThreshold = m_params.eyeDistanceThreshold;

    MeshCorrespondenceSearch<T> meshSearch;
    meshSearch.Init(*m_headMesh);

    TITAN_NAMESPACE::TaskFutures futures;

    auto isLeft = [&](const Eigen::Vector<T, 3>& vert)
    {
        return vert(m_params.leftRightSplitIndex) > 0;
    };

    auto isRight = [&](const Eigen::Vector<T, 3>& vert)
    {
        return vert(m_params.leftRightSplitIndex) <= 0;
    };

    auto initializeEyeAsset
        = [&](std::function<bool(const Eigen::Vector<T, 3>&)> isCorrectFaceSide, const std::shared_ptr<const Mesh<T>>& eyeMesh, Eigen::VectorX<T>& eyeWeights,
              Eigen::VectorXi& faceCorrespondenceIndices, Eigen::Matrix<T, 3, -1>& assetVertices, Eigen::VectorXi& assetVertexIndices, Eigen::Matrix<int, 3, -1>& assetTriangles)
    {
        // calculate the assetVertices and vertex indices and asset triangles
        const Eigen::Matrix<T, 3, -1>& allAssetVertices = m_eyeAssetMesh->Vertices();
        std::vector<int> assetVertexIndicesVec;

        for (int col = 0; col < allAssetVertices.cols(); ++col)
        {
            if (isCorrectFaceSide(allAssetVertices.col(col)))
            {
                assetVertexIndicesVec.push_back(col);
            }
        }

        // resample the asset mesh
        Mesh<T> copyOfAssetMesh = *m_eyeAssetMesh;
        copyOfAssetMesh.Resample(assetVertexIndicesVec);
        assetTriangles = copyOfAssetMesh.Triangles();
        assetVertices = copyOfAssetMesh.Vertices();

        assetVertexIndices = Eigen::VectorXi(int(assetVertexIndicesVec.size()));
        for (int i = 0; i < int(assetVertexIndicesVec.size()); ++i)
        {
            assetVertexIndices(i) = assetVertexIndicesVec[size_t(i)];
        }

        EyeballConstraints<T> eyeConstraintsForAsset;
        auto config = eyeConstraintsForAsset.GetConfiguration();
        config["normalDisplacement"] = m_params.eyeballNormalDisplacement;
        eyeConstraintsForAsset.SetConfiguration(config);
        eyeConstraintsForAsset.SetEyeballMesh(*eyeMesh);

        const std::vector<T> neutralAsseteyeDistances = eyeConstraintsForAsset.GetEyeballDistances(assetVertices);
        eyeWeights = Eigen::VectorX<T>::Zero(neutralAsseteyeDistances.size());
        for (size_t i = 0; i < neutralAsseteyeDistances.size(); ++i)
        {
            if (fabs(neutralAsseteyeDistances[i]) < eyeDistThreshold)
            {
                eyeWeights[i] = (eyeDistThreshold - fabs(neutralAsseteyeDistances[i])) / eyeDistThreshold;
            }
        }

        std::vector<int> faceCorrespondenceIDs;

        for (int i = 0; i < int(assetVertices.cols()); ++i)
        {
            const BarycentricCoordinates<T, 3> bc = meshSearch.Search(assetVertices.col(i));
            const T dist = (bc.template Evaluate<3>(m_headMesh->Vertices()) - assetVertices.col(i)).norm();
            if (dist < faceLockDistThreshold)
            {
                faceCorrespondenceIDs.push_back(i);
            }
        }

        faceCorrespondenceIndices = Eigen::Map<const Eigen::VectorXi>(faceCorrespondenceIDs.data(), faceCorrespondenceIDs.size());
    };

    m_wrapDeformer.Init(m_headMesh, m_eyeAssetMesh, m_params.wrapDeformerParams);
    

    futures.Add(m_taskThreadPool->AddTask([&]()
        { 
            initializeEyeAsset(isLeft, m_eyeLeftMesh, m_eyeLeftWeights, m_leftFaceCorrespondenceIndices, m_leftAssetVertices, m_leftAssetVertexIndices, m_leftAssetTriangles);
        }));


    futures.Add(m_taskThreadPool->AddTask([&]()
        { 
            initializeEyeAsset(isRight, m_eyeRightMesh, m_eyeRightWeights, m_rightFaceCorrespondenceIndices, m_rightAssetVertices, m_rightAssetVertexIndices, m_rightAssetTriangles);
        }));

    futures.Wait();

 
}

template <class T>
void EyeAssetGenerator<T>::SetMeshes(const std::shared_ptr<const Mesh<T>>& headMesh, const std::shared_ptr<const Mesh<T>>& eyeLeftMesh, const std::shared_ptr<const Mesh<T>>& eyeRightMesh,
    const std::shared_ptr<const Mesh<T>>& eyeAssetMesh)
{
    m_headMesh = headMesh;
    m_eyeRightMesh = eyeRightMesh;
    m_eyeLeftMesh = eyeLeftMesh;
    m_eyeAssetMesh = eyeAssetMesh;
    m_wrapDeformer.SetMeshes(m_headMesh, m_eyeAssetMesh);
}

template <class T>
bool EyeAssetGenerator<T>::CheckAssetVertexIndices(const Eigen::VectorXi & vertexIndices) const
{
    std::set<int> uniqueIndices(vertexIndices.data(), vertexIndices.data() + vertexIndices.size());
    if (uniqueIndices.size() != size_t(vertexIndices.size()))
    {
        return false;
    }
    
    for (const auto & index : uniqueIndices)
    {
        if (index < 0 || index >= m_eyeAssetMesh->NumVertices())
        {
            return false;
        }
    }

    return true;
}

template <class T>
void EyeAssetGenerator<T>::Init(const std::shared_ptr<const Mesh<T>>& headMesh, const std::shared_ptr<const Mesh<T>>& eyeLeftMesh, const std::shared_ptr<const Mesh<T>>& eyeRightMesh, 
    const std::shared_ptr<const Mesh<T>>& eyeAssetMesh, const EyeAssetGeneratorParams<T>& params) 
{ 
    SetMeshes(headMesh, eyeLeftMesh, eyeRightMesh, eyeAssetMesh);

    if (!headMesh || !eyeRightMesh || !eyeLeftMesh || !eyeAssetMesh)
    {
        CARBON_CRITICAL("all meshes must be initialized");
    }
    if (eyeAssetMesh->NumQuads() > 0)
    {
        CARBON_CRITICAL("eye asset mesh must contain triangles only; please re-triangulate it");
    }
    if (headMesh->NumQuads() > 0)
    {
        CARBON_CRITICAL("head mesh must contain triangles only; please re-triangulate it");
    }
    if (!CheckAssetVertexIndices(params.wrapDeformOnlyVertexIndices))
    {
        CARBON_CRITICAL("wrap_deform_only_vertex_indices must contain indices in range for the eye asset and there must be no duplicates");
    }
    if (!CheckAssetVertexIndices(params.caruncleVertexIndices))
    {
        CARBON_CRITICAL("caruncle_vertex_indices must contain indices in range for the eye asset and there must be no duplicates");
    }

    m_params = params;
    InitializeAsset();
 }

template <class T>
void EyeAssetGenerator<T>::Apply(const Eigen::Matrix<T, 3, -1>& deformedHeadMeshVertices, const Eigen::Matrix<T, 3, -1>& deformedEyeLeftMeshVertices,
    const Eigen::Matrix<T, 3, -1>& deformedEyeRightMeshVertices, Eigen::Matrix<T, 3, -1>& deformedEyeAssetMeshVertices) const
{
    if (!m_headMesh || !m_eyeRightMesh || !m_eyeLeftMesh || !m_eyeAssetMesh)
    {
        CARBON_CRITICAL("all meshes must be initialized before we can apply the eye asset generator");
    }

    if (deformedHeadMeshVertices.cols() != m_headMesh->Vertices().cols())
    {
        CARBON_CRITICAL("supplied head mesh does not contain the correct number of vertices");
    }

    if (deformedEyeLeftMeshVertices.cols() != m_eyeLeftMesh->Vertices().cols())
    {
        CARBON_CRITICAL("supplied eye left mesh does not contain the correct number of vertices");
    }

    if (deformedEyeRightMeshVertices.cols() != m_eyeRightMesh->Vertices().cols())
    {
        CARBON_CRITICAL("supplied eye right mesh does not contain the correct number of vertices");
    }

    ApplyAsset(deformedHeadMeshVertices, deformedEyeLeftMeshVertices, deformedEyeRightMeshVertices, deformedEyeAssetMeshVertices);
}

template <class T>
void EyeAssetGenerator<T>::ApplyAsset(const Eigen::Matrix<T, 3, -1>& deformedHeadMeshVertices, const Eigen::Matrix<T, 3, -1>& deformedEyeLeftMeshVertices, const Eigen::Matrix<T, 3, -1>& deformedEyeRightMeshVertices, 
    Eigen::Matrix<T, 3, -1>& deformedAssetVertices) const
{
    //Timer timer;

    TaskFutures futures;

    // perform an initial wrap deform to get an approximate result but with artifacts
    m_wrapDeformer.Deform(deformedHeadMeshVertices, deformedAssetVertices);
    Eigen::Matrix<T, 3, -1> initialDeformedAssetVertices = deformedAssetVertices;

    auto optimizeOneSideOfEyeAsset = [&](const std::shared_ptr<const Mesh<T>>& eyeMesh, const Eigen::VectorX<T>& eyeWeights, const Eigen::VectorXi& eyeAssetVertexIndices,
        const Eigen::Matrix<T, 3, -1>& eyeAssetVertices, const Eigen::Matrix<int, 3, -1>& eyeAssetTriangles,  const Eigen::Matrix<T, 3, -1>& deformedEyeMeshVertices,
        const Eigen::VectorXi& faceCorrespondenceIndices, Eigen::Matrix<T, 3, -1>& deformedAllEyeAssetVertices)
    {
        try
        {
            EyeballConstraints<T> eyeConstraintsForAsset;
            auto eyeConfig = eyeConstraintsForAsset.GetConfiguration();
            eyeConfig["normalDisplacement"] = m_params.eyeballNormalDisplacement;
            eyeConstraintsForAsset.SetConfiguration(eyeConfig);
            eyeConstraintsForAsset.SetEyeballMesh(*eyeMesh);
            eyeConstraintsForAsset.SetInfluenceVertices(VertexWeights<T>(eyeWeights));
            eyeConstraintsForAsset.SetRestPose(eyeMesh->Vertices(), eyeAssetVertices);
            eyeConstraintsForAsset.SetEyeballPose(deformedEyeMeshVertices);
            Eigen::Matrix<T, 3, -1> curDeformedAssetVertices(3, eyeAssetVertexIndices.size());
            for (int col = 0; col < eyeAssetVertexIndices.size(); ++col)
            {
                curDeformedAssetVertices.col(col) = deformedAssetVertices.col(eyeAssetVertexIndices(col));
            }
     
            std::vector<Eigen::Matrix<T, 3, 1>> targetAssetVerticesVec;
            for (int i = 0; i < faceCorrespondenceIndices.size(); ++i)
            {
                targetAssetVerticesVec.push_back(curDeformedAssetVertices.col(faceCorrespondenceIndices(i)));
            }
    
            const Eigen::Matrix<T, 3, -1> targetAssetVertices = Eigen::Map<const Eigen::Matrix<T, 3, -1>>((const T*)targetAssetVerticesVec.data(), 3, targetAssetVerticesVec.size());
            const Eigen::Matrix<T, -1, 1> targetAssetOffsetWeights = Eigen::Matrix<T, -1, 1>::Ones(faceCorrespondenceIndices.size());
    
            DeformationModelVertex<T> defModelVertex;
            Mesh<T> eyeAssetMesh;
            eyeAssetMesh.SetVertices(eyeAssetVertices);
            eyeAssetMesh.SetTriangles(eyeAssetTriangles);
            defModelVertex.SetMeshTopology(eyeAssetMesh);
            auto defConfig = defModelVertex.GetConfiguration();
            defConfig["optimizePose"] = m_params.optimizePose;
            defConfig["vertexOffsetRegularization"] = T(0); // no vertex offsets so this should always be 0
            defModelVertex.SetConfiguration(defConfig);
            defModelVertex.SetRigidTransformation(Affine<T, 3, 3>());
            defModelVertex.SetRestVertices(eyeAssetVertices);
            defModelVertex.SetVertexOffsets(Eigen::Matrix<T, 3, -1>::Zero(3, eyeAssetVertices.cols()));
    
            std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context)
            {
                // this is a combination of not touching the eyeballs, minimizing deformation of the archetype mesh, and minimizing point to point to the target vertices
                Cost<T> cost;
                const DiffDataMatrix<T, 3, -1> vertices = defModelVertex.EvaluateVertices(context);
                cost.Add(defModelVertex.EvaluateModelConstraints(context), m_params.deformationModelVertexWeight); // vertex laplacian is slow 
                cost.Add(PointPointConstraintFunction<T, 3>::Evaluate(vertices, faceCorrespondenceIndices, targetAssetVertices, targetAssetOffsetWeights, 1.0f), m_params.pointToPointConstraintWeight);
                cost.Add(eyeConstraintsForAsset.EvaluateEyeballConstraints(vertices), m_params.eyeConstraintWeight);
                return cost.CostToDiffData();
            };
    
            GaussNewtonSolver<T> assetSolver;
            //const T startEnergy = evaluationFunction(nullptr).Value().squaredNorm();
    
            Context<T> context;
            typename GaussNewtonSolver<T>::Settings settings;
            settings.iterations = m_params.numSolverIterations;
            settings.cgIterations = m_params.numSolverCGIterations;
            if (!assetSolver.Solve(evaluationFunction, context, settings))
            {
                LOG_ERROR("could not solve optimization problem");
                return;
            }
            //const T finalEnergy = evaluationFunction(nullptr).Value().squaredNorm();
            //LOG_INFO("asset energy changed from {} to {}", startEnergy, finalEnergy);
    
            curDeformedAssetVertices = defModelVertex.EvaluateVertices(nullptr).Matrix();
    
            for (int i = 0; i < curDeformedAssetVertices.cols(); ++i)
            {
                deformedAllEyeAssetVertices.col(eyeAssetVertexIndices(i)) = curDeformedAssetVertices.col(i);
            }
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("error during eye asset generation: {}", e.what());
        }
    };


    futures.Add(m_taskThreadPool->AddTask([&]()
        { 
            optimizeOneSideOfEyeAsset(m_eyeRightMesh, m_eyeRightWeights, m_rightAssetVertexIndices, m_rightAssetVertices, m_rightAssetTriangles, deformedEyeRightMeshVertices, m_rightFaceCorrespondenceIndices, deformedAssetVertices);
        }));

    
    futures.Add(m_taskThreadPool->AddTask([&]()
        { 
            optimizeOneSideOfEyeAsset(m_eyeLeftMesh, m_eyeLeftWeights, m_leftAssetVertexIndices, m_leftAssetVertices, m_leftAssetTriangles, deformedEyeLeftMeshVertices, m_leftFaceCorrespondenceIndices, deformedAssetVertices); 
        }));

    futures.Wait();


    // set the edge vertices back to those from the original wrap deformer results
    for (int i = 0; i < m_params.wrapDeformOnlyVertexIndices.size(); ++i)
    {
        deformedAssetVertices.col(m_params.wrapDeformOnlyVertexIndices(i)) = initialDeformedAssetVertices.col(m_params.wrapDeformOnlyVertexIndices(i));
    }

    //LOG_INFO("time to apply eye asset: {} ms", timer.Current());
}

template <class T>
bool ToBinaryFile(FILE* pFile, const EyeAssetGenerator<T>& eyeAssetGenerator)
{
    bool success = true;
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_version);
       
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_headMesh);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_eyeLeftMesh);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_eyeRightMesh);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_eyeAssetMesh);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_eyeLeftWeights);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_eyeRightWeights);
    // set the meshes for the wrapDeformer to nullptrs for saving
    auto wrapDeformer = eyeAssetGenerator.m_wrapDeformer;
    wrapDeformer.SetMeshes(nullptr, nullptr);
    success &= ToBinaryFile(pFile, wrapDeformer);
    success &= ToBinaryFile(pFile, eyeAssetGenerator.m_params);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_rightAssetVertexIndices);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_rightAssetVertices);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_rightAssetTriangles);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_rightFaceCorrespondenceIndices);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_leftAssetVertexIndices);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_leftAssetVertices);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_leftAssetTriangles);
    success &= io::ToBinaryFile(pFile, eyeAssetGenerator.m_leftFaceCorrespondenceIndices);

    return success;
}

template <class T>
bool FromBinaryFile(FILE* pFile, EyeAssetGenerator<T>& eyeAssetGenerator)
{
    bool success = true;
    int32_t version;
    success &= io::FromBinaryFile<int32_t>(pFile, version);
    if (success && version == 1)
    {
        std::shared_ptr<Mesh<T>> headMesh, eyeLeftMesh, eyeRightMesh, eyeAssetMesh;

        success &= io::FromBinaryFile(pFile, headMesh);
        eyeAssetGenerator.m_headMesh = headMesh;
        success &= io::FromBinaryFile(pFile, eyeLeftMesh);
        eyeAssetGenerator.m_eyeLeftMesh = eyeLeftMesh;
        success &= io::FromBinaryFile(pFile, eyeRightMesh);
        eyeAssetGenerator.m_eyeRightMesh = eyeRightMesh;
        success &= io::FromBinaryFile(pFile, eyeAssetMesh);
        eyeAssetGenerator.m_eyeAssetMesh = eyeAssetMesh;
        success &= io::FromBinaryFile(pFile, eyeAssetGenerator.m_eyeLeftWeights);
        success &= io::FromBinaryFile(pFile, eyeAssetGenerator.m_eyeRightWeights);
        success &= FromBinaryFile(pFile, eyeAssetGenerator.m_wrapDeformer);
        // set the meshes in the wrapDeformer
        eyeAssetGenerator.m_wrapDeformer.SetMeshes(eyeAssetGenerator.m_headMesh, eyeAssetGenerator.m_eyeAssetMesh);
        success &= FromBinaryFile(pFile, eyeAssetGenerator.m_params);
        success &= io::FromBinaryFile(pFile, eyeAssetGenerator.m_rightAssetVertexIndices);
        success &= io::FromBinaryFile(pFile, eyeAssetGenerator.m_rightAssetVertices);
        success &= io::FromBinaryFile(pFile, eyeAssetGenerator.m_rightAssetTriangles);
        success &= io::FromBinaryFile(pFile, eyeAssetGenerator.m_rightFaceCorrespondenceIndices);
        success &= io::FromBinaryFile(pFile, eyeAssetGenerator.m_leftAssetVertexIndices);
        success &= io::FromBinaryFile(pFile, eyeAssetGenerator.m_leftAssetVertices);
        success &= io::FromBinaryFile(pFile, eyeAssetGenerator.m_leftAssetTriangles);
        success &= io::FromBinaryFile(pFile, eyeAssetGenerator.m_leftFaceCorrespondenceIndices);
    }
    else
    {
        success = false;
    }
    return success;
}


template class EyeAssetGenerator<float>;
template class EyeAssetGenerator<double>;

template struct EyeAssetGeneratorParams<float>;
template struct EyeAssetGeneratorParams<double>;

template bool ToBinaryFile(FILE* pFile, const EyeAssetGeneratorParams<float>& params);
template bool ToBinaryFile(FILE* pFile, const EyeAssetGeneratorParams<double>& params);

template bool FromBinaryFile(FILE* pFile, EyeAssetGeneratorParams<float>& params);
template bool FromBinaryFile(FILE* pFile, EyeAssetGeneratorParams<double>& params);

template bool ToBinaryFile(FILE* pFile, const EyeAssetGenerator<float>& eyeAssetGenerator);
template bool ToBinaryFile(FILE* pFile, const EyeAssetGenerator<double>& eyeAssetGenerator);

template bool FromBinaryFile(FILE* pFile, EyeAssetGenerator<float>& eyeAssetGenerator);
template bool FromBinaryFile(FILE* pFile, EyeAssetGenerator<double>& eyeAssetGenerator);


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
