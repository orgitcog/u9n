// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/rt/PCARigCreator.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>
#include <carbon/utils/Timer.h>
#include <nls/geometry/IncrementalPCA.h>
#include <nls/geometry/PCA.h>
#include <carbon/io/NpyFileFormat.h>
#include <nls/serialization/QsaSerialization.h>
#include <nls/serialization/ObjFileFormat.h>

#include <filesystem>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rt)

bool PCARigCreator::LoadConfig(const std::string& configFilenameOrData)
{
    try
    {
        const bool isValidFile = std::filesystem::exists(configFilenameOrData);
        std::string jsonString;
        if (isValidFile)
        {
            jsonString = ReadFile(configFilenameOrData);
        }
        else
        {
            jsonString = configFilenameOrData;
        }

        const TITAN_NAMESPACE::JsonElement configJson = TITAN_NAMESPACE::ReadJson(jsonString);

        m_defaultRawControls = configJson["default_raw_controls"].Get<std::map<std::string, float>>();

        const auto& fixedControls = configJson["fixed_raw_controls"].Get<std::vector<std::string>>();
        m_fixedControls = std::set(fixedControls.begin(), fixedControls.end());
        // raw controls that are not affected by GUI controls are set as fixed
        const std::vector<int> unusedRawControls = m_rig->GetRigLogic()->UnusedRawControls();
        for (auto rawControl : unusedRawControls)
        {
            m_fixedControls.insert(m_rig->GetRigLogic()->RawControlNames()[rawControl]);
        }

        const auto& neckControls = configJson["neck_raw_controls"].Get<std::vector<std::string>>();
        m_neckControls = std::set(neckControls.begin(), neckControls.end());

        if (configJson.Contains("default_max_modes"))
        {
            m_defaultMaxModes = configJson["default_max_modes"].Get<int>();
        }

        m_headMeshIndex = m_rig->GetRigGeometry()->GetMeshIndex(rt::HeadMeshName());
        m_teethMeshIndex = m_rig->GetRigGeometry()->GetMeshIndex(rt::TeethMeshName());
        m_eyeLeftMeshIndex = m_rig->GetRigGeometry()->GetMeshIndex(rt::EyeLeftMeshName());
        m_eyeRightMeshIndex = m_rig->GetRigGeometry()->GetMeshIndex(rt::EyeRightMeshName());

        m_meshIndices.clear();
        m_meshIndices = { m_headMeshIndex, m_teethMeshIndex, m_eyeLeftMeshIndex, m_eyeRightMeshIndex };

        {
            const int eyeLeftJointIndex = m_rig->GetRigGeometry()->GetJointRig().GetJointIndex(rt::EyeLeftJointName());
            const Eigen::Transform<float, 3, Eigen::Affine> eyeLeftBindPose(m_rig->GetRigGeometry()->GetBindMatrix(eyeLeftJointIndex));
            m_eyeLeftBody.baseTransform = eyeLeftBindPose;
            m_eyeLeftBody.baseVertices = eyeLeftBindPose.inverse() * m_rig->GetRigGeometry()->GetMesh(m_eyeLeftMeshIndex).Vertices();
        }
        {
            const int eyeRightJointIndex = m_rig->GetRigGeometry()->GetJointRig().GetJointIndex(rt::EyeRightJointName());
            const Eigen::Transform<float, 3, Eigen::Affine> eyeRightBindPose(m_rig->GetRigGeometry()->GetBindMatrix(eyeRightJointIndex));
            m_eyeRightBody.baseTransform = eyeRightBindPose;
            m_eyeRightBody.baseVertices = eyeRightBindPose.inverse() * m_rig->GetRigGeometry()->GetMesh(m_eyeRightMeshIndex).Vertices();
        }

        // create the rig control data from the facial expressions
        RigTrainingData rigControlData;
        rigControlData.name = "combination_shapes";
        rigControlData.rawControls = AllFaceExpressions();
        rigControlData.guiControls.resize(rigControlData.rawControls.size());

        std::vector<int> inconsistentControls;
        for (size_t i = 0; i < rigControlData.guiControls.size(); ++i)
        {
            rigControlData.guiControls[i] = m_rig->GetRigLogic()->GuiControlsFromRawControls(rigControlData.rawControls[i], inconsistentControls);
            rigControlData.frameNumbers.push_back(int(i));
        }
        if (rigControlData.frameNumbers.size() > 0)
        {
            m_rigControlTrainingSamples.emplace_back(std::move(rigControlData));
        }
    }
    catch (std::exception&)
    {
        return false;
    }

    return true;
}

PCARigCreator::PCARigCreator(const std::string& dnaFilename, const std::string& configFilename)
{
    // load the DNA
    std::shared_ptr<Rig<float>> rig = std::make_shared<Rig<float>>();
    if (!rig->LoadRig(dnaFilename, /*withJointScaling=*/false))
    {
        CARBON_CRITICAL("failed to load riglogic from dnafile {}", dnaFilename);
    }
    m_rig = rig;

    if (!LoadConfig(configFilename))
    {
        CARBON_CRITICAL("failed to load PCA rig creator config file {}", configFilename);
    }
}

PCARigCreator::PCARigCreator(std::shared_ptr<const Rig<float>> rig)
{
    m_rig = rig;
}

bool PCARigCreator::Create(int maxModes)
{
    Timer timer;

    maxModes = (maxModes >= 0 ? maxModes : m_defaultMaxModes);

    std::vector<int> offsetsPerModel;
    offsetsPerModel.push_back(0);
    offsetsPerModel.push_back(m_rig->GetRigGeometry()->GetMesh(m_headMeshIndex).NumVertices() + offsetsPerModel.back());
    offsetsPerModel.push_back(m_rig->GetRigGeometry()->GetMesh(m_teethMeshIndex).NumVertices() + offsetsPerModel.back());
    offsetsPerModel.push_back(m_rig->GetRigGeometry()->GetMesh(m_eyeLeftMeshIndex).NumVertices() + offsetsPerModel.back());
    offsetsPerModel.push_back(m_rig->GetRigGeometry()->GetMesh(m_eyeRightMeshIndex).NumVertices() + offsetsPerModel.back());

    // collect all raw controls
    std::vector<Eigen::VectorXf> rawControlsForAllExpressions;
    for (int i = 0; i < NumRigTrainingSampleSets(); ++i)
    {
        if (RigTrainingSamples()[i].useForTraining && !RigTrainingSamples()[i].useForNeck)
        {
            rawControlsForAllExpressions.insert(rawControlsForAllExpressions.end(),
                                                RigTrainingSamples()[i].rawControls.begin(),
                                                RigTrainingSamples()[i].rawControls.end());
        }
    }

    // evaluate the meshes
    std::vector<rt::HeadVertexState<float>> headVertexStates;
    for (size_t i = 0; i < rawControlsForAllExpressions.size(); ++i)
    {
        headVertexStates.emplace_back(EvaluateExpression(rawControlsForAllExpressions[i]));
    }
    LOG_INFO("time to evalute expressions: {}", timer.Current());
    timer.Restart();

    Eigen::Matrix<float, -1, -1> dataMatrix(headVertexStates.size(), 0);
    dataMatrix = GetFaceDataMatrix(headVertexStates);
    LOG_INFO("time to create data matrix: {}", timer.Current());
    timer.Restart();
    // calculate face pca model from rig data
    std::pair<Eigen::VectorXf, Eigen::Matrix<float, -1, -1, Eigen::RowMajor>> pca = CreatePCAFromDataMatrix(dataMatrix, maxModes);

    Eigen::VectorXf mean = pca.first;
    Eigen::Matrix<float, -1, -1, Eigen::RowMajor> modes = pca.second;

    LOG_INFO("time to create pca: {}", timer.Current());
    timer.Restart();

    const int numFaceVertices = (offsetsPerModel[1] - offsetsPerModel[0]);
    const int facialRootIndex = m_rig->GetRigGeometry()->GetJointRig().GetJointIndex(rt::FacialRootJointName());
    const Eigen::Transform<float, 3, Eigen::Affine> defaultRootTransform(m_rig->GetRigGeometry()->GetBindMatrix(facialRootIndex));

    // create face pca model from rig data
    CreateFacePCARig(mean, modes, offsetsPerModel, defaultRootTransform);

    // collect raw neck controls
    std::vector<Eigen::VectorXf> rawControlsForNeckExpressions;
    for (size_t i = 0; i < RigTrainingSamples().size(); ++i) {
        if (RigTrainingSamples()[i].useForTraining && RigTrainingSamples()[i].useForNeck)
        {
            rawControlsForNeckExpressions.insert(rawControlsForNeckExpressions.end(),
                                                 RigTrainingSamples()[i].rawControls.begin(),
                                                 RigTrainingSamples()[i].rawControls.end());
        }
    }

    if (rawControlsForNeckExpressions.size() > 0) {
        // evaluate the neck meshes
        std::vector<rt::HeadVertexState<float>> headVertexStatesNeck;
        // evaluate expressions and apply inverse transform to stabilize the head
        for (size_t i = 0; i < rawControlsForNeckExpressions.size(); ++i)
        {
            const rt::HeadVertexState<float> headVertexState = EvaluateExpression(rawControlsForNeckExpressions[i]);
            rt::HeadVertexState<float> stabilizedHeadVertexState;
            const Eigen::Transform<float, 3, Eigen::Affine> rootTransform(m_rig->GetRigGeometry()->GetBindMatrix(facialRootIndex));
            const Eigen::Transform<float, 3, Eigen::Affine> toDefault = defaultRootTransform * rootTransform.inverse();
            stabilizedHeadVertexState.faceVertices = toDefault * headVertexState.faceVertices;
            stabilizedHeadVertexState.eyeLeftVertices = toDefault * headVertexState.eyeLeftVertices;
            stabilizedHeadVertexState.eyeRightVertices = toDefault * headVertexState.eyeRightVertices;
            stabilizedHeadVertexState.teethVertices = toDefault * headVertexState.teethVertices;
            headVertexStatesNeck.emplace_back(stabilizedHeadVertexState);
        }

        Eigen::Matrix<float, -1, -1> neckDataMatrix = GetNeckDataMatrix(headVertexStatesNeck, numFaceVertices, modes.cols());
        LOG_INFO("time to evalute neck expressions: {}", timer.Current());
        timer.Restart();

        // calculate neck pca model from rig data
        std::pair<Eigen::VectorXf, Eigen::Matrix<float, -1, -1, Eigen::RowMajor>> pcaNeck = CreatePCAFromDataMatrix(neckDataMatrix, maxModes);
        Eigen::VectorXf neckMean = pcaNeck.first;
        Eigen::Matrix<float, -1, -1, Eigen::RowMajor> neckModes = pcaNeck.second;


        const auto neckMeanShape = Eigen::Map<const Eigen::Matrix<float, 3, -1>>(neckMean.segment(3 * offsetsPerModel[0], 3 * numFaceVertices).data(), 3, numFaceVertices);
        m_pcaRig.neckPCA.Create(neckMeanShape, neckModes.block(3 * offsetsPerModel[0], 0, 3 * numFaceVertices, neckModes.cols()));
        LOG_INFO("time to create neck PCA: {}", timer.Current());
        timer.Restart();

        // add additional training meshes
        if (m_meshTrainingSamples.size() > 0) {
            for (size_t i = 0; i < m_meshTrainingSamples.size(); ++i)
            {
                if (m_meshTrainingSamples[i].useForTraining)
                {
                    headVertexStates.insert(headVertexStates.end(), m_meshTrainingSamples[i].headVertexStates.begin(), m_meshTrainingSamples[i].headVertexStates.end());
                    headVertexStatesNeck.insert(headVertexStatesNeck.end(), m_meshTrainingSamples[i].headVertexStates.begin(), m_meshTrainingSamples[i].headVertexStates.end());
                }
            }

            // evaluate the meshes
            std::vector<rt::HeadVertexState<float>> headVertexStatesBS;

            for (size_t i = 0; i < headVertexStates.size(); ++i)
            {
                Eigen::VectorXf pcaCoeffs = Eigen::VectorXf::Zero(maxModes);
                Eigen::VectorXf pcaCoeffsNeck = Eigen::VectorXf::Zero(neckModes.cols());
                pcaCoeffs = m_pcaRig.Project(headVertexStates[i], pcaCoeffs);
                rt::HeadVertexState<float> pcaHeadVertexState = m_pcaRig.EvaluatePCARig(pcaCoeffs);

                pcaCoeffsNeck = m_pcaRig.ProjectNeck(headVertexStates[i], pcaHeadVertexState, pcaCoeffsNeck);
                Eigen::Matrix3Xf pcaNeckVertices = m_pcaRig.EvaluatePCARigNeck(pcaCoeffsNeck);

                rt::HeadVertexState<float> headVertexStateBS;
                headVertexStateBS = headVertexStates[i];

                const Eigen::Matrix<float, 3, -1> residual = headVertexStates[i].faceVertices - (pcaHeadVertexState.faceVertices + pcaNeckVertices);
                Eigen::Matrix<float, 3, -1> maskedResidual = residual;
                Eigen::VectorXf weights = m_rig->GetRigGeometry()->GetJointRig().GetSkinningWeights("head", m_rig->GetRigGeometry()->HeadMeshName(0), true);

                for (auto vId = 0; vId < maskedResidual.cols(); vId++)
                {
                    maskedResidual.col(vId) = (1.0 - weights[vId]) * residual.col(vId);
                }

                const Eigen::Matrix<float, 3, -1> faceVerticesBS = headVertexStates[i].faceVertices - (pcaNeckVertices + maskedResidual);
                headVertexStateBS.faceVertices = faceVerticesBS;
                headVertexStatesBS.emplace_back(headVertexStateBS);
            }

            Eigen::Matrix<float, -1, -1> dataMatrixBS(headVertexStatesBS.size(), 0);
            dataMatrixBS = GetFaceDataMatrix(headVertexStatesBS);
            LOG_INFO("time to create bootstrapped data matrix: {}", timer.Current());
            timer.Restart();

            std::pair<Eigen::VectorXf, Eigen::Matrix<float, -1, -1, Eigen::RowMajor>> pcaBS = CreatePCAFromDataMatrix(dataMatrixBS, maxModes);
            Eigen::VectorXf meanBS = pcaBS.first;
            Eigen::Matrix<float, -1, -1, Eigen::RowMajor> modesBS = pcaBS.second;
            LOG_INFO("time to create bootstrapped PCA: {}", timer.Current());
            timer.Restart();

            CreateFacePCARig(meanBS, modesBS, offsetsPerModel, defaultRootTransform);

            Eigen::Matrix<float, -1, -1> neckDataMatrixBS = GetNeckDataMatrix(headVertexStatesNeck, numFaceVertices, modesBS.cols());
            LOG_INFO("time to evalute neck bootstrapped expressions: {}", timer.Current());
            timer.Restart();

            std::pair<Eigen::VectorXf, Eigen::Matrix<float, -1, -1, Eigen::RowMajor>> pcaNeckBS = CreatePCAFromDataMatrix(neckDataMatrixBS, maxModes);
            Eigen::VectorXf neckMeanBS = pcaNeckBS.first;
            Eigen::Matrix<float, -1, -1, Eigen::RowMajor> neckModesBS = pcaNeckBS.second;

            const auto neckMeanShapeBS = Eigen::Map<const Eigen::Matrix<float, 3, -1>>(neckMeanBS.segment(3 * offsetsPerModel[0], 3 * numFaceVertices).data(), 3, numFaceVertices);
            m_pcaRig.neckPCA.Create(neckMeanShapeBS, neckModesBS.block(3 * offsetsPerModel[0], 0, 3 * numFaceVertices, neckModesBS.cols()));
            LOG_INFO("time to create neck PCA: {}", timer.Current());
            timer.Restart();
        }
    } else {
        if (m_meshTrainingSamples.size() > 0) {
            for (size_t i = 0; i < m_meshTrainingSamples.size(); ++i) {
                if (m_meshTrainingSamples[i].useForTraining) {
                    headVertexStates.insert(headVertexStates.end(), m_meshTrainingSamples[i].headVertexStates.begin(), m_meshTrainingSamples[i].headVertexStates.end());
                }
            }
            Eigen::Matrix<float, -1, -1> dataMatrixBS(headVertexStates.size(), 0);
            dataMatrixBS = GetFaceDataMatrix(headVertexStates);
            LOG_INFO("time to create bootstrappepd data matrix: {}", timer.Current());
            timer.Restart();
            // calculate face pca model from rig data
            std::pair<Eigen::VectorXf, Eigen::Matrix<float, -1, -1, Eigen::RowMajor>> pcaBS = CreatePCAFromDataMatrix(dataMatrixBS, maxModes);

            Eigen::VectorXf meanBS = pcaBS.first;
            Eigen::Matrix<float, -1, -1, Eigen::RowMajor> modesBS = pcaBS.second;

            LOG_INFO("time to create bootstrapped pca: {}", timer.Current());
            timer.Restart();

            // create face pca model from rig data
            CreateFacePCARig(meanBS, modesBS, offsetsPerModel, defaultRootTransform);
        }
    }
    return true;
}

int PCARigCreator::GuiControlIndex(const std::string& guiControlName) const
{
    for (size_t i = 0; i < m_rig->GetRigLogic()->GuiControlNames().size(); ++i)
    {
        if (m_rig->GetRigLogic()->GuiControlNames()[i] == guiControlName)
        {
            return int(i);
        }
    }
    return -1;
}

int PCARigCreator::RawControlIndex(const std::string& rawControlName) const
{
    for (size_t i = 0; i < m_rig->GetRigLogic()->RawControlNames().size(); ++i)
    {
        if (m_rig->GetRigLogic()->RawControlNames()[i] == rawControlName)
        {
            return int(i);
        }
    }
    return -1;
}

Eigen::VectorXf PCARigCreator::DefaultRawControls() const
{
    const int numRawControls = m_rig->GetRigLogic()->NumRawControls();

    Eigen::VectorXf defaultRawControls = Eigen::VectorXf::Zero(numRawControls);
    for (const auto& [defaultRawControlName, value] : m_defaultRawControls)
    {
        defaultRawControls[RawControlIndex(defaultRawControlName)] = value;
    }

    return defaultRawControls;
}

std::vector<Eigen::VectorXf> PCARigCreator::AllFaceExpressions() const
{
    std::vector<Eigen::VectorXf> allExpressions;
    allExpressions.reserve(1000);

    // default expression
    const Eigen::VectorXf defaultRawControls = DefaultRawControls();
    allExpressions.emplace_back(defaultRawControls);

    // evaluate corrective controls that are not based on fixed controls
    const SparseMatrix<float> psdToRawMap = m_rig->GetRigLogic()->PsdToRawMap();
    for (int k = 0; k < m_rig->GetRigLogic()->NumRawControls(); ++k)
    {

        Eigen::VectorXf rawControls = defaultRawControls;
        if (!(std::find(m_neckControls.begin(), m_neckControls.end(), m_rig->GetRigLogic()->RawControlNames()[k]) != m_neckControls.end())) {
            bool isFixed = (NumNonzerosForRow(psdToRawMap, k) == 0);
            for (typename SparseMatrix<float>::InnerIterator it(psdToRawMap, k); it; ++it)
            {
                if (it.col() < (int)rawControls.size())
                {
                    rawControls[it.col()] = float(1) / it.value();
                    isFixed |= (m_fixedControls.count(m_rig->GetRigLogic()->RawControlNames()[it.col()]) > 0);
                }
                else
                {
                    // only use expressions that are based on raw controls (no ml or rbf controls)
                    isFixed = true;
                }
            }
            if (isFixed) { continue; }
            allExpressions.emplace_back(rawControls);
        }
    }

    return allExpressions;
}

std::vector<Eigen::VectorXf> PCARigCreator::AllNeckExpressions() const
{
    std::vector<Eigen::VectorXf> neckExpressions;
    neckExpressions.reserve(1000);

    // default expression
    const Eigen::VectorXf defaultRawControls = DefaultRawControls();

    // evaluate neck controls
    for (const std::string& neckControlName : m_neckControls)
    {
        Eigen::VectorXf rawControls = defaultRawControls;
        rawControls[RawControlIndex(neckControlName)] = 1.0f;
        neckExpressions.emplace_back(rawControls);
    }

    return neckExpressions;
}

Eigen::Matrix<float, -1, -1> PCARigCreator::GetFaceDataMatrix(std::vector<rt::HeadVertexState<float>>& headVertexStates)
{
    Eigen::Matrix<float, -1, -1> dataMatrix(headVertexStates.size(), 0);
    for (size_t i = 0; i < headVertexStates.size(); ++i)
    {
        const rt::HeadState<float> headState = rt::HeadState<float>::FromHeadVertexState(headVertexStates[i], m_eyeLeftBody, m_eyeRightBody);
        if (i == 0)
        {
            dataMatrix.resize(headState.Flatten().size(), headVertexStates.size());
        }
        dataMatrix.col(i) = headState.Flatten();
    }
    return dataMatrix;
}

Eigen::Matrix<float, -1, -1> PCARigCreator::GetNeckDataMatrix(std::vector<rt::HeadVertexState<float>>& headVertexStates, const int numFaceVertices, Eigen::Index maxModes)
{
    Eigen::Matrix<float, -1, -1> neckDataMatrix(numFaceVertices * 3, headVertexStates.size());
    for (size_t i = 0; i < headVertexStates.size(); ++i)
    {
        Eigen::VectorXf pcaCoeffs = Eigen::VectorXf::Zero(maxModes);
        pcaCoeffs = m_pcaRig.Project(headVertexStates[i], pcaCoeffs);
        rt::HeadVertexState<float> pcaHeadVertexState = m_pcaRig.EvaluatePCARig(pcaCoeffs);
        const Eigen::Matrix<float, 3, -1> residual = headVertexStates[i].faceVertices - pcaHeadVertexState.faceVertices;
        Eigen::Matrix<float, 3, -1> maskedResidual = residual;
        Eigen::VectorXf weights = m_rig->GetRigGeometry()->GetJointRig().GetSkinningWeights("head", m_rig->GetRigGeometry()->HeadMeshName(0), true);
        for (auto vId = 0; vId < maskedResidual.cols(); vId++) {
            maskedResidual.col(vId) = weights[vId] * residual.col(vId);
        }
        const Eigen::Matrix<float, 3, -1> deltaVertices = headVertexStates[i].faceVertices - (pcaHeadVertexState.faceVertices + maskedResidual);
        neckDataMatrix.col(i) = Eigen::Map<const Eigen::VectorXf>(deltaVertices.data(), deltaVertices.size());
    }
    return neckDataMatrix;
}

std::pair<Eigen::VectorXf, Eigen::Matrix<float, -1, -1, Eigen::RowMajor>> PCARigCreator::CreatePCAFromDataMatrix(Eigen::Matrix<float, -1, -1>& dataMatrix, int maxModes)
{
    const float varianceToKeep = 0.99999f;
    EigPCA<float, Eigen::RowMajor> eigPCA;
    eigPCA.Create(dataMatrix, DataOrder::ColsAreExamples, varianceToKeep, maxModes);
    Eigen::VectorXf mean = eigPCA.mean;
    Eigen::Matrix<float, -1, -1, Eigen::RowMajor> modes = eigPCA.modes;
    return std::make_pair(mean, modes);
}

rt::HeadVertexState<float> PCARigCreator::EvaluateExpression(const Eigen::VectorXf& rawControls) const
{
    rt::HeadVertexState<float> headVertexState;
    const DiffData<float> psdValues = m_rig->GetRigLogic()->EvaluatePSD(DiffData<float>(rawControls));
    const int lod = 0;
    const DiffData<float> diffJoints = m_rig->GetRigLogic()->EvaluateJoints(psdValues, lod);
    RigGeometry<float>::State rigState;
    m_rig->GetRigGeometry()->EvaluateRigGeometry(DiffDataAffine<float, 3, 3>(),
                                                 diffJoints,
                                                 psdValues,
                                                 { m_headMeshIndex, m_teethMeshIndex, m_eyeLeftMeshIndex, m_eyeRightMeshIndex },
                                                 rigState);
    headVertexState.faceVertices = rigState.Vertices()[0].Matrix();
    headVertexState.teethVertices = rigState.Vertices()[1].Matrix();
    headVertexState.eyeLeftVertices = rigState.Vertices()[2].Matrix();
    headVertexState.eyeRightVertices = rigState.Vertices()[3].Matrix();

    // get rid of any neck motion that changes the facial root
    const int facialRootIndex = m_rig->GetRigGeometry()->GetJointRig().GetJointIndex(rt::FacialRootJointName());
    if (facialRootIndex < 0)
    {
        CARBON_CRITICAL("no facial root index");
    }
    const Eigen::Matrix<float, 4, 4> defaultRootTransform = m_rig->GetRigGeometry()->GetBindMatrix(facialRootIndex);
    const Eigen::Matrix<float, 4, 4> rootTransform = rigState.GetWorldMatrix(facialRootIndex);
    const Eigen::Matrix<float, 4, 4> toDefault = defaultRootTransform * rootTransform.inverse();
    const Eigen::Transform<float, 3, Eigen::Affine> rm(toDefault);

    headVertexState.faceVertices = rm * headVertexState.faceVertices;
    headVertexState.teethVertices = rm * headVertexState.teethVertices;
    headVertexState.eyeLeftVertices = rm * headVertexState.eyeLeftVertices;
    headVertexState.eyeRightVertices = rm * headVertexState.eyeRightVertices;

    return headVertexState;
}

bool PCARigCreator::LoadRigTrainingSamples(const std::string& qsaFilename)
{
    return LoadRigTrainingSamples(qsaFilename, false);
}

bool PCARigCreator::LoadRigNeckTrainingSamples(const std::string& qsaFilename)
{
    return LoadRigTrainingSamples(qsaFilename, true);
}

void PCARigCreator::CreateFacePCARig(Eigen::VectorXf& mean,
                                     Eigen::Matrix<float, -1, -1, Eigen::RowMajor>& modes,
                                     std::vector<int>& offsetsPerModel,
                                     const Eigen::Transform<float, 3, Eigen::Affine>& defaultRootTransform)
{
    const int numFaceVertices = (offsetsPerModel[1] - offsetsPerModel[0]);
    const int numTeethVertices = (offsetsPerModel[2] - offsetsPerModel[1]);

    m_pcaRig.rootBindPose = defaultRootTransform;
    const auto faceMean = Eigen::Map<const Eigen::Matrix<float, 3, -1>>(mean.segment(3 * offsetsPerModel[0], 3 * numFaceVertices).data(), 3, numFaceVertices);
    m_pcaRig.facePCA.Create(faceMean, modes.block(3 * offsetsPerModel[0], 0, 3 * numFaceVertices, modes.cols()));
    const auto teethMean = Eigen::Map<const Eigen::Matrix<float, 3, -1>>(mean.segment(3 * offsetsPerModel[1], 3 * numTeethVertices).data(), 3, numTeethVertices);
    m_pcaRig.teethPCA.Create(teethMean, modes.block(3 * offsetsPerModel[1], 0, 3 * numTeethVertices, modes.cols()));
    m_pcaRig.eyeLeftTransformPCA.eyeBody = m_eyeLeftBody;
    m_pcaRig.eyeLeftTransformPCA.linearTransformModel.mean = mean.segment(3 * offsetsPerModel[2], 6);
    m_pcaRig.eyeLeftTransformPCA.linearTransformModel.modes = modes.block(3 * offsetsPerModel[2], 0, 6, modes.cols());
    m_pcaRig.eyeRightTransformPCA.eyeBody = m_eyeRightBody;
    m_pcaRig.eyeRightTransformPCA.linearTransformModel.mean = mean.segment(3 * offsetsPerModel[2] + 6, 6);
    m_pcaRig.eyeRightTransformPCA.linearTransformModel.modes = modes.block(3 * offsetsPerModel[2] + 6, 0, 6, modes.cols());
    m_pcaRig.meshes[0] = m_rig->GetRigGeometry()->GetMesh(m_headMeshIndex);
    m_pcaRig.meshes[1] = m_rig->GetRigGeometry()->GetMesh(m_teethMeshIndex);
    m_pcaRig.meshes[2] = m_rig->GetRigGeometry()->GetMesh(m_eyeLeftMeshIndex);
    m_pcaRig.meshes[3] = m_rig->GetRigGeometry()->GetMesh(m_eyeRightMeshIndex);
}

bool PCARigCreator::LoadRigTrainingSamples(const std::string& qsaFilename, bool useForNeck)
{
    if (!qsaFilename.empty())
    {
        std::map<int, std::map<std::string, float>> valuesPerFrameAndControl;
        if (LoadQsa(qsaFilename, valuesPerFrameAndControl))
        {
            RigTrainingData rigControlData;
            rigControlData.name = std::filesystem::path(qsaFilename).stem().string();
            std::vector<int> inconsistentGuiControls;
            const Eigen::VectorXf defaultGuiControls = m_rig->GetRigLogic()->GuiControlsFromRawControls(DefaultRawControls(), inconsistentGuiControls);

            for (const auto& [frameNumber, valuesPerControl] : valuesPerFrameAndControl)
            {
                rigControlData.frameNumbers.push_back(frameNumber);
                Eigen::VectorXf guiControls = defaultGuiControls;
                for (const auto& [guiControl, value] : valuesPerControl)
                {
                    const int idx = GuiControlIndex(guiControl);
                    if (idx >= 0)
                    {
                        guiControls[idx] = value;
                    }
                    else
                    {
                        LOG_ERROR("gui control {} does not exist in rig", guiControl);
                        return false;
                    }
                }
                rigControlData.guiControls.push_back(guiControls);
                rigControlData.rawControls.push_back(m_rig->GetRigLogic()->EvaluateRawControls(DiffData<float>(guiControls)).Value());
                if (useForNeck) {
                    rigControlData.useForNeck = useForNeck;
                }
            }
            if (rigControlData.frameNumbers.size() > 0)
            {
                m_rigControlTrainingSamples.emplace_back(std::move(rigControlData));
                return true;
            }
        }
    }
    return false;
}

void PCARigCreator::SaveRigTrainingSamples(int idx, const std::string& qsaFilename) const
{
    if (!qsaFilename.empty() && (idx >= 0) && (idx < NumRigTrainingSampleSets()))
    {
        WriteQsa(qsaFilename,
                 m_rig->GetRigLogic()->GuiControlNames(),
                 m_rigControlTrainingSamples[idx].guiControls,
                 m_rigControlTrainingSamples[idx].frameNumbers);
    }
}

void PCARigCreator::CreateRigTrainingSamples(const std::string& name)
{
    RigTrainingData rigControlData;
    rigControlData.name = name;
    rigControlData.useForNeck = false;
    m_rigControlTrainingSamples.emplace_back(std::move(rigControlData));
}

void PCARigCreator::CreateRigNeckTrainingSamples(const std::string& name)
{
    RigTrainingData rigControlData;
    rigControlData.name = name;
    rigControlData.useForNeck = true;
    m_rigControlTrainingSamples.emplace_back(std::move(rigControlData));
}

void PCARigCreator::SetMeshTrainingSamples(const std::vector<std::vector<Eigen::Matrix<float, 3, -1>>>& trainingMeshes, float rotAngle)
{
    std::map<int, rt::HeadVertexState<float>> headVertexStates;
    const Eigen::Transform<float, 3, Eigen::Affine> rot(Eigen::AngleAxisf(rotAngle, Eigen::Vector3f::UnitX()));

    const int numFrames = (int)trainingMeshes.size();

    for (int frameNumber = 0; frameNumber < numFrames; ++frameNumber)
    {
        if ((trainingMeshes[frameNumber].size() < 1) || (trainingMeshes[frameNumber].size() > 4))
        {
            CARBON_CRITICAL("Invalid mesh training samples input.");
        }
        headVertexStates[frameNumber].faceVertices = rot * trainingMeshes[frameNumber][0];
        if (trainingMeshes[frameNumber].size() > 1)
        {
            headVertexStates[frameNumber].teethVertices = rot * trainingMeshes[frameNumber][1];
            headVertexStates[frameNumber].eyeLeftVertices = rot * trainingMeshes[frameNumber][2];
            headVertexStates[frameNumber].eyeRightVertices = rot * trainingMeshes[frameNumber][3];
        }
    }
    MeshTrainingData samples;
    samples.name = "appdata";
    for (auto&& [frame, headVertexState] : headVertexStates)
    {
        samples.frameNumbers.push_back(frame);
        samples.headVertexStates.push_back(headVertexState);
    }
    if (samples.frameNumbers.size() > 0)
    {
        m_meshTrainingSamples.emplace_back(std::move(samples));
    }
}

bool PCARigCreator::LoadMeshTrainingSamples(const std::string& dirname, bool recursive, bool isNumpy, float rotAngle)
{
    std::map<int, rt::HeadVertexState<float>> headVertexStates;

    const Eigen::Transform<float, 3, Eigen::Affine> rot(Eigen::AngleAxisf(rotAngle, Eigen::Vector3f::UnitX()));

    std::vector<std::string> subdirectories;

    std::filesystem::directory_iterator directoryIterator(dirname);
    for (const auto& entry : directoryIterator)
    {
        if (entry.is_directory())
        {
            subdirectories.push_back(entry.path().string());
            continue;
        }
        const auto& path = entry.path();
        const std::string& filename = path.string();
        const std::string extension = path.extension().string();
        const std::string stem = path.stem().string();
        const bool valid = extension == (isNumpy ? ".npy" : ".obj");
        if (valid)
        {
            const std::string frameNumberStr = stem.substr(stem.rfind('_') + 1);
            const int frameNumber = std::stoi(frameNumberStr);
            Eigen::Matrix<float, 3, -1> vertices;
            if (isNumpy)
            {
                TITAN_NAMESPACE::npy::LoadMatrixFromNpy(filename, vertices);
            }
            else
            {
                Mesh<float> mesh;
                ObjFileReader<float>().readObj(filename, mesh);
                vertices = mesh.Vertices();
            }
            if (stem.rfind("frame", 0) == 0)
            {
                headVertexStates[frameNumber].faceVertices = rot * vertices;
            }
            else if (stem.rfind("eyeLeft", 0) == 0)
            {
                headVertexStates[frameNumber].eyeLeftVertices = rot * vertices;
            }
            else if (stem.rfind("eyeRight", 0) == 0)
            {
                headVertexStates[frameNumber].eyeRightVertices = rot * vertices;
            }
            else if (stem.rfind("teeth", 0) == 0)
            {
                headVertexStates[frameNumber].teethVertices = rot * vertices;
            }
        }
    }

    MeshTrainingData samples;
    samples.name = dirname;

    for (auto&& [frame, headVertexState] : headVertexStates)
    {
        samples.frameNumbers.push_back(frame);
        samples.headVertexStates.push_back(headVertexState);
    }
    if (samples.frameNumbers.size() > 0)
    {
        m_meshTrainingSamples.emplace_back(std::move(samples));
    }

    if (recursive && (subdirectories.size() > 0))
    {
        for (const std::string& subdir : subdirectories)
        {
            LoadMeshTrainingSamples(subdir, recursive, isNumpy, rotAngle);
        }
    }

    return true;
}

void PCARigCreator::LoadFaceMask(const std::string& filename, const std::string& maskName)
{
    const TITAN_NAMESPACE::JsonElement maskJson = TITAN_NAMESPACE::ReadJson(TITAN_NAMESPACE::ReadFile(filename));

    const int headIndex = m_rig->GetRigGeometry()->HeadMeshIndex(/*lod=*/0);
    const std::map<std::string, VertexWeights<float>> masks = VertexWeights<float>::LoadAllVertexWeights(maskJson,
                                                                                                         m_rig->GetRigGeometry()->GetMesh(
                                                                                                             headIndex).NumVertices());
    if (!maskName.empty())
    {
        auto it = masks.find(maskName);
        if (it != masks.end())
        {
            m_faceMask = it->second;
        }
        else
        {
            CARBON_CRITICAL("no mask of name {} in {}", maskName, filename);
        }
    }
    else if (masks.size() == 1)
    {
        m_faceMask = masks.begin()->second;
    }
    else
    {
        CARBON_CRITICAL("if no mask name is specified, then the mask json file should contain only one mask");
    }
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rt)
