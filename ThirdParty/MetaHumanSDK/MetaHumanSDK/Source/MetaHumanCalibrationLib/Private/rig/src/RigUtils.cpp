// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/RigUtils.h>
#include <nls/geometry/EulerAngles.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rigutils)

// hardcoded joint names where skin weights vary between rigs
const std::vector<std::string> jointsWithVarSkinLeft = {
"FACIAL_L_EyelidUpperA","FACIAL_L_EyelidUpperA1","FACIAL_L_EyelashesUpperA1", "FACIAL_L_EyelidUpperA2",
"FACIAL_L_EyelashesUpperA2", "FACIAL_L_EyelidUpperA3", "FACIAL_L_EyelashesUpperA3", "FACIAL_L_EyelidUpperB",
"FACIAL_L_EyelidUpperB1", "FACIAL_L_EyelidUpperB2", "FACIAL_L_EyelidUpperB3", "FACIAL_L_EyelidUpperFurrow",
"FACIAL_L_EyelidUpperFurrow1", "FACIAL_L_EyelidUpperFurrow2", "FACIAL_L_EyelidUpperFurrow3", "FACIAL_L_EyesackUpper",
"FACIAL_L_EyesackUpper1", "FACIAL_L_EyesackUpper2", "FACIAL_L_EyesackUpper3", "FACIAL_L_EyeCornerInner",
"FACIAL_L_EyeCornerInner1", "FACIAL_L_EyeCornerInner2", "FACIAL_L_EyelidLowerA", "FACIAL_L_EyelidLowerA1",
"FACIAL_L_EyelidLowerA2", "FACIAL_L_EyelidLowerA3", "FACIAL_L_EyelidLowerB", "FACIAL_L_EyelidLowerB1",
"FACIAL_L_EyelidLowerB2", "FACIAL_L_EyelidLowerB3", "FACIAL_L_EyeCornerOuter", "FACIAL_L_EyeCornerOuter1",
"FACIAL_L_EyelashesCornerOuter1", "FACIAL_L_EyeCornerOuter2", "head"
};

const std::vector<std::string> jointsWithVarSkinRight = {
"FACIAL_R_EyelidUpperA","FACIAL_R_EyelidUpperA1","FACIAL_R_EyelashesUpperA1", "FACIAL_R_EyelidUpperA2",
"FACIAL_R_EyelashesUpperA2", "FACIAL_R_EyelidUpperA3", "FACIAL_R_EyelashesUpperA3", "FACIAL_R_EyelidUpperB",
"FACIAL_R_EyelidUpperB1", "FACIAL_R_EyelidUpperB2", "FACIAL_R_EyelidUpperB3", "FACIAL_R_EyelidUpperFurrow",
"FACIAL_R_EyelidUpperFurrow1", "FACIAL_R_EyelidUpperFurrow2", "FACIAL_R_EyelidUpperFurrow3", "FACIAL_R_EyesackUpper",
"FACIAL_R_EyesackUpper1", "FACIAL_R_EyesackUpper2", "FACIAL_R_EyesackUpper3", "FACIAL_R_EyeCornerInner",
"FACIAL_R_EyeCornerInner1", "FACIAL_R_EyeCornerInner2", "FACIAL_R_EyelidLowerA", "FACIAL_R_EyelidLowerA1",
"FACIAL_R_EyelidLowerA2", "FACIAL_R_EyelidLowerA3", "FACIAL_R_EyelidLowerB", "FACIAL_R_EyelidLowerB1",
"FACIAL_R_EyelidLowerB2", "FACIAL_R_EyelidLowerB3", "FACIAL_R_EyeCornerOuter", "FACIAL_R_EyeCornerOuter1",
"FACIAL_R_EyelashesCornerOuter1", "FACIAL_R_EyeCornerOuter2", "head"
};

std::map<std::string, int> JointNameToIndexMap(const dna::Reader* reader)
{
    RigGeometry<float> rigGeometry;
    rigGeometry.Init(reader);

    return JointNameToIndexMap(rigGeometry);
}

template <class T>
std::map<std::string, int> JointNameToIndexMap(const RigGeometry<T>& rigGeometry)
{
    std::map<std::string, int> jointNameToIndex;

    const JointRig2<T>& jointRig = rigGeometry.GetJointRig();

    for (const std::string& jointName : jointRig.GetJointNames())
    {
        jointNameToIndex[jointName] = jointRig.GetJointIndex(jointName);
    }

    return jointNameToIndex;
}

template std::map<std::string, int> JointNameToIndexMap(const RigGeometry<float>& rigGeometry);
template std::map<std::string, int> JointNameToIndexMap(const RigGeometry<double>& rigGeometry);

std::vector<int> GetJointsWithVariableSkinning(const dna::Reader* reader, const std::string& regionName)
{
    RigGeometry<float> rigGeo;
    rigGeo.Init(reader);

    std::vector<std::string> jointNames;
    if (regionName == "eye_L")
    {
        jointNames = jointsWithVarSkinLeft;
    }
    else if (regionName == "eye_R")
    {
        jointNames = jointsWithVarSkinRight; 
    }
    else
    {
        CARBON_CRITICAL("GetJointsWithVariableSkinning failed: supported region names are eye_L and eye_R.");
    }

    auto jointNameToIndexMap = JointNameToIndexMap(rigGeo);

    std::vector<int> output;
    for (int i = 0; i < (int)jointNames.size(); ++i)
    {
        output.push_back(jointNameToIndexMap.at(jointNames[i]));
    }

    return output;
}

template <class T>
std::vector<Affine<T, 3, 3>> JointWorldTransforms(const RigGeometry<T>& rigGeometry)
{
    const std::uint16_t numJoints = std::uint16_t(rigGeometry.GetJointRig().NumJoints());

    std::vector<Affine<T, 3, 3>> jointsTransformMatrices(numJoints);
    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        Affine<T, 3, 3> jointWorldTransform(rigGeometry.GetBindMatrix(jointIndex));
        jointsTransformMatrices[jointIndex] = jointWorldTransform;
    }

    return jointsTransformMatrices;
}

template std::vector<Affine<float, 3, 3>> JointWorldTransforms(const RigGeometry<float>& rigGeometry);
template std::vector<Affine<double, 3, 3>> JointWorldTransforms(const RigGeometry<double>& rigGeometry);


template <class T>
std::pair<Eigen::Matrix<T, 3, -1>, Eigen::Matrix<T, 3, -1>> CalculateLocalJointRotationAndTranslation(const RigGeometry<T>& rigGeometry,
                                                                                                      const std::vector<Affine<T, 3, 3>>& worldTransforms)
{
    const JointRig2<T>& jointRig = rigGeometry.GetJointRig();
    const std::uint16_t numJoints = std::uint16_t(jointRig.NumJoints());

    Eigen::Matrix<T, 3, -1> jointTranslations(3, numJoints);
    Eigen::Matrix<T, 3, -1> jointRotations(3, numJoints);
    for (std::uint16_t jointIndex = 0; jointIndex < numJoints; jointIndex++)
    {
        Affine<T, 3, 3> localTransform;
        const int parentJointIndex = jointRig.GetParentIndex(jointIndex);
        if (parentJointIndex >= 0)
        {
            Affine<T, 3, 3> parentTransform = worldTransforms[parentJointIndex];
            localTransform = parentTransform.Inverse() * worldTransforms[jointIndex];
        }
        else
        {
            localTransform = worldTransforms[jointIndex];
        }

        jointTranslations.col(jointIndex) = localTransform.Translation();

        constexpr T rad2deg = T(180.0 / CARBON_PI);
        jointRotations.col(jointIndex) = rad2deg * RotationMatrixToEulerXYZ<T>(localTransform.Linear());
    }

    return std::pair<Eigen::Matrix<T, 3, -1>, Eigen::Matrix<T, 3, -1>>(jointRotations, jointTranslations);
}

template std::pair<Eigen::Matrix<float, 3, -1>, Eigen::Matrix<float, 3, -1>> CalculateLocalJointRotationAndTranslation(const RigGeometry<float>& rigGeometry,
                                                                                                                       const std::vector<Affine<float, 3,
                                                                                                                                                3>>& worldTransforms);
template std::pair<Eigen::Matrix<double, 3, -1>, Eigen::Matrix<double, 3, -1>> CalculateLocalJointRotationAndTranslation(const RigGeometry<double>& rigGeometry,
                                                                                                                         const std::vector<Affine<double, 3,
                                                                                                                                                  3>>& worldTransforms);

template <class T>
std::vector<Affine<T, 3, 3>> WorldToLocalJointDeltas(const RigGeometry<T>& rigGeometry, const std::vector<Affine<T, 3, 3>>& worldJointDeltas)
{
    const int numJoints = (int)worldJointDeltas.size();
    if (numJoints != rigGeometry.GetJointRig().NumJoints())
    {
        CARBON_CRITICAL("Input joint delta number {} but rig contains {}", numJoints, rigGeometry.GetJointRig().NumJoints());
    }

    const JointRig2<T>& jointRig = rigGeometry.GetJointRig();
    const int jointHiearchyLevels = (int)jointRig.GetJointsPerHierarchyLevel().size();
    std::vector<Affine<T, 3, 3>> worldJointAbsolute(numJoints);
    std::vector<Affine<T, 3, 3>> localJointDeltas(numJoints);

    for (int jointIndex = 0; jointIndex < numJoints; ++jointIndex)
    {
        worldJointAbsolute[jointIndex] = Affine<T, 3, 3>(rigGeometry.GetBindMatrix(jointIndex)) * worldJointDeltas[jointIndex];
    }

    for (int h = 0; h < jointHiearchyLevels; ++h)
    {
        auto jointIndicesForLevel = jointRig.GetJointsPerHierarchyLevel()[h];
        for (int j = 0; j < (int)jointIndicesForLevel.size(); j++)
        {
            auto jointIndex = jointIndicesForLevel[j];
            const int parentJointIndex = jointRig.GetParentIndex(jointIndex);
            if (parentJointIndex >= 0)
            {
                const auto parentTransform = worldJointAbsolute[parentJointIndex];
                localJointDeltas[jointIndex] = parentTransform.Inverse() * worldJointAbsolute[jointIndex];
            }
            else
            {
                localJointDeltas[jointIndex] = worldJointAbsolute[jointIndex];
            }

            const auto restAffine = Affine<T, 3, 3>(rigGeometry.GetRestMatrix(jointIndex));
            localJointDeltas[jointIndex].SetLinear(restAffine.Linear().transpose() * localJointDeltas[jointIndex].Linear());
            localJointDeltas[jointIndex].SetTranslation(localJointDeltas[jointIndex].Translation() - restAffine.Translation());
        }
    }

    return localJointDeltas;
}

template std::vector<Affine<float, 3, 3>> WorldToLocalJointDeltas(const RigGeometry<float>& rigGeometry, const std::vector<Affine<float, 3,
                                                                                                                                  3>>& worldTransforms);
template std::vector<Affine<double, 3, 3>> WorldToLocalJointDeltas(const RigGeometry<double>& rigGeometry,
                                                                   const std::vector<Affine<double, 3, 3>>& worldTransforms);

void ReindexControls(dna::Reader* reader, dna::Writer* writer, const std::map<int, int>& mapping, const std::vector<std::string>& newControlNames)
{
    writer->setFileFormatVersion(5);

    // raw controls
    writer->clearRawControlNames();
    for (int index = 0; index < (int)newControlNames.size(); ++index)
    {
        writer->setRawControlName(uint16_t(index), newControlNames[index].c_str());
    }

    auto oldIndices = reader->getGUIToRawOutputIndices();
    std::vector<uint16_t> newIndices;
    for (int i = 0; i < (int)oldIndices.size(); ++i)
    {
        auto newIndex = mapping.at((int)oldIndices[i]);
        newIndices.push_back((uint16_t)newIndex);
    }
    writer->setGUIToRawOutputIndices(newIndices.data(), (uint16_t)newIndices.size());

    // psd
    oldIndices = reader->getPSDColumnIndices();
    newIndices.clear();
    for (int i = 0; i < (int)oldIndices.size(); ++i)
    {
        auto newIndex = mapping.at((int)oldIndices[i]);
        newIndices.push_back((uint16_t)newIndex);
    }
    writer->setPSDColumnIndices(newIndices.data(), (uint16_t)newIndices.size());

    oldIndices = reader->getPSDRowIndices();
    newIndices.clear();
    for (int i = 0; i < (int)oldIndices.size(); ++i)
    {
        auto newIndex = mapping.at((int)oldIndices[i]);
        newIndices.push_back((uint16_t)newIndex);
    }
    writer->setPSDRowIndices(newIndices.data(), (uint16_t)newIndices.size());

    oldIndices = reader->getBlendShapeChannelInputIndices();
    newIndices.clear();
    for (int i = 0; i < (int)oldIndices.size(); ++i)
    {
        auto newIndex = mapping.at((int)oldIndices[i]);
        newIndices.push_back((uint16_t)newIndex);
    }
    writer->setBlendShapeChannelInputIndices(newIndices.data(), (uint16_t)newIndices.size());

    oldIndices = reader->getAnimatedMapInputIndices();
    newIndices.clear();
    for (int i = 0; i < (int)oldIndices.size(); ++i)
    {
        auto newIndex = mapping.at((int)oldIndices[i]);
        newIndices.push_back((uint16_t)newIndex);
    }
    writer->setAnimatedMapInputIndices(newIndices.data(), (uint16_t)newIndices.size());

    const auto jointGroups = reader->getJointGroupCount();

    for (uint16_t jointGroup = 0; jointGroup < jointGroups; ++jointGroup)
    {
        oldIndices = reader->getJointGroupInputIndices(jointGroup);
        newIndices.clear();
        for (int i = 0; i < (int)oldIndices.size(); ++i)
        {
            auto newIndex = mapping.at((int)oldIndices[i]);
            newIndices.push_back((uint16_t)newIndex);
        }
        writer->setJointGroupInputIndices(jointGroup, newIndices.data(), (uint16_t)newIndices.size());
    }
}

void WriteFacePoseBehavior(dna::Writer* writer, dna::Reader* rbfReader)
{
    const auto rbfPoseControlCount = rbfReader->getRBFPoseControlCount();
    for (uint16_t ctrl = 0; ctrl < rbfPoseControlCount; ++ctrl)
    {
        writer->setRBFPoseControlName(ctrl, rbfReader->getRBFPoseControlName(ctrl));
    }

    const auto rbfPoseCount = rbfReader->getRBFPoseCount();
    for (uint16_t pose = 0; pose < rbfPoseCount; ++pose)
    {
        writer->setRBFPoseName(pose, rbfReader->getRBFPoseName(pose));
        writer->setRBFPoseScale(pose, rbfReader->getRBFPoseScale(pose));
        writer->setRBFPoseOutputControlIndices(pose,
                                               rbfReader->getRBFPoseOutputControlIndices(pose).data(),
                                               (uint16_t)rbfReader->getRBFPoseOutputControlIndices(pose).size());
        writer->setRBFPoseOutputControlWeights(pose,
                                               rbfReader->getRBFPoseOutputControlWeights(pose).data(),
                                               (uint16_t)rbfReader->getRBFPoseOutputControlWeights(pose).size());
    }
}

void WriteRbfBehavior(dna::Writer* writer, dna::Reader* rbfReader)
{
    const auto lodCount = rbfReader->getLODCount();
    for (uint16_t lod = 0; lod < lodCount; ++lod)
    {
        writer->setRBFSolverIndices(0, rbfReader->getRBFSolverIndicesForLOD(lod).data(), (uint16_t)rbfReader->getRBFSolverIndicesForLOD(lod).size());
        writer->setLODRBFSolverMapping(lod, 0);
    }

    const auto rbfPoseCount = rbfReader->getRBFPoseCount();
    for (uint16_t pose = 0; pose < rbfPoseCount; ++pose)
    {
        writer->setRBFPoseName(pose, rbfReader->getRBFPoseName(pose));
        writer->setRBFPoseScale(pose, rbfReader->getRBFPoseScale(pose));
        writer->setRBFPoseOutputControlIndices(pose,
                                               rbfReader->getRBFPoseOutputControlIndices(pose).data(),
                                               (uint16_t)rbfReader->getRBFPoseOutputControlIndices(pose).size());
        writer->setRBFPoseOutputControlWeights(pose,
                                               rbfReader->getRBFPoseOutputControlWeights(pose).data(),
                                               (uint16_t)rbfReader->getRBFPoseOutputControlWeights(pose).size());
    }

    const auto rbfSolverCount = rbfReader->getRBFSolverCount();
    for (uint16_t solver = 0; solver < rbfSolverCount; ++solver)
    {
        writer->setRBFSolverName(solver, rbfReader->getRBFSolverName(solver));
        writer->setRBFSolverType(solver, rbfReader->getRBFSolverType(solver));
        writer->setRBFSolverRadius(solver, rbfReader->getRBFSolverRadius(solver));
        writer->setRBFSolverAutomaticRadius(solver, rbfReader->getRBFSolverAutomaticRadius(solver));
        writer->setRBFSolverWeightThreshold(solver, rbfReader->getRBFSolverWeightThreshold(solver));
        writer->setRBFSolverDistanceMethod(solver, rbfReader->getRBFSolverDistanceMethod(solver));
        writer->setRBFSolverNormalizeMethod(solver, rbfReader->getRBFSolverNormalizeMethod(solver));
        writer->setRBFSolverFunctionType(solver, rbfReader->getRBFSolverFunctionType(solver));
        writer->setRBFSolverTwistAxis(solver, rbfReader->getRBFSolverTwistAxis(solver));

        writer->setRBFSolverPoseIndices(solver, rbfReader->getRBFSolverPoseIndices(solver).data(), (uint16_t)rbfReader->getRBFSolverPoseIndices(solver).size());
        writer->setRBFSolverRawControlValues(solver, rbfReader->getRBFSolverRawControlValues(solver).data(),
                                             (uint16_t)rbfReader->getRBFSolverRawControlValues(solver).size());
        writer->setRBFSolverRawControlIndices(solver,
                                              rbfReader->getRBFSolverRawControlIndices(solver).data(),
                                              (uint16_t)rbfReader->getRBFSolverRawControlIndices(solver).size());
    }
}

void AddRBFLayerToDnaStream(dna::Reader* reader, dna::Reader* rbfReader, dna::Writer* writer)
{
    const auto numJoints = reader->getJointCount();

    for (uint16_t jnt = 0; jnt < numJoints; ++jnt)
    {
        writer->setJointTranslationRepresentation(jnt, dna::TranslationRepresentation::Vector);
        writer->setJointRotationRepresentation(jnt, dna::RotationRepresentation::EulerAngles);
        writer->setJointScaleRepresentation(jnt, dna::ScaleRepresentation::Vector);
    }

    auto oldControlCount = reader->getRawControlCount();

    std::vector<std::string> oldControlNames;
    for (uint16_t index = 0; index < oldControlCount; ++index)
    {
        oldControlNames.push_back(std::string(reader->getRawControlName(index)));
    }

    const auto rbfDnaControlCount = rbfReader->getRawControlCount();

    std::vector<std::string> rbfDnaControlNames;
    for (uint16_t index = 0; index < rbfDnaControlCount; ++index)
    {
        rbfDnaControlNames.push_back(std::string(rbfReader->getRawControlName(index)));
    }

    // old index : new index
    std::map<int, int> mapping;
    std::vector<int> missingOldRawControls;
    for (auto i = 0; i < (int)oldControlNames.size(); ++i)
    {
        const auto& oldCtrlName = oldControlNames[i];
        bool matchFound = false;
        for (auto j = 0; j < (int)rbfDnaControlNames.size(); ++j)
        {
            const auto& currCtrlName = rbfDnaControlNames[j];
            if (oldCtrlName == currCtrlName)
            {
                mapping[i] = j;
                matchFound = true;
                break;
            }
        }

        if (!matchFound)
        {
            missingOldRawControls.push_back(i);
        }
    }

    const auto psdCount = reader->getPSDCount();
    for (uint16_t psd = 0; psd < psdCount; ++psd)
    {
        mapping[oldControlCount + psd] = rbfDnaControlCount + psd;
    }
    const auto mlCount = reader->getMLControlCount();
    for (uint16_t ml = 0; ml < mlCount; ++ml)
    {
        mapping[oldControlCount + psdCount + ml] = rbfDnaControlCount + psdCount + ml;
    }

    const auto rbfStartIndex = rbfDnaControlCount + psdCount + mlCount;
    for (uint16_t rbf = 0; rbf < (uint16_t)missingOldRawControls.size(); ++rbf)
    {
        mapping[missingOldRawControls[rbf]] = rbfStartIndex + rbf;
    }

    ReindexControls(reader, writer, mapping, rbfDnaControlNames);
    WriteFacePoseBehavior(writer, rbfReader);
    WriteRbfBehavior(writer, rbfReader);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rigutils)
