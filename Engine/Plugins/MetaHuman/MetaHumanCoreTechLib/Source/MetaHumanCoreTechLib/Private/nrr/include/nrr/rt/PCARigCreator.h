// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <rig/Rig.h>
#include <nrr/VertexWeights.h>
#include <nrr/rt/HeadState.h>
#include <nrr/rt/PCARig.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4324)
#endif

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rt)

//! Class to create a PCA rig from a DNA rig.
class PCARigCreator
{
public:
    struct RigTrainingData
    {
        std::string name;
        bool useForTraining = true;
        bool useForNeck = false;
        std::vector<int> frameNumbers;
        std::vector<Eigen::VectorXf> rawControls;
        std::vector<Eigen::VectorXf> guiControls;
    };

    struct MeshTrainingData
    {
        std::string name;
        bool useForTraining = true;
        std::vector<int> frameNumbers;
        std::vector<rt::HeadVertexState<float>> headVertexStates;
    };

public:
    PCARigCreator(const std::string& dnaFilename, const std::string& configFilename);
    PCARigCreator(std::shared_ptr<const Rig<float>> rig);

    //! Loads the configuration for creating a PCA model from a rig; may also contain a Json string containing the config data
    bool LoadConfig(const std::string& configFilenameOrData);

    /**
     * @brief Create the PCA model for the input dna and the current config.
     *
     * @param maxModes  The maximum number of modes to be used for the PCA model. If negative, then the
     *                  maximum number of modes is as defined in config file (and 100 if not defined in the config).
     * @return True is PCA model was created, false otherwise.
     */
    bool Create(int maxModes = -1);

    //! @returns the number of animation sample sets. Includes also the sample set for all facial expressions.
    int NumRigTrainingSampleSets() const { return int(m_rigControlTrainingSamples.size()); }

    //! Load training samples from a qsa file @p qsaFilename for face pca
    bool LoadRigTrainingSamples(const std::string& qsaFilename);

    //! Load training samples from a qsa file @p qsaFilename for neck pca
    bool LoadRigNeckTrainingSamples(const std::string& qsaFilename);

    //! Save the training samples at index @p idx to file @p qsaFilename.
    void SaveRigTrainingSamples(int idx, const std::string& qsaFilename) const;

    //! Create an empty traing samples set.
    void CreateRigTrainingSamples(const std::string& name);

    //! Create an empty traing samples set.
    void CreateRigNeckTrainingSamples(const std::string& name);

    //! Training samples
    /* */std::vector<RigTrainingData>& RigTrainingSamples()/* */{ return m_rigControlTrainingSamples; }
    const std::vector<RigTrainingData>& RigTrainingSamples() const { return m_rigControlTrainingSamples; }

    //! @returns the number of mesh training sample sets.
    int NumMeshTrainingSampleSets() const { return int(m_meshTrainingSamples.size()); }

    //! Load training samples from a directory @p dirname
    bool LoadMeshTrainingSamples(const std::string& dirname, bool recursive, bool isNumpy, float rotAngle);

    //! Set training samples from runtime
    void SetMeshTrainingSamples(const std::vector<std::vector<Eigen::Matrix<float, 3, -1>>>& trainingMeshes, float rotAngle);

    //! Training samples
    /* */std::vector<MeshTrainingData>& MeshTrainingSamples()/* */{ return m_meshTrainingSamples; }
    const std::vector<MeshTrainingData>& MeshTrainingSamples() const { return m_meshTrainingSamples; }

    //! @returns the index of the gui control @p guiControlName.
    int GuiControlIndex(const std::string& guiControlName) const;

    //! @returns the index of the raw control @p rawControlName.
    int RawControlIndex(const std::string& rawControlName) const;

    //! @returns the default raw controls
    Eigen::VectorXf DefaultRawControls() const;

    //! evaluate the face on the rig
    rt::HeadVertexState<float> EvaluateExpression(const Eigen::VectorXf& rawControls) const;

    const std::shared_ptr<const Rig<float>>& GetRig() const { return m_rig; }
    const rt::PCARig& GetPCARig() const { return m_pcaRig; }

    //! load mask for the face vertices
    void LoadFaceMask(const std::string& filename, const std::string& maskName = "");

    //! @returns the face mask
    VertexWeights<float> FaceMask() const { return m_faceMask; }

private:
    //! Create the Face PCA Rig
    void CreateFacePCARig(Eigen::VectorXf& mean,
                          Eigen::Matrix<float, -1, -1, Eigen::RowMajor>& modes,
                          std::vector<int>& offsetsPerModel,
                          const Eigen::Transform<float, 3, Eigen::Affine>& defaultRootTransform);

    //! Load training samples from a qsa file @p qsaFilename
    bool LoadRigTrainingSamples(const std::string& qsaFilename, bool useForNeck);

    //! @returns all expressions for the face
    std::vector<Eigen::VectorXf> AllFaceExpressions() const;

    //! @returns all neck expressions for the face
    std::vector<Eigen::VectorXf> AllNeckExpressions() const;

    // @returns data matrix from head vertex states
    Eigen::Matrix<float, -1, -1> GetFaceDataMatrix(std::vector<rt::HeadVertexState<float>>& headVertexStates);

    // @returns data matrix from head vertex states for neck region
    Eigen::Matrix<float, -1, -1> GetNeckDataMatrix(std::vector<rt::HeadVertexState<float>>& headVertexStates, const int numFaceVertices, Eigen::Index maxModes);

    // @calculate pca from data matrix
    std::pair<Eigen::VectorXf, Eigen::Matrix<float, -1, -1, Eigen::RowMajor>> CreatePCAFromDataMatrix(Eigen::Matrix<float, -1, -1>& dataMatrix, int maxModes);

private:
    std::map<std::string, float> m_defaultRawControls;
    std::set<std::string> m_fixedControls;
    std::set<std::string> m_neckControls;
    std::shared_ptr<const Rig<float>> m_rig;

    rt::PCARig m_pcaRig;
    rt::RigidBody<float> m_eyeLeftBody;
    rt::RigidBody<float> m_eyeRightBody;

    std::vector<int> m_meshIndices;
    int m_headMeshIndex;
    int m_teethMeshIndex;
    int m_eyeLeftMeshIndex;
    int m_eyeRightMeshIndex;

    int m_defaultMaxModes = 100;

    std::vector<RigTrainingData> m_rigControlTrainingSamples;
    std::vector<MeshTrainingData> m_meshTrainingSamples;

    VertexWeights<float> m_faceMask;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rt)

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
