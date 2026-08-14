// Copyright Epic Games, Inc. All Rights Reserved.

#include <ActorRefinementAPI.h>

#include <carbon/utils/TaskThreadPool.h>
#include <Common.h>
#include <rig/RigLogicDNAResource.h>
#include <pma/PolyAllocator.h>
#include <pma/utils/ManagedInstance.h>
#include <rigmorpher/RigMorphModule.h>
#include <conformer/TeethAlignment.h>
#include "Internals/ActorRefinementUtils.h"

using namespace TITAN_NAMESPACE;

namespace TITAN_API_NAMESPACE
{

struct ActorRefinementAPI::Private
{
    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> globalThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(
        /*createIfNotAvailable=*/true);
    std::map<RefinementMaskType, VertexWeights<float>> masks;

    // TO DO: default values set this way for now to avoid breaking API changes
    // modified with "SetDrivingMeshNames", "SetMeshCorrespondance" and "SetDrivenJointNames"
    std::vector<std::string> drivingMeshes = { "head_lod0_mesh", "teeth_lod0_mesh", "eyeLeft_lod0_mesh", "eyeRight_lod0_mesh" };

    std::vector<std::string> jointsToOptimize = {};

    std::vector<std::string> inactiveJoints = { "spine_04", "spine_05", "clavicle_pec_l", "clavicle_pec_r", "spine_04_latissimus_l", "spine_04_latissimus_r",
                                                "clavicle_l", "clavicle_out_l", "clavicle_scap_l", "upperarm_l", "upperarm_correctiveRoot_l",
                                                "upperarm_out_l", "upperarm_fwd_l", "upperarm_in_l", "upperarm_bck_l",
                                                "clavicle_r", "clavicle_out_r", "clavicle_scap_r", "upperarm_r", "upperarm_correctiveRoot_r",
                                                "upperarm_out_r", "upperarm_fwd_r", "upperarm_in_r", "upperarm_bck_r", "neck_01",
                                                "FACIAL_C_Neck1Root", "neck_02", "FACIAL_C_Neck2Root", "head", "FACIAL_C_FacialRoot" };

    std::map<std::string, std::vector<std::string>> deltaTransferMeshes = {
        { "head_lod0_mesh", { "cartilage_lod0_mesh" } },
        { "teeth_lod0_mesh", { "saliva_lod0_mesh", "saliva_lod1_mesh", "saliva_lod2_mesh" } }
    };

    std::map<std::string, std::vector<std::string>> rigidTransformMeshes = {
        { "head_lod0_mesh", { "head_lod6_mesh", "head_lod7_mesh" } },
        { "teeth_lod0_mesh", { "teeth_lod5_mesh", "teeth_lod6_mesh", "teeth_lod7_mesh" } }
    };

    std::map<std::string, std::vector<std::string>> uvProjectionMeshes = {
        { "head_lod0_mesh", { "head_lod1_mesh", "head_lod2_mesh", "head_lod3_mesh", "head_lod4_mesh", "head_lod5_mesh" } },
        { "teeth_lod0_mesh", { "teeth_lod1_mesh", "teeth_lod2_mesh", "teeth_lod3_mesh", "teeth_lod4_mesh" } },
        { "eyeLeft_lod0_mesh",
          { "eyeLeft_lod1_mesh", "eyeLeft_lod2_mesh", "eyeLeft_lod3_mesh", "eyeLeft_lod4_mesh", "eyeLeft_lod5_mesh", "eyeLeft_lod6_mesh",
            "eyeLeft_lod7_mesh" } },
        { "eyeRight_lod0_mesh",
          { "eyeRight_lod1_mesh", "eyeRight_lod2_mesh", "eyeRight_lod3_mesh", "eyeRight_lod4_mesh", "eyeRight_lod5_mesh", "eyeRight_lod6_mesh",
            "eyeRight_lod7_mesh" } } };

    std::map<std::string, std::vector<std::string>> drivenJoints = {
        { "eyeLeft_lod0_mesh", { "FACIAL_L_Eye" } },
        { "eyeRight_lod0_mesh", { "FACIAL_R_Eye" } },
        { "teeth_lod0_mesh", { "FACIAL_C_TeethUpper", "FACIAL_C_TeethLower" } }
    };

    std::map<std::string, std::vector<std::string>> dependentJoints = {}; /*{
                                                                             { "FACIAL_L_Eye", { "FACIAL_L_EyelidUpperA",
                                                                                "FACIAL_L_EyelidUpperB","FACIAL_L_EyelidLowerA","FACIAL_L_EyelidLowerB" } },
                                                                             { "FACIAL_R_Eye", { "FACIAL_R_EyelidUpperA", "FACIAL_R_EyelidUpperB",
                                                                                "FACIAL_R_EyelidLowerA", "FACIAL_R_EyelidLowerB" } }
                                                                             };*/
};


ActorRefinementAPI::ActorRefinementAPI()
    : m(new Private())
{}

ActorRefinementAPI::~ActorRefinementAPI()
{
    delete m;
}

bool ActorRefinementAPI::UpdateRigWithTeethMeshVertices(dna::Reader* InDnaStream, const float* InTeethMeshVertexPositions, dna::Writer* OutDnaStream)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InDnaStream, false, "input dna stream not valid");

        const int headMeshDnaId = 0;
        const int teethMeshDnaId = 1;
        const std::string headMeshName = (std::string)InDnaStream->getMeshName(headMeshDnaId);
        const std::string teethMeshName = (std::string)InDnaStream->getMeshName(teethMeshDnaId);

        const auto drivenJointsIt = m->drivenJoints.find(teethMeshName);
        if (drivenJointsIt == m->drivenJoints.end())
        {
            LOG_ERROR("Dna file not supported with defined joint and mesh names.");
            return false;
        }

        const auto deltaTransferMeshesIt = m->deltaTransferMeshes.find(teethMeshName);
        if (deltaTransferMeshesIt == m->deltaTransferMeshes.end())
        {
            LOG_ERROR("Dna file not supported with defined joint and mesh names.");
            return false;
        }
        const auto rigidTransformMeshesIt = m->rigidTransformMeshes.find(teethMeshName);
        if (rigidTransformMeshesIt == m->rigidTransformMeshes.end())
        {
            LOG_ERROR("Dna file not supported with defined joint and mesh names.");
            return false;
        }
        const auto uvProjectionMeshesIt = m->uvProjectionMeshes.find(teethMeshName);
        if (uvProjectionMeshesIt == m->uvProjectionMeshes.end())
        {
            LOG_ERROR("Dna file not supported with defined joint and mesh names.");
            return false;
        }

        const std::vector<std::string> drivenJoints = drivenJointsIt->second;
        const std::vector<std::string> deltaTranferMeshes = deltaTransferMeshesIt->second;
        const std::vector<std::string> rigidTransformMeshes = rigidTransformMeshesIt->second;
        const std::vector<std::string> uvProjectionMeshes = uvProjectionMeshesIt->second;

        Eigen::Matrix3Xf verticesMap = Eigen::Map<const Eigen::Matrix<float, 3, -1, Eigen::ColMajor>>(
            (const float*)InTeethMeshVertexPositions,
            3,
            InDnaStream->getVertexPositionCount(teethMeshDnaId))
            .template cast<float>();

        VertexWeights<float> weights(int(InDnaStream->getVertexPositionCount(headMeshDnaId)), 0.0f);
        auto it = m->masks.find(RefinementMaskType::MOUTH_SOCKET);
        if (it != m->masks.end())
        {
            weights = m->masks[RefinementMaskType::MOUTH_SOCKET];
        }

        RigMorphModule<float>::UpdateTeeth(InDnaStream,
                                           OutDnaStream,
                                           verticesMap,
                                           teethMeshName,
                                           headMeshName,
                                           drivenJoints,
                                           deltaTranferMeshes,
                                           rigidTransformMeshes,
                                           uvProjectionMeshes,
                                           weights,
                                           /*gridSize=*/64);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to fit teeth: {}",
                         e.what());
    }
}

bool ActorRefinementAPI::RefineRig(dna::Reader* InDnaStream,
                                   std::map<std::string, const float*> InVertexPositions,
                                   dna::Writer* OutDnaStream,
                                   std::map<std::string, std::tuple<std::string, std::vector<int>,
                                                                    std::vector<std::vector<float>>>>& OutDeltaTransferCorrespondanceData)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InDnaStream, false, "input dna stream not valid");
        TITAN_CHECK_OR_RETURN(OutDnaStream, false, "output dna stream not valid");
        std::map<std::string, int> meshNameToIndex;
        for (uint16_t i = 0; i < InDnaStream->getMeshCount(); ++i)
        {
            const auto meshName = (std::string)InDnaStream->getMeshName(i);
            meshNameToIndex[meshName] = (int)i;
        }

        std::map<std::string, Eigen::Matrix<float, 3, -1>> targets;
        for (const auto& [key, value] : InVertexPositions)
        {
            const auto& indexIt = meshNameToIndex.find(key);
            TITAN_CHECK_OR_RETURN(indexIt != meshNameToIndex.end(), false, "input data not valid - target mesh do not exist in dna file");
            const int meshId = indexIt->second;
            targets[key] = Eigen::Map<const Eigen::Matrix<float, 3, -1, Eigen::ColMajor>>(
                (const float*)value,
                3,
                (uint32_t)InDnaStream->getVertexPositionCount(uint16_t(meshId))).template cast<float>();
        }

        VertexWeights<float> gridDeformWeights(int(InDnaStream->getVertexPositionCount(0)), 1.0f);
        auto msIt = m->masks.find(RefinementMaskType::MOUTH_SOCKET);
        if (msIt != m->masks.end())
        {
            const auto maskedMouthSocketWeights = m->masks[RefinementMaskType::MOUTH_SOCKET];
            gridDeformWeights = VertexWeights<float>(Eigen::VectorXf::Ones(
                                                                    maskedMouthSocketWeights.NumVertices()) - maskedMouthSocketWeights.Weights());
        }

        RigMorphModule<float>::Morph(InDnaStream,
                                     OutDnaStream,
                                     targets,
                                     m->drivingMeshes,
                                     m->inactiveJoints,
                                     m->drivenJoints,
                                     m->dependentJoints,
                                     m->jointsToOptimize,
                                     m->deltaTransferMeshes,
                                     m->rigidTransformMeshes,
                                     m->uvProjectionMeshes,
                                     gridDeformWeights,
                                     /*gridSize*/128);
        OutDeltaTransferCorrespondanceData = RigMorphModule<float>::CollectDeltaTransferCorrespondences(InDnaStream, m->deltaTransferMeshes);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to modify dna: {}", e.what());
    }
}

bool ActorRefinementAPI::UpdateRigWithHeadMeshVertices(dna::Reader* InDnaStream,
                                                       const float* InHeadMeshVertexPositions,
                                                       const float* InTeethMeshVertexPositions,
                                                       const float* InEyeLeftMeshVertexPositions,
                                                       const float* InEyeRightMeshVertexPositions,
                                                       dna::Writer* OutDnaStream)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InDnaStream, false, "input dna stream not valid");
        TITAN_CHECK_OR_RETURN(OutDnaStream, false, "output dna stream not valid");
        const int headMeshDnaId = 0;

        std::map<std::string, Eigen::Matrix<float, 3, -1>> targets;
        targets["head_lod0_mesh"] = Eigen::Map<const Eigen::Matrix<float, 3, -1, Eigen::ColMajor>>(
            (const float*)InHeadMeshVertexPositions,
            3,
            InDnaStream->getVertexPositionCount(0)).template cast<float>();

        if (InEyeLeftMeshVertexPositions && InEyeRightMeshVertexPositions)
        {
            targets["eyeLeft_lod0_mesh"] = Eigen::Map<const Eigen::Matrix<float, 3, -1, Eigen::ColMajor>>(
                (const float*)InEyeLeftMeshVertexPositions,
                3,
                InDnaStream->getVertexPositionCount(3)).template cast<float>();

            targets["eyeRight_lod0_mesh"] = Eigen::Map<const Eigen::Matrix<float, 3, -1, Eigen::ColMajor>>(
                (const float*)InEyeRightMeshVertexPositions,
                3,
                InDnaStream->getVertexPositionCount(4)).template cast<float>();
        }
        if (InTeethMeshVertexPositions)
        {
            targets["teeth_lod0_mesh"] = Eigen::Map<const Eigen::Matrix<float, 3, -1, Eigen::ColMajor>>(
                (const float*)InTeethMeshVertexPositions,
                3,
                InDnaStream->getVertexPositionCount(1)).template cast<float>();
        }

        VertexWeights<float> gridDeformWeights(int(InDnaStream->getVertexPositionCount(headMeshDnaId)), 1.0f);
        auto msIt = m->masks.find(RefinementMaskType::MOUTH_SOCKET);
        if ((msIt != m->masks.end()) && !InTeethMeshVertexPositions)
        {
            const auto maskedMouthSocketWeights = m->masks[RefinementMaskType::MOUTH_SOCKET];
            gridDeformWeights = VertexWeights<float>(Eigen::VectorXf::Ones(
                                                                    maskedMouthSocketWeights.NumVertices()) - maskedMouthSocketWeights.Weights());
        }

        RigMorphModule<float>::Morph(InDnaStream, OutDnaStream,
                                     targets,
                                     m->drivingMeshes,
                                     m->inactiveJoints,
                                     m->drivenJoints,
                                     m->dependentJoints,
                                     m->jointsToOptimize,
                                     m->deltaTransferMeshes,
                                     m->rigidTransformMeshes,
                                     m->uvProjectionMeshes,
                                     gridDeformWeights,
                                     /*gridSize*/128);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to modify dna: {}", e.what());
    }
}

bool ActorRefinementAPI::CheckControlsConfig(const char* InControlsConfigJson)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InControlsConfigJson, false, "input controls json is not valid");
        pma::PolyAllocator<TeethAlignment<float>> polyAllocator{ MEM_RESOURCE };
        std::shared_ptr<TeethAlignment<float>> teethToRigAlignment = std::allocate_shared<TeethAlignment<float>>(polyAllocator);
        return teethToRigAlignment->CheckControlsConfig(InControlsConfigJson);
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("Controls config is not valid: {}", e.what());
    }
}

bool ActorRefinementAPI::RefineTeethPlacement(dna::Reader* InDnaStream, dna::Reader* InRefDnaStream, const char* InControlsConfigJson,
                                              dna::Writer* OutDnaStream)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InDnaStream, false, "input dna stream not valid");
        TITAN_CHECK_OR_RETURN(InRefDnaStream, false, "input referent dna stream not valid");
        TITAN_CHECK_OR_RETURN(OutDnaStream, false, "output dna stream not valid");
        TITAN_CHECK_OR_RETURN(InControlsConfigJson, false, "input controls json is not valid");
        pma::PolyAllocator<TeethAlignment<float>> polyAllocator { MEM_RESOURCE };
        const int teethMeshDnaId = 1;
        VertexWeights<float> teethPlacementVertices(int(InDnaStream->getVertexPositionCount(teethMeshDnaId)), 1.0f);
        auto msIt = m->masks.find(RefinementMaskType::TEETH_PLACEMENT);
        if (msIt != m->masks.end())
        {
            teethPlacementVertices = m->masks[RefinementMaskType::TEETH_PLACEMENT];
        }
        std::shared_ptr<TeethAlignment<float>> teethToRigAlignment = std::allocate_shared<TeethAlignment<float>>(polyAllocator);
        teethToRigAlignment->LoadRig(InRefDnaStream, InDnaStream);
        teethToRigAlignment->LoadControlsToEvaluate(InControlsConfigJson);
        teethToRigAlignment->SetInterfaceVertices(teethPlacementVertices);
        const auto [resultScale, resultTransform] = teethToRigAlignment->Align(Affine<float, 3, 3>());

        const Eigen::Matrix<float, 3, -1> resultVertices = resultScale * resultTransform.Transform(teethToRigAlignment->CurrentVertices());
        UpdateRigWithTeethMeshVertices(InDnaStream, (float*)resultVertices.data(), OutDnaStream);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to modify dna: {}", e.what());
    }
}

bool ActorRefinementAPI::TransformRigOrigin(dna::Reader* InDnaStream, const float* InTransformMatrix, dna::Writer* OutDnaStream)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InDnaStream, false, "input dna stream not valid");
        TITAN_CHECK_OR_RETURN(OutDnaStream, false, "output dna stream not valid");

        Eigen::Matrix4f transformMap = Eigen::Map<const Eigen::Matrix<float, 4, 4, Eigen::ColMajor>>(
            (const float*)InTransformMatrix,
            4,
            4).template cast<float>();

        Affine<float, 3, 3> rigTransformation;
        rigTransformation.SetMatrix(transformMap);
        RigMorphModule<float>::ApplyRigidTransform(InDnaStream, OutDnaStream, rigTransformation);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to modify dna: {}", e.what());
    }
}

bool ActorRefinementAPI::ScaleRig(dna::Reader* InDnaStream, const float InScale, const float* InScalingPivot, dna::Writer* OutDnaStream)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InDnaStream, false, "input dna stream not valid");
        TITAN_CHECK_OR_RETURN(OutDnaStream, false, "output dna stream not valid");

        Eigen::Vector3f pivotMap = Eigen::Map<const Eigen::Vector3f>((const float*)InScalingPivot, 3).template cast<float>();
        RigMorphModule<float>::ApplyScale(InDnaStream, OutDnaStream, InScale, pivotMap);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to modify dna: {}", e.what());
    }
}

bool ActorRefinementAPI::ScaleAndTransformRig(dna::Reader* InDnaStream,
                                              const float* InTransformMatrix,
                                              const float InScale,
                                              const float* InScalingPivot,
                                              dna::Writer* OutDnaStream)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InDnaStream, false, "input dna stream not valid");
        TITAN_CHECK_OR_RETURN(OutDnaStream, false, "output dna stream not valid");

        Eigen::Vector3f pivotMap = Eigen::Map<const Eigen::Vector3f>((const float*)InScalingPivot, 3).template cast<float>();
        Eigen::Matrix4f transformMap = Eigen::Map<const Eigen::Matrix<float, 4, 4, Eigen::ColMajor>>(
            (const float*)InTransformMatrix,
            4,
            4).template cast<float>();

        Affine<float, 3, 3> rigTransformation;
        rigTransformation.SetMatrix(transformMap);

        RigMorphModule<float>::ApplyScale(InDnaStream, OutDnaStream, InScale, pivotMap);
        RigMorphModule<float>::ApplyRigidTransform(InDnaStream, OutDnaStream, rigTransformation);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to modify dna: {}", e.what());
    }
}

bool ActorRefinementAPI::GetRefinementMask(float* OutVertexWeights, RefinementMaskType InMaskType) const
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(!m->masks.empty(), false, "frame data is empty");

        // mask
        const auto& weights = m->masks[InMaskType].Weights();

        memcpy(OutVertexWeights,
               weights.data(),
               int32_t(weights.cols() * weights.rows()) *
               sizeof(float));

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to get mask: {}", e.what());
    }
}

bool ActorRefinementAPI::SetRefinementMask(int32_t numVertices, const float* InVertexWeights, RefinementMaskType InMaskType)
{
    try
    {
        TITAN_RESET_ERROR;

        Eigen::VectorXf weightsMap = Eigen::Map<const Eigen::VectorXf>(
            (const float*)InVertexWeights,
            numVertices).template cast<float>();

        m->masks[InMaskType] = VertexWeights<float>(weightsMap);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to set mask: {}", e.what());
    }
}

bool ActorRefinementAPI::ApplyDNA(dna::Reader* InDna, dna::Reader* InDeltaDna, dna::Writer* OutFinalDna, const std::vector<float>& InMask)
{
    try
    {
        TITAN_RESET_ERROR;
        ApplyDNAInternal(InDna, InDeltaDna, OutFinalDna, Operation::Add, InMask);
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to apply dna: {}", e.what());
    }
}

bool ActorRefinementAPI::GenerateDeltaDNA(dna::Reader* InFromDna, dna::Reader* InToDna, dna::Writer* OutDeltaDna, const std::vector<float>& InMask)
{
    try
    {
        TITAN_RESET_ERROR;
        ApplyDNAInternal(InFromDna, InToDna, OutDeltaDna, Operation::Substract, InMask);
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to generate delta dna: {}", e.what());
    }
}

bool ActorRefinementAPI::SetDrivingMeshNames(const std::vector<std::string>& InDrivingMeshNames)
{
    try
    {
        TITAN_RESET_ERROR;
        m->drivingMeshes = InDrivingMeshNames;
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to set driving mesh names: {}", e.what());
    }
}

bool ActorRefinementAPI::SetInactiveJointNames(const std::vector<std::string>& InInactiveJointNames)
{
    try
    {
        TITAN_RESET_ERROR;
        m->inactiveJoints = InInactiveJointNames;
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to set driving mesh names: {}", e.what());
    }
}

bool ActorRefinementAPI::SetOptimizationJointNames(const std::vector<std::string>& InOptimizationJointNames)
{
    try
    {
        TITAN_RESET_ERROR;
        m->jointsToOptimize = InOptimizationJointNames;
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to set driving mesh names: {}", e.what());
    }
}

bool ActorRefinementAPI::SetMeshCorrespondance(const std::map<std::string, std::vector<std::string>>& InDrivenMeshNames,
                                               RefinementMeshCorrespondenceType InCorrespondanceType)
{
    try
    {
        TITAN_RESET_ERROR;
        if (InCorrespondanceType == RefinementMeshCorrespondenceType::DELTA_TRANSFER)
        {
            m->deltaTransferMeshes = InDrivenMeshNames;
        }
        else if (InCorrespondanceType == RefinementMeshCorrespondenceType::RIGID)
        {
            m->rigidTransformMeshes = InDrivenMeshNames;
        }
        else
        {
            m->uvProjectionMeshes = InDrivenMeshNames;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to set driving mesh names: {}", e.what());
    }
}

bool ActorRefinementAPI::SetDrivenJointNames(const std::map<std::string, std::vector<std::string>>& InDrivenJointNames)
{
    try
    {
        TITAN_RESET_ERROR;
        m->drivenJoints = InDrivenJointNames;
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to set driven joint names: {}", e.what());
    }
}

bool ActorRefinementAPI::SetDependentJointNames(const std::map<std::string, std::vector<std::string>>& InDependentJointNames)
{
    try
    {
        TITAN_RESET_ERROR;
        m->dependentJoints = InDependentJointNames;
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to set driven joint names: {}", e.what());
    }
}

} // namespace TITAN_API_NAMESPACE
