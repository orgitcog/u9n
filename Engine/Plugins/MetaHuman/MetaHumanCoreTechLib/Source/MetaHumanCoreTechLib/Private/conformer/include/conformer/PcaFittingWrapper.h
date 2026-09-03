// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nls/geometry/Affine.h>
#include <nls/geometry/Camera.h>
#include <nls/geometry/DepthmapData.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/MeshCorrespondenceSearch.h>
#include <nls/utils/Configuration.h>
#include <nrr/MeshLandmarks.h>
#include <nrr/VertexWeights.h>
#include <nrr/landmarks/LandmarkConfiguration.h>
#include <nrr/landmarks/LandmarkConstraints2D.h>
#include <nrr/landmarks/LandmarkConstraints3D.h>
#include <nrr/landmarks/LandmarkInstance.h>
#include <nrr/rt/PCARig.h>
#include <nrr/FlowConstraints.h>

#include <dna/Reader.h>
#include <dna/Writer.h>

#include <map>
#include <memory>
#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Module for fitting PCA model to input data.
 *
 * Implemented for T=float
 */
template <class T>
class PcaRigFitting
{
public:
    PcaRigFitting();
    ~PcaRigFitting();
    PcaRigFitting(PcaRigFitting&& o);
    PcaRigFitting(const PcaRigFitting& o) = delete;
    PcaRigFitting& operator=(PcaRigFitting&& o);
    PcaRigFitting& operator=(const PcaRigFitting& o) = delete;

    void LoadRig(dna::Reader* dnaReader);

    void SaveRig(dna::Writer* dnaWriter);

    bool IsRigLoaded() const;
    bool HasNeckPCA() const;

    void SetFlowConstraints(const std::map<std::string, std::shared_ptr<const FlowConstraintsData<T>>>& flowConstraintsData);
    bool HasFlowConstraints() const;

    rt::PCARig GetRig();

    //! get/set the model registration settings (identity PCA model)
    const Configuration& PcaRigFittingRegistrationConfiguration() const { return m_pcaRigFittingConfig; }
    Configuration& PcaRigFittingRegistrationConfiguration() { return m_pcaRigFittingConfig; }
    void ResetPcaRigFittingRegistrationConfiguration() { m_pcaRigFittingConfig = s_defaultPcaRigFittingConfig; }

    //! Set the target mesh
    void SetTargetMeshes(const std::vector<std::shared_ptr<const Mesh<T>>>& targetMeshes, const std::vector<Eigen::VectorX<T>>& targetWeights);

    //! Set the target depth
    void SetTargetDepths(const std::vector<std::vector<std::shared_ptr<const DepthmapData<T>>>>& targetDepths);

    void SetTopology(const Mesh<T>& topology);

    void SetInnerLipInterfaceVertices(const VertexWeights<T>& maskUpperLip, const VertexWeights<T>& maskLowerLip);

    //! Sets the mesh landmarks that are use for registration
    void SetMeshLandmarks(const MeshLandmarks<T>& headMeshLandmarks,
                          const MeshLandmarks<T>& teethMeshLandmarks,
                          const MeshLandmarks<T>& eyeLeftMeshLandmarks,
                          const MeshLandmarks<T>& eyeRightMeshLandmarks);

    //! Set the target 2D landmarks
    void SetTarget2DLandmarks(const std::vector<std::vector<std::pair<LandmarkInstance<T, 2>, Camera<T>>>>& landmarks);

    //! Sets the current face vertices and estimate the pca coefficients
    void SetCurrentVertices(const Eigen::Matrix<T, 3, -1>& headVertices);

    //! @return the estimated vertices
    Eigen::Matrix<T, 3, -1> CurrentVertices(const int meshId = 0);

    /**
     * PCA rig registration using the PCA rig model
     */
    std::vector<Affine<T, 3, 3>> RegisterPcaRig(const std::vector<Affine<T, 3, 3>>& source2target,
                                                const VertexWeights<T>& faceSearchWeights,
                                                const VertexWeights<T>& neckSearchWeights,
                                                int numIterations = 10);

private:
    void InitIcpConstraints(int numOfObservations);
    void Init2DLandmarksConstraints(int numOfObservations);

    void UpdateIcpConfiguration(const Configuration& targetConfig);
    void Update2DLandmarkConfiguration(const Configuration& targetConfig);
    void UpdateLipClosureConfiguration(const Configuration& targetConfig);
    void UpdateIcpWeights(const VertexWeights<T>& weights);

private:
    static inline Configuration s_defaultPcaRigFittingConfig = { std::string("PCA Rig Fitting Configuration"), {
                                                // ! whether to use distance threshold
                                                { "useDistanceThreshold", ConfigurationParameter(true) },
                                                // ! whether to use distance threshold
                                                { "useFlowConstraints", ConfigurationParameter(false) },
                                                // ! whether to optimize rigid transform
                                                { "optimizePose", ConfigurationParameter(true) },
                                                //!< how much weight to use on geometry constraint
                                                { "geometryWeight", ConfigurationParameter(T(1), T(0), T(1)) },
                                                //!< how much weight to use on geometry constraint
                                                { "flowWeight", ConfigurationParameter(T(0.001), T(0), T(1)) },
                                                //!< adapt between point2surface constraint (point2point = 0) to point2point constraint (point2point = 1)
                                                { "point2point", ConfigurationParameter(T(0), T(0), T(1)) },
                                                //!< how much weight to use on landmark constraints
                                                { "landmarksWeight", ConfigurationParameter(T(0.001), T(0), T(0.1)) },
                                                //!< how much weight to use on inner lip constraints
                                                { "innerLipWeight", ConfigurationParameter(T(0.01), T(0), T(0.1)) },
                                                //!< how much weight to use on lip closure  constraints
                                                { "lipClosureWeight", ConfigurationParameter(T(0), T(0), T(10)) },
                                                //!< minimum distance threshold value - if used
                                                { "minimumDistanceThreshold", ConfigurationParameter(T(5), T(0), T(10)) },
                                                //!< how much weight to use on inner lip constraints
                                                { "velocity", ConfigurationParameter(T(0.1), T(0), T(0.1)) },
                                                //!< minimum distance threshold value - if used
                                                { "acceleration", ConfigurationParameter(T(0.2), T(0), T(1)) },
                                                //!< minimum distance threshold value - if used
                                                { "regularization", ConfigurationParameter(T(0.05), T(0), T(1)) },
                                                //!< resampling of curves
                                                { "curveResampling", ConfigurationParameter(1, 5, 1) },
                                                //!< minimum distance threshold value - if used
                                                { "neckRegularization", ConfigurationParameter(T(1.0), T(0), T(1)) }
                                            } };

    Configuration m_pcaRigFittingConfig = s_defaultPcaRigFittingConfig;

    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
