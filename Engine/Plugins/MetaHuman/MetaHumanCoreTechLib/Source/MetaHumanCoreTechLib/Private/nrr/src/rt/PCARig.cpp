// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/rt/PCARig.h>
#include <nls/geometry/EulerAngles.h>
#include <rig/Rig.h>
#include <rig/RigLogicDNAResource.h>
#include <carbon/io/NpyFileFormat.h>

#include <riglogic/RigLogic.h>

#include <array>
#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rt)

const char* EyeLeftJointName() { return "FACIAL_L_Eye"; }
const char* EyeRightJointName() { return "FACIAL_R_Eye"; }
const char* FacialRootJointName() { return "FACIAL_C_FacialRoot"; }

const char* HeadMeshName() { return "head_lod0_mesh"; }
const char* TeethMeshName() { return "teeth_lod0_mesh"; }
const char* EyeLeftMeshName() { return "eyeLeft_lod0_mesh"; }
const char* EyeRightMeshName() { return "eyeRight_lod0_mesh"; }

HeadVertexState<float> PCARig::EvaluatePCARig(const Eigen::VectorX<float>& pcaCoeffs) const
{
    HeadVertexState<float> headVertexState;
    headVertexState.faceVertices = facePCA.EvaluateLinearized(pcaCoeffs, LinearVertexModel<float>::EvaluationMode::STATIC);
    headVertexState.teethVertices = teethPCA.EvaluateLinearized(pcaCoeffs, LinearVertexModel<float>::EvaluationMode::STATIC);
    headVertexState.eyeLeftVertices = eyeLeftTransformPCA.EvaluateVertices(pcaCoeffs, LinearVertexModel<float>::EvaluationMode::STATIC);
    headVertexState.eyeRightVertices = eyeRightTransformPCA.EvaluateVertices(pcaCoeffs, LinearVertexModel<float>::EvaluationMode::STATIC);
    return headVertexState;
}

Eigen::Matrix3Xf PCARig::EvaluatePCARigNeck(const Eigen::VectorX<float>& pcaCoeffs) const
{
    Eigen::Matrix3Xf neckVertices = neckPCA.EvaluateLinearized(pcaCoeffs, LinearVertexModel<float>::EvaluationMode::STATIC);
    return neckVertices;
}

Eigen::VectorX<float> PCARig::Project(const HeadVertexState<float>& headVertexState, const Eigen::VectorX<float>& coeffs) const
{
    Eigen::VectorX<float> currCoeffs = coeffs;
    if (coeffs.size() == 0)
    {
        currCoeffs = Eigen::VectorX<float>::Zero(facePCA.NumPCAModes());
    }
    const int numCoeffs = int(currCoeffs.size());
    Eigen::Matrix<float, -1, -1> AtA = Eigen::Matrix<float, -1, -1>::Zero(numCoeffs, numCoeffs);
    Eigen::VectorX<float> Atb = Eigen::VectorX<float>::Zero(numCoeffs);

    const auto evaluationMode = LinearVertexModel<float>::EvaluationMode::STATIC;

    if (headVertexState.IsValidFaceData())
    {
        AtA.noalias() += facePCA.Modes(evaluationMode).transpose() * facePCA.Modes(evaluationMode);
        const auto verticesVec = Eigen::Map<const Eigen::VectorXf>(headVertexState.faceVertices.data(), headVertexState.faceVertices.size());
        Atb.noalias() += -facePCA.Modes(evaluationMode).transpose() * (facePCA.BaseAsVector() + facePCA.Modes(evaluationMode) * currCoeffs - verticesVec);
    }
    if (headVertexState.IsValidTeethData())
    {
        AtA.noalias() += teethPCA.Modes(evaluationMode).transpose() * teethPCA.Modes(evaluationMode);
        const auto verticesVec = Eigen::Map<const Eigen::VectorXf>(headVertexState.teethVertices.data(), headVertexState.teethVertices.size());
        Atb.noalias() += -teethPCA.Modes(evaluationMode).transpose() * (teethPCA.BaseAsVector() + teethPCA.Modes(evaluationMode) * currCoeffs - verticesVec);
    }
    LinearVertexModel<float> eyeVertexModel;
    if (headVertexState.IsValidLeftEyeData())
    {
        eyeLeftTransformPCA.EvaluateVerticesAndJacobian(currCoeffs, evaluationMode, eyeVertexModel);
        const auto& eyeLeftVerticesVec = eyeVertexModel.BaseAsVector();
        const auto targetVerticesVec = Eigen::Map<const Eigen::VectorXf>(headVertexState.eyeLeftVertices.data(), headVertexState.eyeLeftVertices.size());
        AtA.noalias() += eyeVertexModel.Modes(evaluationMode).transpose() * eyeVertexModel.Modes(evaluationMode);
        Atb.noalias() += -eyeVertexModel.Modes(evaluationMode).transpose() * (eyeLeftVerticesVec - targetVerticesVec);
    }
    if (headVertexState.IsValidRightEyeData())
    {
        eyeRightTransformPCA.EvaluateVerticesAndJacobian(currCoeffs, evaluationMode, eyeVertexModel);
        const auto& eyeRightVerticesVec = eyeVertexModel.BaseAsVector();
        const auto targetVerticesVec = Eigen::Map<const Eigen::VectorXf>(headVertexState.eyeRightVertices.data(), headVertexState.eyeRightVertices.size());
        AtA.noalias() += eyeVertexModel.Modes(evaluationMode).transpose() * eyeVertexModel.Modes(evaluationMode);
        Atb.noalias() += -eyeVertexModel.Modes(evaluationMode).transpose() * (eyeRightVerticesVec - targetVerticesVec);
    }

    return currCoeffs + AtA.ldlt().solve(Atb);
}

Eigen::VectorX<float> PCARig::ProjectNeck(const HeadVertexState<float>& headVertexState, const HeadVertexState<float>& neutralHeadVertexState, const Eigen::VectorX<float>& coeffs) const
{
    Eigen::VectorX<float> currCoeffs = coeffs;
    if (coeffs.size() == 0)
    {
        currCoeffs = Eigen::VectorX<float>::Zero(neckPCA.NumPCAModes());
    }
    const int numCoeffs = int(currCoeffs.size());
    Eigen::Matrix<float, -1, -1> AtA = Eigen::Matrix<float, -1, -1>::Zero(numCoeffs, numCoeffs);
    Eigen::VectorX<float> Atb = Eigen::VectorX<float>::Zero(numCoeffs);

    const auto evaluationMode = LinearVertexModel<float>::EvaluationMode::STATIC;

    {
        AtA.noalias() += neckPCA.Modes(evaluationMode).transpose() * neckPCA.Modes(evaluationMode);
        const auto verticesVec = Eigen::Map<const Eigen::VectorXf>(headVertexState.faceVertices.data(), headVertexState.faceVertices.size());
        const auto dVerticesVec = Eigen::Map<const Eigen::VectorXf>(neutralHeadVertexState.faceVertices.data(), neutralHeadVertexState.faceVertices.size());
        Atb.noalias() += -neckPCA.Modes(evaluationMode).transpose() * (dVerticesVec + neckPCA.BaseAsVector() + neckPCA.Modes(evaluationMode) * currCoeffs - verticesVec);
    }

    return currCoeffs + AtA.ldlt().solve(Atb);
}

void PCARig::Translate(const Eigen::Vector3f& translation)
{
    facePCA.Translate(translation);
    teethPCA.Translate(translation);
    eyeLeftTransformPCA.Translate(translation);
    eyeRightTransformPCA.Translate(translation);
    rootBindPose = Eigen::Translation3f(translation) * rootBindPose;
    offset += translation;
}

Eigen::Vector3f PCARig::EyesMidpoint() const
{
    const auto evaluationMode = LinearVertexModel<float>::EvaluationMode::STATIC;

    const Eigen::Vector3f eyeLeftCenter = eyeLeftTransformPCA.EvaluateVertices(Eigen::VectorXf::Zero(facePCA.NumPCAModes()), evaluationMode).rowwise().mean();
    const Eigen::Vector3f eyeRightCenter = eyeRightTransformPCA.EvaluateVertices(Eigen::VectorXf::Zero(facePCA.NumPCAModes()), evaluationMode).rowwise().mean();
    return 0.5f * (eyeLeftCenter + eyeRightCenter);
}

void PCARig::SaveAsDNA(dna::Writer* writer) const
{
    const std::vector<std::string> allMeshNames = { HeadMeshName(), TeethMeshName(), EyeLeftMeshName(), EyeRightMeshName() };
    const std::vector<std::string> bsMeshNames = { HeadMeshName(), TeethMeshName() };
    const uint16_t numPCAcoeffs = uint16_t(NumCoeffs());
    const uint16_t numNeckPCAcoeffs = neckPCA.NumTotalModes() > 0 ? uint16_t(NumCoeffsNeck()) + 1 : 0;

    constexpr float rad2deg = float(180.0 / CARBON_PI);

    // we save the meshes with the mean
    HeadVertexState<float> meanHeadVertexState = EvaluatePCARig(Eigen::VectorXf::Zero(NumCoeffs()));
    std::vector<Eigen::Matrix<float, 3, -1>> allMeanVertices;
    allMeanVertices.push_back(meanHeadVertexState.faceVertices);
    allMeanVertices.push_back(meanHeadVertexState.teethVertices);
    allMeanVertices.push_back(meanHeadVertexState.eyeLeftVertices);
    allMeanVertices.push_back(meanHeadVertexState.eyeRightVertices);

    // DescriptorWriter methods
    writer->setName("pca_model");
    writer->setLODCount(1);

    // definition
    writer->clearGUIControlNames();
    writer->clearRawControlNames();

    for (uint16_t bsIndex = 0; bsIndex < numPCAcoeffs; ++bsIndex)
    {
        writer->setGUIControlName(bsIndex, (std::string("gui_pca_") + std::to_string(bsIndex)).c_str());
        writer->setRawControlName(bsIndex, (std::string("raw_pca_") + std::to_string(bsIndex)).c_str());
    }

    if (numNeckPCAcoeffs > 0)
    {
        for (uint16_t bsIndex = 0; bsIndex < numNeckPCAcoeffs; ++bsIndex)
        {
            writer->setGUIControlName(numPCAcoeffs + bsIndex, (std::string("gui_pca_neck_") + std::to_string(bsIndex)).c_str());
            writer->setRawControlName(numPCAcoeffs + bsIndex, (std::string("raw_pca_neck_") + std::to_string(bsIndex)).c_str());
        }
    }

    writer->clearMeshNames();
    std::vector<std::uint16_t> meshIndices;
    for (size_t i = 0; i < allMeshNames.size(); ++i)
    {
        const uint16_t meshIndex = static_cast<uint16_t>(i);
        writer->setMeshName(meshIndex, allMeshNames[i].c_str());
        meshIndices.push_back(meshIndex);
    }
    writer->setMeshIndices(0, meshIndices.data(), std::uint16_t(meshIndices.size()));
    writer->setLODMeshMapping(0, 0);

    writer->clearBlendShapeChannelNames();
    writer->clearMeshBlendShapeChannelMappings();
    std::vector<uint16_t> controlInputIndices;
    std::vector<uint16_t> blendshapeOutputIndices;
    for (size_t meshIndex = 0; meshIndex < bsMeshNames.size(); ++meshIndex)
    {
        for (uint16_t bsIndex = 0; bsIndex < numPCAcoeffs; ++bsIndex)
        {
            const uint16_t meshBsIndex = uint16_t(numPCAcoeffs * meshIndex + bsIndex);
            writer->setBlendShapeChannelName(meshBsIndex, (bsMeshNames[meshIndex] + std::string("_bs_pca_") + std::to_string(bsIndex)).c_str());
            writer->setMeshBlendShapeChannelMapping(0, uint16_t(meshIndex), meshBsIndex);
            controlInputIndices.push_back(bsIndex);
            blendshapeOutputIndices.push_back(meshBsIndex);
        }
    }

    if (numNeckPCAcoeffs > 0) {
        for (uint16_t bsIndex = 0; bsIndex < numNeckPCAcoeffs; ++bsIndex)
        {
            const uint16_t meshBsIndex = uint16_t(numPCAcoeffs * bsMeshNames.size() + bsIndex);
            writer->setBlendShapeChannelName(meshBsIndex, (bsMeshNames[0] + std::string("_bs_pca_neck_") + std::to_string(bsIndex)).c_str());
            writer->setMeshBlendShapeChannelMapping(0, 0, meshBsIndex);
            const uint16_t neckCtrlIndex = numPCAcoeffs + bsIndex;
            controlInputIndices.push_back(neckCtrlIndex);
            blendshapeOutputIndices.push_back(meshBsIndex);
        }
    }

    writer->setBlendShapeChannelIndices(0, blendshapeOutputIndices.data(), uint16_t(blendshapeOutputIndices.size()));
    writer->clearLODBlendShapeChannelMappings();
    writer->setLODBlendShapeChannelMapping(0, 0);

    writer->clearJointNames();
    writer->setJointName(0, FacialRootJointName());
    writer->setJointName(1, EyeLeftJointName());
    writer->setJointName(2, EyeRightJointName());
    std::array<std::uint16_t, 3> jointIndices{ 0, 1, 2 };
    writer->clearJointIndices();
    writer->setJointIndices(0, jointIndices.data(), uint16_t(jointIndices.size()));
    writer->clearLODJointMappings();
    writer->setLODJointMapping(0, 0);
    std::array<std::uint16_t, 3> jointHierarchy{ 0, 0, 0 };
    writer->setJointHierarchy(jointHierarchy.data(), uint16_t(jointHierarchy.size()));

    Eigen::Matrix<float, 3, -1> jointTranslations(3, 3);
    Eigen::Matrix<float, 3, -1> jointRotations(3, 3);
    {
        const Eigen::Transform<float, 3, Eigen::Affine> eyeLeftTransform = rootBindPose.inverse() * eyeLeftTransformPCA.eyeBody.baseTransform;
        const Eigen::Transform<float, 3, Eigen::Affine> eyeRightTransform = rootBindPose.inverse() * eyeRightTransformPCA.eyeBody.baseTransform;
        jointRotations.col(0) = (RotationMatrixToEulerXYZ<float>(rootBindPose.linear()) * rad2deg).template cast<float>();
        jointTranslations.col(0) = rootBindPose.translation();
        jointRotations.col(1) = (RotationMatrixToEulerXYZ<float>(eyeLeftTransform.linear()) * rad2deg).template cast<float>();
        jointTranslations.col(1) = eyeLeftTransform.translation();
        jointRotations.col(2) = (RotationMatrixToEulerXYZ<float>(eyeRightTransform.linear()) * rad2deg).template cast<float>();
        jointTranslations.col(2) = eyeRightTransform.translation();
    }
    writer->setNeutralJointTranslations((const dna::Vector3*)jointTranslations.data(), 3);
    writer->setNeutralJointRotations((const dna::Vector3*)jointRotations.data(), 3);

    // BehaviorWriter methods
    const uint16_t totalCoeffs = numPCAcoeffs + numNeckPCAcoeffs;
    {
        const Eigen::VectorX<uint16_t> inputIndices = Eigen::VectorX<uint16_t>::LinSpaced(totalCoeffs, 0, totalCoeffs - 1);
        const Eigen::VectorX<uint16_t> outputIndices = Eigen::VectorX<uint16_t>::LinSpaced(totalCoeffs, 0, totalCoeffs - 1);
        const Eigen::VectorX<float> fromValues = Eigen::VectorX<float>::Constant(totalCoeffs, -3000);
        const Eigen::VectorX<float> toValues = Eigen::VectorX<float>::Constant(totalCoeffs, 3000);
        const Eigen::VectorX<float> slopeValues = Eigen::VectorX<float>::Constant(totalCoeffs, 1);
        const Eigen::VectorX<float> cutValues = Eigen::VectorX<float>::Zero(totalCoeffs);
        writer->setGUIToRawInputIndices(inputIndices.data(), totalCoeffs);
        writer->setGUIToRawOutputIndices(outputIndices.data(), totalCoeffs);
        writer->setGUIToRawFromValues(fromValues.data(), totalCoeffs);
        writer->setGUIToRawToValues(toValues.data(), totalCoeffs);
        writer->setGUIToRawSlopeValues(slopeValues.data(), totalCoeffs);
        writer->setGUIToRawCutValues(cutValues.data(), totalCoeffs);
    }

    {
        const Eigen::VectorX<uint16_t> inputIndices = Eigen::VectorX<uint16_t>::LinSpaced(numPCAcoeffs, 0, numPCAcoeffs - 1);
        std::uint16_t rows = 12;
        Eigen::VectorX<uint16_t> outputIndices(12);
        for (int i = 0; i < 2; ++i)
        {
            for (int j = 0; j < 6; ++j)
            {
                outputIndices[6 * i + j] = uint16_t(9 + 9 * i + j);
            }
        }
        writer->setJointRowCount(3 * 9);
        writer->setJointColumnCount(totalCoeffs);
        writer->clearJointGroups();
        writer->setJointGroupLODs(0, &rows, 1);
        writer->setJointGroupInputIndices(0, inputIndices.data(), uint16_t(inputIndices.size()));
        writer->setJointGroupOutputIndices(0, outputIndices.data(), uint16_t(outputIndices.size()));
        Eigen::Matrix<float, -1, -1, Eigen::RowMajor> rotationsAndTranslations(rows, numPCAcoeffs);
        rotationsAndTranslations.block(0, 0, 3, numPCAcoeffs) = eyeLeftTransformPCA.linearTransformModel.modes.block(3, 0, 3, numPCAcoeffs);
        rotationsAndTranslations.block(3, 0, 3, numPCAcoeffs) = eyeLeftTransformPCA.linearTransformModel.modes.block(0, 0, 3, numPCAcoeffs) * rad2deg;
        rotationsAndTranslations.block(6, 0, 3, numPCAcoeffs) = eyeRightTransformPCA.linearTransformModel.modes.block(3, 0, 3, numPCAcoeffs);
        rotationsAndTranslations.block(9, 0, 3, numPCAcoeffs) = eyeRightTransformPCA.linearTransformModel.modes.block(0, 0, 3, numPCAcoeffs) * rad2deg;
        writer->setJointGroupValues(0, rotationsAndTranslations.data(), uint16_t(rotationsAndTranslations.size()));
    }

    {
        const uint16_t blendshapeChannelLODs = uint16_t(bsMeshNames.size() * numPCAcoeffs + numNeckPCAcoeffs);
        writer->setBlendShapeChannelLODs(&blendshapeChannelLODs, 1);
        writer->setBlendShapeChannelInputIndices(controlInputIndices.data(), uint16_t(controlInputIndices.size()));
        writer->setBlendShapeChannelOutputIndices(blendshapeOutputIndices.data(), uint16_t(blendshapeOutputIndices.size()));
    }

    // GeometryWriter methods
    writer->clearMeshes();
    for (uint16_t meshIndex = 0; meshIndex < uint16_t(allMeshNames.size()); ++meshIndex)
    {
        writer->setVertexPositions(meshIndex, (const dna::Position*)allMeanVertices[meshIndex].data(), uint32_t(allMeanVertices[meshIndex].cols()));
        const Mesh<float>& quadMesh = meshes[meshIndex];
        Mesh<float> triMesh = quadMesh;
        triMesh.Triangulate();
        triMesh.SetVertices(allMeanVertices[meshIndex]);
        triMesh.CalculateVertexNormals();
        writer->setVertexNormals(meshIndex, (const dna::Normal*)triMesh.VertexNormals().data(), uint32_t(triMesh.NumVertices()));
        Eigen::Matrix<float, 2, -1> texcoords = quadMesh.Texcoords();
        for (int k = 0; k < texcoords.cols(); ++k)
        {
            texcoords(1, k) = 1.0f - texcoords(1, k);
        }
        writer->setVertexTextureCoordinates(meshIndex, (const dna::TextureCoordinate*)texcoords.data(), uint32_t(texcoords.cols()));
        {
            int totalFaces = quadMesh.NumQuads() + quadMesh.NumTriangles();
            writer->setFaceVertexLayoutIndices(uint16_t(meshIndex), totalFaces - 1, nullptr, 0);

            int faceCount = 0;
            std::vector<dna::VertexLayout> vertexLayouts;
            for (int quadIndex = 0; quadIndex < quadMesh.NumQuads(); ++quadIndex)
            {
                std::vector<uint32_t> layoutIndices;
                for (int k = 0; k < 4; ++k)
                {
                    dna::VertexLayout vertexLayout;
                    vertexLayout.position = quadMesh.Quads()(k, quadIndex);
                    vertexLayout.normal = quadMesh.Quads()(k, quadIndex);
                    vertexLayout.textureCoordinate = quadMesh.TexQuads()(k, quadIndex);
                    layoutIndices.push_back(uint32_t(vertexLayouts.size()));
                    vertexLayouts.push_back(vertexLayout);
                }
                writer->setFaceVertexLayoutIndices(meshIndex, faceCount, layoutIndices.data(), uint32_t(layoutIndices.size()));
                faceCount++;
            }
            for (int triIndex = 0; triIndex < quadMesh.NumTriangles(); ++triIndex)
            {
                std::vector<uint32_t> layoutIndices;
                for (int k = 0; k < 3; ++k)
                {
                    dna::VertexLayout vertexLayout;
                    vertexLayout.position = quadMesh.Triangles()(k, triIndex);
                    vertexLayout.normal = quadMesh.Triangles()(k, triIndex);
                    vertexLayout.textureCoordinate = quadMesh.TexTriangles()(k, triIndex);
                    layoutIndices.push_back(uint32_t(vertexLayouts.size()));
                    vertexLayouts.push_back(vertexLayout);
                }
                writer->setFaceVertexLayoutIndices(meshIndex, faceCount, layoutIndices.data(), uint32_t(layoutIndices.size()));
                faceCount++;
            }
            writer->setVertexLayouts(meshIndex, vertexLayouts.data(), uint32_t(vertexLayouts.size()));
        }
    }

    {
        for (uint16_t meshIndex = 0; meshIndex < 4; ++meshIndex)
        {
            writer->setMaximumInfluencePerVertex(meshIndex, 1);
            writer->clearSkinWeights(meshIndex);
            const int numVertices = meshes[meshIndex].NumVertices();
            const float weight = 1.0f;
            const std::uint16_t jointIndex = std::uint16_t(std::max(0, (meshIndex - 1)));
            for (int vID = numVertices - 1; vID >= 0; --vID)
            {
                writer->setSkinWeightsValues(meshIndex, vID, &weight, 1);
                writer->setSkinWeightsJointIndices(meshIndex, vID, &jointIndex, 1);
            }
        }
    }

    for (uint16_t meshIndex = 0; meshIndex < uint16_t(bsMeshNames.size()); ++meshIndex)
    {
        writer->clearBlendShapeTargets(meshIndex);
        const LinearVertexModel<float>& linearModel = (meshIndex == 0) ? facePCA : teethPCA;
        const int numVertices = linearModel.NumVertices();
        Eigen::VectorX<std::uint32_t> vertexIndices = Eigen::VectorX<std::uint32_t>::LinSpaced(numVertices, 0, numVertices - 1);
        for (uint16_t bsIndex = 0; bsIndex < numPCAcoeffs; ++bsIndex)
        {
            const uint16_t meshBsIndex = numPCAcoeffs * meshIndex + bsIndex;
            writer->setBlendShapeChannelIndex(meshIndex, bsIndex, meshBsIndex);
            const Eigen::VectorXf mode = linearModel.Modes(LinearVertexModel<float>::EvaluationMode::STATIC).col(bsIndex);
            writer->setBlendShapeTargetDeltas(meshIndex, bsIndex, (const dna::Delta*)mode.data(), numVertices);
            writer->setBlendShapeTargetVertexIndices(meshIndex, bsIndex, vertexIndices.data(), numVertices);
        }
    }

    if (numNeckPCAcoeffs > 0) {
        const int numVertices = neckPCA.NumVertices();
        Eigen::VectorX<std::uint32_t> vertexIndices = Eigen::VectorX<std::uint32_t>::LinSpaced(numVertices, 0, numVertices - 1);
        for (uint16_t bsIndex = 0; bsIndex < numNeckPCAcoeffs; ++bsIndex)
        {
            const uint16_t neckCtrlIndex = numPCAcoeffs + bsIndex;
            const uint16_t meshBsIndex = uint16_t(numPCAcoeffs * bsMeshNames.size() + bsIndex);
            writer->setBlendShapeChannelIndex(0, neckCtrlIndex, meshBsIndex);
            Eigen::VectorXf mode;
            if (bsIndex == (numNeckPCAcoeffs - 1)) {
                mode = neckPCA.BaseAsVector();
            } else {
                mode = neckPCA.Modes(LinearVertexModel<float>::EvaluationMode::STATIC).col(bsIndex);
            }
            writer->setBlendShapeTargetDeltas(0, neckCtrlIndex, (const dna::Delta*)mode.data(), numVertices);
            writer->setBlendShapeTargetVertexIndices(0, neckCtrlIndex, vertexIndices.data(), numVertices);
        }
    }
}

void PCARig::SaveAsDNA(const std::string& filename) const
{
    pma::ScopedPtr<dna::FileStream> stream = pma::makeScoped<dna::FileStream>(filename.c_str(),
                                                                              dna::FileStream::AccessMode::Write,
                                                                              dna::FileStream::OpenMode::Binary);
    pma::ScopedPtr<dna::BinaryStreamWriter> writer = pma::makeScoped<dna::BinaryStreamWriter>(stream.get());

    // UNTILL LATEST DB VERSION CAME THROUGH WE SAVE IT AS PREVIOUS VERSION
    writer->setFileFormatVersion(2);
    writer->setFileFormatGeneration(2);
    SaveAsDNA(writer.get());
    writer->write();

    // verify that we get the same reconstruction using the rig
    Rig<float> testDnaRig;
    if (!testDnaRig.LoadRig(filename))
    {
        CARBON_CRITICAL("failed to read dna rig");
    }
    PCARig testPcaRig;
    if (!testPcaRig.LoadFromDNA(filename))
    {
        CARBON_CRITICAL("failed to read pca rig from dna");
    }

    const uint16_t numPCAcoeffs = uint16_t(NumCoeffs());
    const uint16_t numNeckPCAcoeffs = uint16_t(NumCoeffsNeck());
    uint16_t totalPCACoeffs = numPCAcoeffs;
    if (numNeckPCAcoeffs > 0) {
        totalPCACoeffs += numNeckPCAcoeffs + 1;
    }

    for (int k = 0; k < totalPCACoeffs; ++k)
    {
        Eigen::VectorXf pcaCoeffs = Eigen::VectorXf::Zero(numPCAcoeffs);
        Eigen::VectorXf pcaCoeffsWNeck = Eigen::VectorXf::Zero(totalPCACoeffs);

        pcaCoeffsWNeck[k] = 1;

        if (k < numPCAcoeffs) {
            pcaCoeffs[k] = 1;
        }

        HeadVertexState<float> headVertexState = EvaluatePCARig(pcaCoeffs);
        HeadVertexState<float> headVertexStatePCA = testPcaRig.EvaluatePCARig(pcaCoeffs);

        if (k >= numPCAcoeffs) {
            if (k < (totalPCACoeffs - 1)) {
                pcaCoeffsWNeck[totalPCACoeffs - 1] = 1;
                Eigen::VectorXf pcaCoeffsNeck = Eigen::VectorXf::Zero(numNeckPCAcoeffs);
                pcaCoeffsNeck[k - numPCAcoeffs] = 1;
                headVertexState.faceVertices += EvaluatePCARigNeck(pcaCoeffsNeck);
                headVertexStatePCA.faceVertices += testPcaRig.EvaluatePCARigNeck(pcaCoeffsNeck);
            }
            else {
                Eigen::VectorXf pcaCoeffsNeck = Eigen::VectorXf::Zero(numNeckPCAcoeffs);
                headVertexState.faceVertices += EvaluatePCARigNeck(pcaCoeffsNeck);
                headVertexStatePCA.faceVertices += testPcaRig.EvaluatePCARigNeck(pcaCoeffsNeck);
            }
        }

        {
            const std::vector<Eigen::Matrix<float, 3, -1>> rigVertices = testDnaRig.EvaluateVertices(pcaCoeffsWNeck, 0, { 0, 1, 2, 3 });
            const float diffHead = (rigVertices[0] - headVertexState.faceVertices).cwiseAbs().maxCoeff();
            const float diffTeeth = (rigVertices[1] - headVertexState.teethVertices).cwiseAbs().maxCoeff();
            const float diffEyeLeft = (rigVertices[2] - headVertexState.eyeLeftVertices).cwiseAbs().maxCoeff();
            const float diffEyeRight = (rigVertices[3] - headVertexState.eyeRightVertices).cwiseAbs().maxCoeff();
            if (diffHead > 1e-4)
            {
                LOG_WARNING("{}: rig face error: {}", k, diffHead);
            }
            if (diffTeeth > 1e-4)
            {
                LOG_WARNING("{}: rig teeth error: {}", k, diffTeeth);
            }
            if (diffEyeLeft > 1e-4)
            {
                LOG_WARNING("{}: rig eye left error: {}", k, diffEyeLeft);
            }
            if (diffEyeRight > 1e-4)
            {
                LOG_WARNING("{}: rig eye right error: {}", k, diffEyeRight);
            }
        }
        {
            const float diffHead = (headVertexStatePCA.faceVertices - headVertexState.faceVertices).cwiseAbs().maxCoeff();
            const float diffTeeth = (headVertexStatePCA.teethVertices - headVertexState.teethVertices).cwiseAbs().maxCoeff();
            const float diffEyeLeft = (headVertexStatePCA.eyeLeftVertices - headVertexState.eyeLeftVertices).cwiseAbs().maxCoeff();
            const float diffEyeRight = (headVertexStatePCA.eyeRightVertices - headVertexState.eyeRightVertices).cwiseAbs().maxCoeff();
            if (diffHead > 1e-4)
            {
                LOG_WARNING("{}: pca rig face error: {}", k, diffHead);
            }
            if (diffTeeth > 1e-4)
            {
                LOG_WARNING("{}: pca rig teeth error: {}", k, diffTeeth);
            }
            if (diffEyeLeft > 1e-4)
            {
                LOG_WARNING("{}: pca rig eye left error: {}", k, diffEyeLeft);
            }
            if (diffEyeRight > 1e-4)
            {
                LOG_WARNING("{}: pca rig eye right error: {}", k, diffEyeRight);
            }
        }
    }
}

bool PCARig::LoadFromDNA(const std::string& filename)
{
    std::shared_ptr<const RigLogicDNAResource> dnaResource = RigLogicDNAResource::LoadDNA(filename, /*retain=*/false);
    if (!dnaResource)
    {
        LOG_ERROR("failed to open dnafile {}", filename);
        return false;
    }

    return LoadFromDNA(dnaResource->Stream());
}

bool PCARig::LoadFromDNA(dna::Reader* reader)
{
    if (reader->getLODCount() != 1)
    {
        LOG_ERROR("dna does not seem to be a PCA rig");
        return false;
    }

    std::uint16_t totalPCACoeffs = reader->getGUIControlCount();
    std::uint16_t numNeckPCACoeffs = 0;

    for (std::uint16_t ctrlIndex = 0; ctrlIndex < totalPCACoeffs; ++ctrlIndex) {
        std::string ctrlName = reader->getGUIControlName(ctrlIndex).c_str();
        if (ctrlName.rfind("gui_pca_neck_", 0) == 0) {
            numNeckPCACoeffs += 1;
        }
    }

    std::uint16_t numPCACoeffs = totalPCACoeffs - numNeckPCACoeffs;

    if ((reader->getRawControlCount() != totalPCACoeffs) || (reader->getPSDCount() != 0))
    {
        LOG_ERROR("dna does not seem to be a PCA rig");
        return false;
    }

    if ((reader->getBlendShapeChannelLODs()[0] != (totalPCACoeffs + numPCACoeffs))
        || (reader->getMeshCount() != 4)
        || (reader->getBlendShapeTargetCount(0) != totalPCACoeffs)
        || (reader->getBlendShapeTargetCount(1) != numPCACoeffs))
    {
        LOG_ERROR("number of PCA blendshape channels incorrect");
        return false;
    }

    if ((reader->getJointCount() != 3)
        || (reader->getJointCount() * 9 != reader->getJointRowCount())
        || (reader->getJointColumnCount() != totalPCACoeffs)
        || (reader->getJointGroupCount() != 1)
        || (int(reader->getJointGroupInputIndices(0).size()) != numPCACoeffs)
        || (reader->getJointGroupOutputIndices(0).size() != 12))
    {
        LOG_ERROR("unexpected number of joints in pca rig");
        return false;
    }

    // load meshes
    meshes[0] = RigGeometry<float>::ReadMesh(reader, 0); // face
    meshes[1] = RigGeometry<float>::ReadMesh(reader, 1); // teeth
    meshes[2] = RigGeometry<float>::ReadMesh(reader, 2); // left eye
    meshes[3] = RigGeometry<float>::ReadMesh(reader, 3); // right eye

    std::vector<Eigen::Matrix<float, -1, -1, Eigen::RowMajor>> modes(2);
    modes[0].resize(meshes[0].NumVertices() * 3, numPCACoeffs);
    modes[1].resize(meshes[1].NumVertices() * 3, numPCACoeffs);

    // read blendshape data
    for (std::uint16_t meshIndex = 0; meshIndex < 2; ++meshIndex)
    {
        // auto& modes = (meshIndex == 0) ? facePCA.Modes(/*withRigid*/false) : teethPCA.Modes(/*withRigid*/false);
        const int numVertices = meshes[meshIndex].NumVertices();
        for (std::uint16_t blendShapeTargetIndex = 0; blendShapeTargetIndex < numPCACoeffs; blendShapeTargetIndex++)
        {
            // const std::uint16_t channelIndex = reader->getBlendShapeChannelIndex(meshIndex, blendShapeTargetIndex);
            const int numDeltas = reader->getBlendShapeTargetDeltaCount(meshIndex, blendShapeTargetIndex);
            if (numDeltas != numVertices)
            {
                LOG_ERROR("invalid number of vertices for blendshape");
                return false;
            }
            rl4::ConstArrayView<std::uint32_t> vertexIndices = reader->getBlendShapeTargetVertexIndices(meshIndex, blendShapeTargetIndex);
            for (int deltaIndex = 0; deltaIndex < numDeltas; deltaIndex++)
            {
                if (deltaIndex != int(vertexIndices[deltaIndex]))
                {
                    LOG_ERROR("invalid blendshape delta vertex index");
                    return false;
                }
                const dna::Delta delta = reader->getBlendShapeTargetDelta(meshIndex, blendShapeTargetIndex, deltaIndex);
                modes[meshIndex](3 * vertexIndices[deltaIndex] + 0, blendShapeTargetIndex) = delta.x;
                modes[meshIndex](3 * vertexIndices[deltaIndex] + 1, blendShapeTargetIndex) = delta.y;
                modes[meshIndex](3 * vertexIndices[deltaIndex] + 2, blendShapeTargetIndex) = delta.z;
            }
        }
    }

    facePCA.Create(meshes[0].Vertices(), modes[0]);
    teethPCA.Create(meshes[1].Vertices(), modes[1]);

    // load neck blendshape data
    if (numNeckPCACoeffs > 0) {
        Eigen::Matrix<float, -1, -1, Eigen::RowMajor> neckModes;

        const int numVertices = meshes[0].NumVertices();
        Eigen::Matrix3Xf meanVertices = meshes[0].Vertices();


        neckModes.resize(numVertices * 3, numNeckPCACoeffs - 1);
        for (std::uint16_t bsTargetIndex = 0; bsTargetIndex < numNeckPCACoeffs; bsTargetIndex++)
        {
            const std::uint16_t neckBSTargetIndex = numPCACoeffs + bsTargetIndex;
            const int numDeltas = reader->getBlendShapeTargetDeltaCount(0, neckBSTargetIndex);
            if (numDeltas != numVertices)
            {
                LOG_ERROR("invalid number of vertices for blendshape");
                return false;
            }
            rl4::ConstArrayView<std::uint32_t> vertexIndices = reader->getBlendShapeTargetVertexIndices(0, neckBSTargetIndex);

            if (bsTargetIndex == numNeckPCACoeffs - 1) {
                for (int deltaIndex = 0; deltaIndex < numDeltas; deltaIndex++)
                {
                    if (deltaIndex != int(vertexIndices[deltaIndex]))
                    {
                        LOG_ERROR("invalid blendshape delta vertex index");
                        return false;
                    }
                    const dna::Delta delta = reader->getBlendShapeTargetDelta(0, neckBSTargetIndex, deltaIndex);
                    meanVertices(0, vertexIndices[deltaIndex]) = delta.x;
                    meanVertices(1, vertexIndices[deltaIndex]) = delta.y;
                    meanVertices(2, vertexIndices[deltaIndex]) = delta.z;
                }
            }
            else {
                for (int deltaIndex = 0; deltaIndex < numDeltas; deltaIndex++)
                {
                    if (deltaIndex != int(vertexIndices[deltaIndex]))
                    {
                        LOG_ERROR("invalid blendshape delta vertex index");
                        return false;
                    }
                    const dna::Delta delta = reader->getBlendShapeTargetDelta(0, neckBSTargetIndex, deltaIndex);
                    neckModes(3 * vertexIndices[deltaIndex] + 0, bsTargetIndex) = delta.x;
                    neckModes(3 * vertexIndices[deltaIndex] + 1, bsTargetIndex) = delta.y;
                    neckModes(3 * vertexIndices[deltaIndex] + 2, bsTargetIndex) = delta.z;
                }
            }
        }
        neckPCA.Create(meanVertices, neckModes);
    }

    // load eye transforms
    {
        constexpr float deg2rad = float(CARBON_PI / 180.0);

        Eigen::Transform<float, 3, Eigen::Affine> rootTransform = Eigen::Transform<float, 3, Eigen::Affine>::Identity();
        Eigen::Transform<float, 3, Eigen::Affine> eyeLeftLocalTransform = Eigen::Transform<float, 3, Eigen::Affine>::Identity();
        Eigen::Transform<float, 3, Eigen::Affine> eyeRightLocalTransform = Eigen::Transform<float, 3, Eigen::Affine>::Identity();

        const dna::Vector3 t0 = reader->getNeutralJointTranslation(0);
        const dna::Vector3 rot0 = reader->getNeutralJointRotation(0) * deg2rad;
        rootTransform.linear() = EulerXYZ<float>(rot0.x, rot0.y, rot0.z);
        rootTransform.translation() = Eigen::Vector3f(t0.x, t0.y, t0.z);

        const dna::Vector3 t1 = reader->getNeutralJointTranslation(1);
        const dna::Vector3 rot1 = reader->getNeutralJointRotation(1) * deg2rad;
        eyeLeftLocalTransform.linear() = EulerXYZ<float>(rot1.x, rot1.y, rot1.z);
        eyeLeftLocalTransform.translation() = Eigen::Vector3f(t1.x, t1.y, t1.z);

        const dna::Vector3 t2 = reader->getNeutralJointTranslation(2);
        const dna::Vector3 rot2 = reader->getNeutralJointRotation(2) * deg2rad;
        eyeRightLocalTransform.linear() = EulerXYZ<float>(rot2.x, rot2.y, rot2.z);
        eyeRightLocalTransform.translation() = Eigen::Vector3f(t2.x, t2.y, t2.z);

        rootBindPose = rootTransform;
        eyeLeftTransformPCA.eyeBody.baseTransform = rootTransform * eyeLeftLocalTransform;
        eyeRightTransformPCA.eyeBody.baseTransform = rootTransform * eyeRightLocalTransform;

        eyeLeftTransformPCA.eyeBody.baseVertices = eyeLeftTransformPCA.eyeBody.baseTransform.inverse() * meshes[2].Vertices();
        eyeRightTransformPCA.eyeBody.baseVertices = eyeRightTransformPCA.eyeBody.baseTransform.inverse() * meshes[3].Vertices();

        // we expect the eye joint info to be stored as [translations, rotations]
        auto rotationsAndTranslations =
            Eigen::Map<const Eigen::Matrix<float, -1, -1, Eigen::RowMajor>>(reader->getJointGroupValues(0).data(), 12, numPCACoeffs);
        eyeLeftTransformPCA.linearTransformModel.mean.setZero(6);
        eyeLeftTransformPCA.linearTransformModel.modes.resize(6, numPCACoeffs);
        eyeLeftTransformPCA.linearTransformModel.modes.block(0, 0, 3, numPCACoeffs) = rotationsAndTranslations.block(3, 0, 3, numPCACoeffs) * deg2rad;
        eyeLeftTransformPCA.linearTransformModel.modes.block(3, 0, 3, numPCACoeffs) = rotationsAndTranslations.block(0, 0, 3, numPCACoeffs);
        eyeRightTransformPCA.linearTransformModel.mean.setZero(6);
        eyeRightTransformPCA.linearTransformModel.modes.resize(6, numPCACoeffs);
        eyeRightTransformPCA.linearTransformModel.modes.block(0, 0, 3, numPCACoeffs) = rotationsAndTranslations.block(9, 0, 3, numPCACoeffs) * deg2rad;
        eyeRightTransformPCA.linearTransformModel.modes.block(3, 0, 3, numPCACoeffs) = rotationsAndTranslations.block(6, 0, 3, numPCACoeffs);
    }

    return true;
}

void PCARig::SaveAsNpy(const std::string& filename) const
{
    std::vector<int> offsets;
    offsets.push_back(0);
    offsets.push_back(offsets.back() + facePCA.NumVertices() * 3);
    offsets.push_back(offsets.back() + teethPCA.NumVertices() * 3);
    offsets.push_back(offsets.back() + int(eyeLeftTransformPCA.linearTransformModel.modes.rows()));
    offsets.push_back(offsets.back() + int(eyeRightTransformPCA.linearTransformModel.modes.rows()));

    const int totalCols = facePCA.NumPCAModes() + 1;

    Eigen::Matrix<float, -1, -1> matrix(offsets.back(), totalCols);
    matrix.block(offsets[0], 0, offsets[1] - offsets[0], 1) = facePCA.BaseAsVector();
    matrix.block(offsets[0], 1, offsets[1] - offsets[0], totalCols - 1) = facePCA.Modes(LinearVertexModel<float>::EvaluationMode::STATIC);
    matrix.block(offsets[1], 0, offsets[2] - offsets[1], 1) = teethPCA.BaseAsVector();
    matrix.block(offsets[1], 1, offsets[2] - offsets[1], totalCols - 1) = teethPCA.Modes(LinearVertexModel<float>::EvaluationMode::STATIC);
    matrix.block(offsets[2], 0, offsets[3] - offsets[2], 1) = eyeLeftTransformPCA.linearTransformModel.mean;
    matrix.block(offsets[2], 1, offsets[3] - offsets[2], totalCols - 1) = eyeLeftTransformPCA.linearTransformModel.modes;
    matrix.block(offsets[2], 0, offsets[4] - offsets[3], 1) = eyeRightTransformPCA.linearTransformModel.mean;
    matrix.block(offsets[2], 1, offsets[4] - offsets[3], totalCols - 1) = eyeRightTransformPCA.linearTransformModel.modes;

    TITAN_NAMESPACE::npy::SaveMatrixAsNpy(filename, matrix);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rt)
