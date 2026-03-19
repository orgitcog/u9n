// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nls/geometry/Affine.h>
#include <nls/geometry/Mesh.h>
#include <rig/Rig.h>
#include <nls/utils/Configuration.h>
#include <nls/utils/ConfigurationParameter.h>
#include <nrr/VertexWeights.h>

#include <dna/Reader.h>

#include <map>
#include <memory>
#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Module to align teeth asset in the autorig using the reference rig as model
 *
 * Implemented for T=float and T=double
 */
template <class T>
class TeethAlignment
{
public:
    TeethAlignment();
    ~TeethAlignment();
    TeethAlignment(TeethAlignment&& o);
    TeethAlignment(const TeethAlignment& o) = delete;
    TeethAlignment& operator=(TeethAlignment&& o);
    TeethAlignment& operator=(const TeethAlignment& o) = delete;

    void LoadRig(dna::Reader* referentRig, dna::Reader* targetRig);
    void SetRig(const std::shared_ptr<Rig<T>>& referentRig, const std::shared_ptr<Rig<T>>& targetRig);

    //! get/set the model registration settings (identity PCA model)
    const Configuration& RegistrationConfiguration() const { return m_teethPlacementConfig; }
    Configuration& RegistrationConfiguration() { return m_teethPlacementConfig; }

    void SetControlsToEvaluate(const std::vector<Eigen::VectorX<T>>& controls);
    void SetControlsToEvaluate(const std::vector<std::map<std::string, float>>& controls);
    bool CheckControlsConfig(const std::string& configFilenameOrData);
    void LoadControlsToEvaluate(const std::string& InConfigurationFilenameOrData);

    const Eigen::Matrix<T, 3, -1> CurrentVertices() const;

    void SetInterfaceVertices(const VertexWeights<T>& mask);

    /**
     * Run rigid registration.
     * @param inAffine                        The (current) affine transformation of the source to target.
     * @param inScale                         The (current) scale of the source to target.
     * @param numIterations                   The number of iterations for rigid registration.
     */
    std::pair<T, Affine<T, 3, 3>> Align(const Affine<T, 3, 3>& inAffine, T inScale = T(1), int numIterations = 10);

private:
    Configuration m_teethPlacementConfig = { std::string("Teeth Rig Placement Configuration"), {
                                                 //!< how much weight to use on geometry constraint
                                                 { "geometryWeight", ConfigurationParameter(T(1), T(0), T(10)) },
                                                 // ! whether to fix the rotation part
                                                 { "fixRotation", ConfigurationParameter(false) },
                                                 // ! whether to fix the transaltion part
                                                 { "fixTranslation", ConfigurationParameter(false) },
                                                 // ! whether to fix the scale part
                                                 { "fixScale", ConfigurationParameter(false) },
                                                 // ! whether to use closest point or ray intersection for correspondence search
                                                 { "useClosestPoint", ConfigurationParameter(true) },
                                             } };

    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
