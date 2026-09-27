// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <carbon/io/JsonIO.h>
#include <nls/Context.h>
#include <nls/DiffDataMatrix.h>
#include <nls/VectorVariable.h>

#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * IdentityBlendModel models the geometry of a face as
 * Vertices = MeanShape + sum_regions (B_region x_region)
 * where B_region is the linear model for a region, and x_region are the coefficients.
 */
template <class T, int D = 3>
class IdentityBlendModel
{
public:
    struct RegionData
    {
        std::string regionName;
        Eigen::Matrix<T, -1, -1> modes;
        Eigen::VectorXi vertexIDs;
        Eigen::VectorX<T> weights;
        std::vector<std::string> modeNames;
    };

public:
    IdentityBlendModel();
    ~IdentityBlendModel();
    IdentityBlendModel(IdentityBlendModel&& other);
    IdentityBlendModel(const IdentityBlendModel& other) = delete;
    IdentityBlendModel& operator=(IdentityBlendModel&& other);
    IdentityBlendModel& operator=(const IdentityBlendModel& other) = delete;

    std::shared_ptr<IdentityBlendModel> Reduce(const std::vector<int>& vertexIds) const;

    //! Set the model data
    void SetModel(Eigen::Matrix<T, D, -1>&& mean, std::vector<RegionData>&& regionsData);

    // void UpdateModelWithScale(const Eigen::VectorX<T>& perRegionScale, const std::vector<bool>& scaleIds);

    /**
     * Loads the identity blend model from a json file of the following format:
     *
     * format of identity_model.json:
     * {
     *   'mean': [-1xN matrix of the model]
     *   'regions': {
     *      'region name': {
     *          'vertex_ids': [list of vertex indices],
     *          'weights': [weights per vertex],
     *          'modes': (-1 * num vertexIDs in region, num modes), // the modes still need to be multiplied by the weights
     *           'mode names': [mode 1, mode 2, ...] (optional)
     *       }, ...
     *    }
     * }
     *
     * Note that the weights for a region are also integrated into the PCA model of each region, so there is
     * no need to apply the weights to the model.
     */

    //! Load model from json file
    void LoadModelJson(const std::string& identityModelFile);

    //! Load model from JsonElement object
    void LoadModelJson(const TITAN_NAMESPACE::JsonElement& identityJson);

    //! Save model to json file
    void SaveModelJson(const std::string& identityModelFile) const;

    //! @returns the model as JsonElement object
    TITAN_NAMESPACE::JsonElement SaveModelJson() const;

    //! Save model to binary file
    void SaveModelBinary(const std::string& filename) const;

    //! Load model from binary file
    bool LoadModelBinary(const std::string& identityModelFile);

    //! @returns the number of parameters of the model
    int NumParameters() const;

    //! @returns the number of regions in the model
    int NumRegions() const;

    //! @returns the number of vertices in the model
    int NumVertices() const;

    //! @returns default parameters resulting in an average face
    Vector<T> DefaultParameters() const;

    //! @returns the base shape
    const Eigen::Matrix<T, D, -1>& Base() const;

    //! @returns the model matrix
    SparseMatrixConstPtr<T> ModelMatrix() const;

    //! @returns the evaluated model for the parameters
    Eigen::Matrix<T, D, -1> Evaluate(const Vector<T>& parameters) const;

    //! @returns the evaluated model for the parameters
    DiffDataMatrix<T, D, -1> Evaluate(const DiffData<T>& parameters) const;

    //! @returns evaluates the regularization on the parameters
    DiffData<T> EvaluateRegularization(const DiffData<T>& parameters) const;

    //! @returns the name of the region at index @p regionIndex in the model.
    const std::string& RegionName(int regionIndex) const;

    //! @returns the name of the modes for a certain region.
    const std::vector<std::string>& ModeNames(int regionIndex) const;

    //! @returns for each region the indeces [startIndex, endIndex) in the parameter array.
    const std::vector<std::pair<int, int>>& RegionRanges() const;

    //! @returns the vertex ids used for a certain region.
    const Eigen::VectorXi& RegionVertexIds(int regionIndex) const;

    //! @returns the blending weights for a certain region.
    const Eigen::VectorX<T>& RegionWeights(int regionIndex) const;

    //! @returns the mode matrix for a certain region.
    const Eigen::Matrix<T, -1, -1>& RegionModes(int regionIndex) const;

private:
    void UpdateBlendModelMatrix();

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

/**
 * IdentityBlendModel class extension to accommodate separate global joints and global blendshapes models
 */
template<class T, int D = 3>
class GlobalExpressionPCAModel : public IdentityBlendModel<T, D>
{
public:
    /**
     * @brief Load joints and blendshapes models from binary file and create IdentityBlendModel that unifies the two models
     * 
     * @param[in] jointsPCAModelFile - joints pca model path
     * @param[in] blendshapesPCAModelFile - blendshapes pca model path
     */
    void LoadModelBinary(const std::string &jointsPCAModelFile, const std::string &blendshapesPCAModelFile);

    void LoadModelBinary(const std::string& jointsPCAModelFile);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
