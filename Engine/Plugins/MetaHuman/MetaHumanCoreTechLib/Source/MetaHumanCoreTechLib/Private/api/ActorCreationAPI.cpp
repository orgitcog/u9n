// Copyright Epic Games, Inc. All Rights Reserved.

#include <ActorCreationAPI.h>

#include "Internals/ActorCreationUtils.h"
#include "Internals/ReferenceAligner.h"
#include "Internals/FrameInputData.h"
#include "Internals/OpenCVCamera2MetaShapeCamera.h"
#include <nls/serialization/CameraSerialization.h>
#include <carbon/utils/TaskThreadPool.h>
#include <Common.h>
#include <conformer/BrowLandmarksGenerator.h>
#include <conformer/FaceFitting.h>
#include <conformer/FittingInitializer.h>
#include <conformer/IdentityModelFitting.h>
#include <conformer/EyeFittingHelper.h>
#include <conformer/TargetLandmarksHandler.h>
#include <dna/BinaryStreamWriter.h>
#include <nls/geometry/GeometryHelpers.h>
#include <nls/geometry/Procrustes.h>
#include <rig/RigLogicDNAResource.h>
#include <nls/serialization/ObjFileFormat.h>
#include <nrr/landmarks/LandmarkConfiguration.h>
#include <nrr/landmarks/LandmarkSequence.h>
#include <nrr/TemplateDescription.h>
#include <pma/PolyAllocator.h>
#include <pma/utils/ManagedInstance.h>
#include <conformer/RigLogicFitting.h>
#include <rigmorpher/RigMorphModule.h>
#include <conformer/PcaFittingWrapper.h>
#include <nrr/rt/PCARigCreator.h>
#include <carbon/io/JsonIO.h>
#include <carbon/utils/FlattenJson.h>
#include <carbon/utils/Base64.h>

#include <cstring>
#include <filesystem>
#include <map>

namespace TITAN_API_NAMESPACE
{

struct ActorCreationAPI::Private
{
    std::unique_ptr<FaceFitting<float>, std::function<void(FaceFitting<float>*)>> faceFitting{};
    std::unique_ptr<IdentityModelFitting<float>, std::function<void(IdentityModelFitting<float>*)>> teethFitting{};
    std::unique_ptr<IdentityModelFitting<float>, std::function<void(IdentityModelFitting<float>*)>> leftEyeFitting {};
    std::unique_ptr<IdentityModelFitting<float>, std::function<void(IdentityModelFitting<float>*)>> rightEyeFitting {};
    std::unique_ptr<RigLogicFitting<float>, std::function<void(RigLogicFitting<float>*)>> rigLogicFitting{};
    std::unique_ptr<PcaRigFitting<float>, std::function<void(PcaRigFitting<float>*)>> pcaRigFitting{};
    std::unique_ptr<BrowLandmarksGenerator<float>, std::function<void(BrowLandmarksGenerator<float>*)>> browLandmarksGenerator{};
    std::unique_ptr<FittingInitializer<float>, std::function<void(FittingInitializer<float>*)>> fittingInitializer {};
    MultiCameraSetup<float> cameras;
    std::map<FittingMaskType, VertexWeights<float>> masks;
    std::vector<std::shared_ptr<FrameInputData>> frameData;
    std::unique_ptr<ReferenceAligner, std::function<void(ReferenceAligner*)>> referenceAligner{};
    std::vector<Affine<float, 3, 3>> currentToScanTransforms;
    std::vector<float> currentToScanScales;
    std::map<std::string, MeshLandmarks<float>> meshLandmarks;
    Affine<float, 3, 3> eyeLeftToHead, eyeRightToHead, teethToHead;
    bool fittingDataCollected = false;
    bool multiViewLandmarkMasking = false;
    bool eyeFittingInitialized = false;
    bool teethFittingInitialized = false;
    bool globalScaleCalculated = false;
    Mesh<float> faceTopology, teethTopology, eyeTopology;
    Eigen::Matrix<float, 3, -1> currentFaceVertices, currentTeethVertices, currentEyeLeftVertices, currentEyeRightVertices;
    std::map<std::string, float> landmarkAndCurveWeights;
    InputDataType dataType { InputDataType::NONE };
    ScanMaskType scanMaskType { ScanMaskType::GLOBAL };
    IdentityModelType identityModelType { IdentityModelType::FACE };
    Eigen::Vector3f teethMean;

    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> globalThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(
        /*createIfNotAvailable=*/true);

    void SetupDefaultLandmarkAndCurveWeights()
    {
        for (const auto& [meshName, meshSpecificLandmarks] : meshLandmarks)
        {
            for (const auto& [name, _] : meshSpecificLandmarks.LandmarksBarycentricCoordinates())
            {
                if (landmarkAndCurveWeights.find(name) == landmarkAndCurveWeights.end())
                {
                    landmarkAndCurveWeights[name] = 1.0f;
                }
            }
            for (const auto& [name, _] : meshSpecificLandmarks.MeshCurvesBarycentricCoordinates())
            {
                if (landmarkAndCurveWeights.find(name) == landmarkAndCurveWeights.end())
                {
                    landmarkAndCurveWeights[name] = 1.0f;
                }
            }
        }
    }

    void UpdateIndividualLandmarkWeights(const std::vector<std::vector<std::pair<LandmarkInstance<float, 2>, Camera<float>>>>& landmarks)
    {
        if (multiViewLandmarkMasking && (landmarks.size() > 0))
        {
            const auto weightsPerInstance = MaskLandmarksToAvoidAmbiguity(landmarkAndCurveWeights, landmarks);
            faceFitting->SetPerInstanceUserDefinedLandmarkAndCurveWeights(weightsPerInstance);

            if (teethFitting)
            {
                teethFitting->SetPerInstanceUserDefinedLandmarkAndCurveWeights(weightsPerInstance);
            }
        }
        else
        {
            faceFitting->SetGlobalUserDefinedLandmarkAndCurveWeights(landmarkAndCurveWeights);

            if (teethFitting)
            {
                teethFitting->SetGlobalUserDefinedLandmarkAndCurveWeights(landmarkAndCurveWeights);
            }
        }
    }

    void UpdateCurrentVerticesState()
    {
        const Eigen::Matrix3Xf vertices = faceFitting->CurrentDeformedVertices();
        if (identityModelType == IdentityModelType::COMBINED)
        {
            const auto splitedVertices = geoutils::SplitVertices(vertices, std::vector<int> { faceTopology.NumVertices(),
                                                                                              teethTopology.NumVertices(),
                                                                                              eyeTopology.NumVertices(),
                                                                                              eyeTopology.NumVertices() });
            currentFaceVertices = splitedVertices[0];
            currentTeethVertices = splitedVertices[1];
            currentEyeLeftVertices = splitedVertices[2];
            currentEyeRightVertices = splitedVertices[3];
        }
        else
        {
            currentFaceVertices = vertices;
        }
    }

    // helper method to enable working with scans of arbitrary scale
    bool CalculateRigToTargetDataScale()
    {
        std::vector<float> estimatedToRigScale(frameData.size());
        std::vector<MultiCameraSetup<float>> camerasPerFrame(frameData.size());

        // collect scans with original scale for scan to MH transform and scale calculation
        fittingInitializer->SetTargetMeshes(CollectMeshes(frameData, std::vector<float> {}).second);

        // same with landmarks - use unscaled cameras for projection
        fittingInitializer->SetTargetLandmarks(Collect2DLandmarks(frameData, cameras));

        // initially rigid + scale align using projected landmarks
        // scan is scaled, and transform is added to toScanTransform for simple implementation (not adding scale variable to FaceFitting)
        std::vector<Affine<float, 3, 3>> rigToScanTransform(frameData.size());

        // note that we are updating the object internals because it can be changed only in this place
        if (!fittingInitializer->InitializeFace(rigToScanTransform, currentToScanScales, faceFitting->CurrentMeshLandmarks(), /*withScale=*/true))
        {
            return false;
        }

        return true;
    }

    // helper method just to get the inverse (Target-2-Rig) scale for every frame
    std::vector<float> TargetDataToRigScale()
    {
        std::vector<float> scanToRigScales(currentToScanScales.size());
        for (int frameNum = 0; frameNum < int(frameData.size()); ++frameNum)
        {
            float scanToRigScale = 1.0f / currentToScanScales[frameNum];
            scanToRigScales[frameNum] = scanToRigScale;
        }

        return scanToRigScales;
    }

    // collect, order and set fitting data for every fitting class object
    bool CollectFittingData()
    {
        if (!fittingDataCollected)
        {
            const auto landmarks3d = Collect3DLandmarks(frameData);
            faceFitting->SetTarget3DLandmarks(landmarks3d);
            rigLogicFitting->SetTarget3DLandmarks(landmarks3d);
            teethFitting->SetTarget3DLandmarks(landmarks3d);
            leftEyeFitting->SetTarget3DLandmarks(landmarks3d);
            rightEyeFitting->SetTarget3DLandmarks(landmarks3d);
            if (dataType == InputDataType::DEPTHS)
            {
                const auto landmarks2d = Collect2DLandmarks(frameData, cameras);
                const auto [weights, depths] = CollectDepthmapsAsMeshes(frameData);
                faceFitting->SetTarget2DLandmarks(landmarks2d);
                faceFitting->SetTargetMeshes(depths, weights);
                rigLogicFitting->SetTarget2DLandmarks(landmarks2d);
                rigLogicFitting->SetTargetMeshes(depths, weights);
                teethFitting->SetTarget2DLandmarks(landmarks2d);
                teethFitting->SetTargetMeshes(depths, weights);
                leftEyeFitting->SetTarget2DLandmarks(landmarks2d);
                leftEyeFitting->SetTargetMeshes(depths, weights);
                rightEyeFitting->SetTarget2DLandmarks(landmarks2d);
                rightEyeFitting->SetTargetMeshes(depths, weights);
                pcaRigFitting->SetTarget2DLandmarks(landmarks2d);
                pcaRigFitting->SetTargetMeshes(depths, weights);
                browLandmarksGenerator->SetLandmarks(landmarks2d[0][0]);
                fittingInitializer->SetTargetLandmarks(landmarks2d);
                fittingInitializer->SetTargetMeshes(depths);
                UpdateIndividualLandmarkWeights(landmarks2d);
            }
            else if (dataType == InputDataType::SCAN)
            {
                // support arbitrary scan scale
                if (!globalScaleCalculated)
                {
                    // ensure that scale to the target data is calculated only once
                    globalScaleCalculated = true;
                    if (!CalculateRigToTargetDataScale())
                    {
                        return false;
                    }
                }
                const auto scanToRigScalesPerFrame = TargetDataToRigScale();
                std::vector<MultiCameraSetup<float>> camerasPerFrame = ScaledCamerasPerFrame(cameras, scanToRigScalesPerFrame);
                // collect scans, now with updated scale
                const auto [weights, meshes] = CollectMeshes(frameData, scanToRigScalesPerFrame);
                const auto landmarks2d = Collect2DLandmarks(frameData, camerasPerFrame);
                faceFitting->SetTarget2DLandmarks(landmarks2d);
                faceFitting->SetTargetMeshes(meshes, weights);
                rigLogicFitting->SetTarget2DLandmarks(landmarks2d);
                rigLogicFitting->SetTargetMeshes(meshes, weights);
                teethFitting->SetTarget2DLandmarks(landmarks2d);
                teethFitting->SetTargetMeshes(meshes, weights);
                leftEyeFitting->SetTarget2DLandmarks(landmarks2d);
                leftEyeFitting->SetTargetMeshes(meshes, weights);
                rightEyeFitting->SetTarget2DLandmarks(landmarks2d);
                rightEyeFitting->SetTargetMeshes(meshes, weights);
                pcaRigFitting->SetTarget2DLandmarks(landmarks2d);
                pcaRigFitting->SetTargetMeshes(meshes, weights);
                browLandmarksGenerator->SetLandmarks(landmarks2d[0][0]);

                // update fitting initializer state for further use with scaled scan and cameras
                fittingInitializer->SetTargetLandmarks(landmarks2d);
                fittingInitializer->SetTargetMeshes(meshes);

                UpdateIndividualLandmarkWeights(landmarks2d);
            }
            else
            {
                return false;
            }
        }
        fittingDataCollected = true;
        return true;
    }

    void SetData(const TemplateDescription& templateDescription, const TITAN_NAMESPACE::JsonElement& identityModelJson)
    {
        faceTopology = templateDescription.Topology();
        faceTopology.Triangulate();
        teethTopology = templateDescription.GetAssetTopology("teeth");
        teethTopology.Triangulate();
        eyeTopology = templateDescription.GetAssetTopology("eye");
        eyeTopology.Triangulate();
        meshLandmarks["head"] = templateDescription.GetMeshLandmarks();
        meshLandmarks["teeth"] = templateDescription.GetTeethMeshLandmarks();
        meshLandmarks["eye_left"] = templateDescription.GetEyeLeftMeshLandmarks();
        meshLandmarks["eye_right"] = templateDescription.GetEyeRightMeshLandmarks();

        browLandmarksGenerator = pma::UniqueInstance<BrowLandmarksGenerator<float>>::with(MEM_RESOURCE).create();
        browLandmarksGenerator->Init(templateDescription);

        masks[FittingMaskType::RIGID] = templateDescription.GetVertexWeights("nonrigid_mask");
        masks[FittingMaskType::NONRIGID] = templateDescription.GetVertexWeights("nonrigid_mask");
        masks[FittingMaskType::FINE] = templateDescription.GetVertexWeights("fine_mask");
        masks[FittingMaskType::TEETH] = templateDescription.GetAssetVertexWeights("teeth", "nonrigid_mask");
        masks[FittingMaskType::TEETH_HEAD_COLLISION_INTERFACE] = templateDescription.GetAssetVertexWeights("teeth", "head_collision_interface");
        masks[FittingMaskType::EYE] = templateDescription.GetAssetVertexWeights("eye", "nonrigid_mask");
        masks[FittingMaskType::EYE_INTERFACE_LEFT] = templateDescription.GetVertexWeights("eyeball_interface_left");
        masks[FittingMaskType::EYE_INTERFACE_RIGHT] = templateDescription.GetVertexWeights("eyeball_interface_right");
        masks[FittingMaskType::MOUTH_SOCKET] = templateDescription.GetVertexWeights("mouth_socket");
        if (templateDescription.HasVertexWeights("smile_stab"))
        {
            masks[FittingMaskType::STABILIZATION] = templateDescription.GetVertexWeights("smile_stab");
        }
        else
        {
            VertexWeights<float> w(Eigen::VectorXf::Zero(faceTopology.NumVertices()));
            masks[FittingMaskType::STABILIZATION] = w;
        }


        faceFitting = pma::UniqueInstance<FaceFitting<float>>::with(MEM_RESOURCE).create();

        faceFitting->LoadModel(TITAN_NAMESPACE::Base64Decode(identityModelJson["pca_identity_model"].String()));

        // check face identity model type
        if (faceFitting->CurrentDeformedVertices().cols() != faceTopology.NumVertices())
        {
            identityModelType = IdentityModelType::COMBINED;

            const auto [_, combinedMesh] = geoutils::CombineMeshes<float>(std::vector<Mesh<float>> { faceTopology, teethTopology, eyeTopology, eyeTopology });
            faceFitting->SetTopology(combinedMesh);

            std::vector<FittingMaskType> maskIdsToSkip = { FittingMaskType::EYE, FittingMaskType::TEETH };
            for (auto [id, currentWeights] : masks)
            {
                if (std::find(maskIdsToSkip.begin(), maskIdsToSkip.end(), id) != maskIdsToSkip.end())
                {
                    continue;
                }
                Eigen::VectorXf currWeightsVector = currentWeights.Weights();
                Eigen::VectorXf newWeightsVector = Eigen::VectorXf::Zero(combinedMesh.NumVertices());
                newWeightsVector.head(currWeightsVector.size()) = currWeightsVector;

                VertexWeights<float> newWeights(newWeightsVector);
                masks[id] = newWeights;
            }
        }
        else
        {
            identityModelType = IdentityModelType::FACE;
            faceFitting->SetTopology(faceTopology);
        }

        UpdateCurrentVerticesState();
        faceFitting->SetInnerLipInterfaceVertices(templateDescription.GetVertexWeights("lip_collision_upper"),
                                                  templateDescription.GetVertexWeights("lip_collision_lower"));
        faceFitting->SetMeshLandmarks(meshLandmarks["head"]);
        faceFitting->SetEyeballMesh(eyeTopology);
        faceFitting->SetEyeConstraintVertexWeights(masks[FittingMaskType::EYE_INTERFACE_LEFT], masks[FittingMaskType::EYE_INTERFACE_RIGHT]);

        // set default parameters for scan
        faceFitting->ModelRegistrationConfiguration()["minimumDistanceThreshold"].Set(10.f);
        faceFitting->FineRegistrationConfiguration()["minimumDistanceThreshold"].Set(10.f);
        faceFitting->FineRegistrationConfiguration()["vertexOffsetRegularization"].Set(0.01f);
        faceFitting->FineRegistrationConfiguration()["vertexLaplacian"].Set(1.0f);
        faceFitting->FineRegistrationConfiguration()["collisionWeight"].Set(0.1f);

        teethFitting = pma::UniqueInstance<IdentityModelFitting<float>>::with(MEM_RESOURCE).create();
        teethFitting->SetSourceMesh(teethTopology);
        teethFitting->LoadModel(TITAN_NAMESPACE::Base64Decode(identityModelJson["assets_identity_models"]["teeth"].String()));
        teethFitting->SetMeshLandmarks(meshLandmarks["teeth"]);
        teethFitting->ModelRegistrationConfiguration()["optimizeScale"].Set(false);

        leftEyeFitting = pma::UniqueInstance<IdentityModelFitting<float>>::with(MEM_RESOURCE).create();
        leftEyeFitting->SetSourceMesh(eyeTopology);
        leftEyeFitting->LoadModel(TITAN_NAMESPACE::Base64Decode(identityModelJson["assets_identity_models"]["eye_left"].String()));
        leftEyeFitting->SetMeshLandmarks(meshLandmarks["eye_left"]);
        leftEyeFitting->ModelRegistrationConfiguration()["optimizeScale"].Set(false);
        leftEyeFitting->ModelRegistrationConfiguration()["geometryWeight"].Set(20.f);

        rightEyeFitting = pma::UniqueInstance<IdentityModelFitting<float>>::with(MEM_RESOURCE).create();
        rightEyeFitting->SetSourceMesh(eyeTopology);
        rightEyeFitting->LoadModel(TITAN_NAMESPACE::Base64Decode(identityModelJson["assets_identity_models"]["eye_right"].String()));
        rightEyeFitting->SetMeshLandmarks(meshLandmarks["eye_right"]);
        rightEyeFitting->ModelRegistrationConfiguration()["optimizeScale"].Set(false);
        rightEyeFitting->ModelRegistrationConfiguration()["geometryWeight"].Set(20.f);

        rigLogicFitting = pma::UniqueInstance<RigLogicFitting<float>>::with(MEM_RESOURCE).create();
        rigLogicFitting->SetMeshLandmarks(meshLandmarks["head"]);

        pcaRigFitting = pma::UniqueInstance<PcaRigFitting<float>>::with(MEM_RESOURCE).create();
        pcaRigFitting->SetMeshLandmarks(meshLandmarks["head"],
                                        meshLandmarks["teeth"],
                                        meshLandmarks["eye_left"],
                                        meshLandmarks["eye_right"]);
        pcaRigFitting->SetTopology(faceTopology);

        fittingInitializer = pma::UniqueInstance<FittingInitializer<float>>::with(MEM_RESOURCE).create();

        SetupDefaultLandmarkAndCurveWeights();

        if (meshLandmarks["head"].HasLandmark("pt_frankfurt_fr") && meshLandmarks["head"].HasLandmark("pt_frankfurt_rr") &&
            meshLandmarks["head"].HasLandmark("pt_frankfurt_fl") &&
            meshLandmarks["head"].HasLandmark("pt_frankfurt_rl"))
        {
            const BarycentricCoordinates<float> fr = meshLandmarks["head"].LandmarksBarycentricCoordinates().find("pt_frankfurt_fr")->second;
            const BarycentricCoordinates<float> rr = meshLandmarks["head"].LandmarksBarycentricCoordinates().find("pt_frankfurt_rr")->second;
            const BarycentricCoordinates<float> fl = meshLandmarks["head"].LandmarksBarycentricCoordinates().find("pt_frankfurt_fl")->second;
            const BarycentricCoordinates<float> rl = meshLandmarks["head"].LandmarksBarycentricCoordinates().find("pt_frankfurt_rl")->second;

            referenceAligner = pma::UniqueInstance<ReferenceAligner>::with(MEM_RESOURCE).create(faceTopology, fr, rr, fl, rl);
        }
    }
};


ActorCreationAPI::ActorCreationAPI()
    : m(new Private())
{}

ActorCreationAPI::~ActorCreationAPI()
{
    delete m;
}

bool ActorCreationAPI::Init(const char* InTemplateDescriptionJson, const char* InIdentityModelJson)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InTemplateDescriptionJson, false, "template description json is not valid");
        TITAN_CHECK_OR_RETURN(InIdentityModelJson, false, "identity model json is not valid");

        TemplateDescription templateDescription;

        TITAN_CHECK_OR_RETURN(templateDescription.Load(InTemplateDescriptionJson),
                        false,
                        "failed to load template description");

        TITAN_CHECK_OR_RETURN(InIdentityModelJson,
                        false,
                        "identity model json must be set");

        TITAN_NAMESPACE::JsonElement json = TITAN_NAMESPACE::ReadJson(InIdentityModelJson);
        m->SetData(templateDescription, json);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to initialize: {}", e.what());
    }
}

bool ActorCreationAPI::Init(const std::string& InConfigurationDirectory)
{
    try
    {
        TITAN_RESET_ERROR;

        TemplateDescription templateDescription;

        TITAN_CHECK_OR_RETURN(templateDescription.Load(InConfigurationDirectory + "/template_description.json"),
                        false,
                        "failed to load template description");

        m->SetData(templateDescription, TITAN_NAMESPACE::FlattenJson(InConfigurationDirectory + "/dna_database_description.json"));

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to initialize: {}", e.what());
    }
}

bool ActorCreationAPI::SaveDebuggingData(const std::string& InDebugDataDirectory)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->CollectFittingData(), false, "face fitting data has not been collected");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");
        TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS || m->dataType == InputDataType::SCAN, false,
                        "no input data set");

        if (m->dataType == InputDataType::SCAN)
        {
            // for scans, we expect a single frame, we save different json for each camera, and set the frame number to 0
            // check that each frame has one camera
            TITAN_CHECK_OR_RETURN(m->frameData.size() == 1, false, "expecting one frame");

            for (const auto& camera : m->cameras.GetCameras())
            {
                LandmarkSequence<float> landmarks;
                std::map<int, std::shared_ptr<const LandmarkInstance<float, 2>>> landmarkInstances;

                const auto& landmarksPerCamera = m->frameData[0]->LandmarksPerCamera();
                if (landmarksPerCamera.find(camera.first) != landmarksPerCamera.end())
                {
                    const auto& cameraLandmarks = landmarksPerCamera.at(camera.first);
                    std::shared_ptr<LandmarkInstance<float, 2>> curPtr = std::make_shared<LandmarkInstance<float, 2>>();
                    *curPtr = *cameraLandmarks;
                    curPtr->SetFrameNumber(0);
                    landmarkInstances[0] = curPtr;
                    // we do not need to re-distort the landmarks for the mesh 2 metahuman case as it is a pinhole camera
                }

                if (!landmarkInstances.empty())
                {
                    landmarks.SetLandmarkInstances(landmarkInstances);
                    std::filesystem::path landmarksJsonPath(InDebugDataDirectory);
                    landmarksJsonPath /= std::string("identity_creation_landmarks_" + camera.first + ".json");
                    landmarks.Save(landmarksJsonPath.string());
                }
            }
        }
        else if (m->dataType == InputDataType::DEPTHS) //-V547
        {
            // for depths, we need to save the same camera for each frame. When the API is called in UE,
            // a different camera name is used for each frame (which is incorrect). As a work-around, just use
            // the camera name for the first frame and use this for all frames and log a Warning. We save one json file only
            // and expect one camera only per frame.
            LandmarkSequence<float> landmarks;
            std::string cameraName;
            bool bFirstFrame = true;
            std::map<int, std::shared_ptr<const LandmarkInstance<float, 2>>> landmarkInstances;
            int frameNumberCounter = 0;
            for (const auto& frame : m->frameData)
            {
                const auto& landmarksPerCamera = frame->LandmarksPerCamera();
                TITAN_CHECK_OR_RETURN(landmarksPerCamera.size() == 1, false, "expecting one camera only per frame");
                if (bFirstFrame)
                {
                    bFirstFrame = false;
                    cameraName = landmarksPerCamera.begin()->first;
                }
                if (landmarksPerCamera.begin()->first != cameraName)
                {
                    LOG_INFO("Warning: expecting camera name to be the same for each depth frame; assuming the first camera should be used for all frames.");
                }
                const auto& cameraLandmarks = landmarksPerCamera.begin()->second;
                std::shared_ptr<LandmarkInstance<float, 2>> curPtr = std::make_shared<LandmarkInstance<float, 2>>();
                *curPtr = *cameraLandmarks;
                curPtr->SetFrameNumber(frameNumberCounter);
                landmarkInstances[frameNumberCounter] = curPtr;
                // we need to re-distort the debug landmarks as they have been undistorted by the Actor Creation API
                const auto& camera = m->cameras.GetCameras().at(cameraName);
                for (int i = 0; i < curPtr->NumLandmarks(); ++i)
                {
                    const Eigen::Vector2f pix = camera.Distort(curPtr->Points().col(i));
                    curPtr->SetLandmark(i, pix, curPtr->Confidence()[i]);
                }
                landmarkInstances[frameNumberCounter] = curPtr;
                frameNumberCounter++;
            }

            landmarks.SetLandmarkInstances(landmarkInstances);
            std::filesystem::path landmarksJsonPath(InDebugDataDirectory);
            landmarksJsonPath /= std::string("identity_creation_landmarks_" + cameraName + ".json");
            landmarks.Save(landmarksJsonPath.string());
        }

        // save the camera calibrations (this is the same for scan and depth-maps, although for depthmaps all but one of the cameras will be ignored)
        std::filesystem::path camerasJsonPath(InDebugDataDirectory);
        camerasJsonPath /= "identity_creation_calib.json";
        TITAN_CHECK_OR_RETURN(WriteMetaShapeCamerasToJsonFile(camerasJsonPath.generic_string(), m->cameras.GetCamerasAsVector()),
                        false,
                        "failed to save cameras");

        // save out the depthmaps or scan as .objs
        // we save each depthmap as a separate frame
        ObjFileWriter<float> writer;
        if (m->dataType == InputDataType::SCAN)
        {
            std::filesystem::path scanObjPath = InDebugDataDirectory;
            scanObjPath /= "scan.obj";

            if (!m->frameData.empty())
            {
                const auto& frame = m->frameData.front();
                const auto& scan = frame->Scan();
                writer.writeObj(*scan.mesh, scanObjPath.string());
            }
        }
        else if (m->dataType == InputDataType::DEPTHS)
        {
            const auto [weights, depths] = CollectDepthmapsAsMeshes(m->frameData);
            std::vector<std::string> cameraNames;
            for (auto& frame : m->frameData)
            {
                for (const auto& [cameraName, depthAsMeshData] : frame->DepthmapsAsMeshes())
                {
                    cameraNames.emplace_back(cameraName);
                }
            }
            unsigned counter = 0;
            std::filesystem::path depthObjFolder(InDebugDataDirectory);
            depthObjFolder /= "depth";
            if (!std::filesystem::exists(depthObjFolder))
            {
                std::filesystem::create_directory(depthObjFolder);
            }
            for (const auto& mesh : depths)
            {
                char buffer[80];
                snprintf(buffer, sizeof(buffer), "%06d.obj", static_cast<int>(counter));
                std::filesystem::path depthObjPath = depthObjFolder / buffer;
                writer.writeObj(*mesh, depthObjPath.string());
                counter++;
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to save debugging data: {}",
                         e.what());
    }
}

bool ActorCreationAPI::SetCameras(const std::map<std::string, OpenCVCamera>& InCameras)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->rigLogicFitting, false, "riglogic fitting has not been initialized");

        std::vector<MetaShapeCamera<float>> metaCameras;
        for (const auto& [cameraName, opencvCamera] : InCameras)
        {
            metaCameras.emplace_back(OpenCVCamera2MetaShapeCamera<float>(cameraName.c_str(), opencvCamera));
        }
        MultiCameraSetup<float> cameraSetup;
        cameraSetup.Init(metaCameras);

        m->cameras = cameraSetup;

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to set cameras: {}",
                         e.what());
    }
}

bool ActorCreationAPI::SetDepthInputData(const std::map<std::string, const std::map<std::string, FaceTrackingLandmarkData>>& InLandmarksDataPerCamera,
                                         const std::map<std::string, const float*>& InDepthMaps)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->rigLogicFitting, false, "riglogic fitting has not been initialized");
        if (!m->frameData.empty())
        {
            TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS, false, "data buffer already contains non-depth frames");
        }
        else
        {
            m->dataType = InputDataType::DEPTHS;
        }

        const MultiCameraSetup<float>& cameraSetup = m->cameras;

        std::map<std::string, std::shared_ptr<const LandmarkInstance<float, 2>>> landmarksPerCamera;
        std::map<std::string, GeometryData> depthPerCamera;

        // convert landmark input
        for (const auto& [cameraName, perCameraLandmarkData] : InLandmarksDataPerCamera)
        {
            TITAN_CHECK_OR_RETURN(cameraSetup.HasCamera(cameraName), false, "no camera {}", cameraName);
            landmarksPerCamera[cameraName] = CreateLandmarkInstanceForCamera(perCameraLandmarkData,
                                                                             std::map<std::string, std::vector<std::string>>{},
                                                                             cameraSetup.GetCamera(
                                                                                 cameraName));
        }

        for (const auto& [cameraName, depthMap] : InDepthMaps)
        {
            const auto camera = cameraSetup.GetCamera(cameraName);
            const auto depthAsMesh = geoutils::ConstructMeshFromDepthStream<float>(camera, depthMap, /*distThresh=*/80.0f);
            bool bInvalidMeshTopology = true;
            const auto vertexWeights = geoutils::CalculateMaskBasedOnMeshTopology<float>(depthAsMesh, bInvalidMeshTopology);
            TITAN_CHECK_OR_RETURN(!bInvalidMeshTopology, false, "All vertices on the input mesh marked as invalid.Please check input mesh topology.");
            pma::PolyAllocator<Mesh<float>> geometryDataPolyAllocator { MEM_RESOURCE };

            depthPerCamera[cameraName] = { std::allocate_shared<Mesh<float>>(geometryDataPolyAllocator, depthAsMesh), vertexWeights };
        }
        pma::PolyAllocator<FrameInputData> framePolyAllocator{ MEM_RESOURCE };

        m->frameData.push_back(std::allocate_shared<FrameInputData>(framePolyAllocator, landmarksPerCamera, depthPerCamera));
        m->currentToScanTransforms.resize(/*numOfInputFrames=*/m->frameData.size());
        m->currentToScanScales.resize(/*numOfInputFrames=*/m->frameData.size());
        std::fill(m->currentToScanTransforms.begin(), m->currentToScanTransforms.end(), Affine<float, 3, 3>{});
        std::fill(m->currentToScanScales.begin(), m->currentToScanScales.end(), 1.0f);
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to Set input data: {}",
                         e.what());
    }
}

bool ActorCreationAPI::SetScanInputData(const std::map<std::string, const FaceTrackingLandmarkData>& In3dLandmarksData,
                                        const std::map<std::string,
                                                       const std::map<std::string, FaceTrackingLandmarkData>>& In2dLandmarksData,
                                        const MeshInputData& InScanData,
                                        bool& bOutInvalidMeshTopology)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->rigLogicFitting, false, "riglogic fitting has not been initialized");
        if (!m->frameData.empty())
        {
            LOG_WARNING("data stream is not empty. Cleared to store scan data.");
            TITAN_CHECK_OR_RETURN(ResetInputData(), false, "failed to reset input data.");
        }

        bOutInvalidMeshTopology = false;
        // convert landmark input
        auto landmarksIn3D = Create3dLandmarkInstance(In3dLandmarksData, std::map<std::string, std::vector<std::string>>{});

        const auto& [numTriangles, triangles, numVertices, vertices] = InScanData;
        const auto scanMesh = geoutils::ConstructMeshFromMeshStream<float>(numTriangles, triangles, numVertices, vertices);
        const auto vertexWeights = geoutils::CalculateMaskBasedOnMeshTopology<float>(scanMesh, bOutInvalidMeshTopology);

        pma::PolyAllocator<Mesh<float>> geometryDataPolyAllocator { MEM_RESOURCE };
        GeometryData scanData = { std::allocate_shared<Mesh<float>>(geometryDataPolyAllocator, scanMesh), vertexWeights };

        std::map<std::string, std::shared_ptr<const LandmarkInstance<float, 2>>> landmarksPerCamera;
        const MultiCameraSetup<float>& cameraSetup = m->cameras;

        // convert landmark input
        for (const auto& [cameraName, perCameraLandmarkData] : In2dLandmarksData)
        {
            TITAN_CHECK_OR_RETURN(cameraSetup.HasCamera(cameraName), false, "no camera {}", cameraName);
            landmarksPerCamera[cameraName] = CreateLandmarkInstanceForCamera(perCameraLandmarkData,
                                                                             std::map<std::string, std::vector<std::string>>{},
                                                                             cameraSetup.GetCamera(cameraName));
        }

        pma::PolyAllocator<FrameInputData> framePolyAllocator{ MEM_RESOURCE };
        m->frameData.push_back(std::allocate_shared<FrameInputData>(framePolyAllocator, landmarksPerCamera, landmarksIn3D, scanData));
        m->dataType = InputDataType::SCAN;
        m->currentToScanTransforms.resize(/*numOfInputFrames=*/m->frameData.size());
        m->currentToScanScales.resize(/*numOfInputFrames=*/m->frameData.size());
        std::fill(m->currentToScanTransforms.begin(), m->currentToScanTransforms.end(), Affine<float, 3, 3>{});
        std::fill(m->currentToScanScales.begin(), m->currentToScanScales.end(), 1.0f);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to Set input data: {}", e.what());
    }
}

bool ActorCreationAPI::CalculateAndUpdateScanMask(const std::string& InCameraName, ScanMaskType InScanMaskType)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");

        if (InScanMaskType == m->scanMaskType)
        {
            return true;
        }
        else
        {
            m->scanMaskType = InScanMaskType;
            m->fittingDataCollected = false;
            if (InScanMaskType == ScanMaskType::GLOBAL)
            {
                for (auto frame : m->frameData)
                {
                    bool bInvalidMeshTopology = true;
                    if (m->dataType == InputDataType::SCAN)
                    {
                        const auto currentScan = frame->Scan().mesh;
                        const auto newWeights = geoutils::CalculateMaskBasedOnMeshTopology(*currentScan, bInvalidMeshTopology);
                        frame->UpdateScanMask(newWeights);
                    }
                    else
                    {
                        const auto currentDepthsPerCamera = frame->DepthmapsAsMeshes();
                        std::map<std::string, Eigen::VectorXf> newWeightsPerCamera;
                        for (const auto& [cameraName, geometryData] : currentDepthsPerCamera)
                        {
                            newWeightsPerCamera[cameraName] = geoutils::CalculateMaskBasedOnMeshTopology(*geometryData.mesh, bInvalidMeshTopology);
                        }
                        frame->UpdateDepthmapsMask(newWeightsPerCamera);
                    }
                    TITAN_CHECK_OR_RETURN(!bInvalidMeshTopology, false, "All vertices on the input mesh marked as invalid.Please check input mesh topology.");
                }
            }
            else if (InScanMaskType == ScanMaskType::EYE_FITTING)
            {
                const std::string eyelidLowerLeftName = "crv_eyelid_lower_l";
                const std::string eyelidUpperLeftName = "crv_eyelid_upper_l";
                const std::string eyelidLowerRightName = "crv_eyelid_lower_r";
                const std::string eyelifUpperRightName = "crv_eyelid_upper_r";
                const std::string irisLeftName = "crv_iris_l";
                const std::string irisRightName = "crv_iris_r";

                for (auto frame : m->frameData)
                {
                    const auto landmarksForCamera = Extract2DLandmarksForCamera(frame, m->cameras, InCameraName).first;
                    const auto landmarkConfig = landmarksForCamera.GetLandmarkConfiguration();
                    const auto camera = Extract2DLandmarksForCamera(frame, m->cameras, InCameraName).second;
                    TITAN_CHECK_OR_RETURN(landmarkConfig->HasCurve(eyelidLowerLeftName), false, "landmarks missing {}", eyelidLowerLeftName);
                    TITAN_CHECK_OR_RETURN(landmarkConfig->HasCurve(eyelidUpperLeftName), false, "landmarks missing {}", eyelidUpperLeftName);
                    TITAN_CHECK_OR_RETURN(landmarkConfig->HasCurve(eyelidLowerRightName), false, "landmarks missing {}", eyelidLowerRightName);
                    TITAN_CHECK_OR_RETURN(landmarkConfig->HasCurve(eyelifUpperRightName), false, "landmarks missing {}", eyelifUpperRightName);
                    TITAN_CHECK_OR_RETURN(landmarkConfig->HasCurve(irisLeftName), false, "landmarks missing {}", irisLeftName);
                    TITAN_CHECK_OR_RETURN(landmarkConfig->HasCurve(irisRightName), false, "landmarks missing {}", irisRightName);

                    const Eigen::Matrix<float, 2, -1> crvLeftLower = landmarksForCamera.Points(landmarkConfig->IndicesForCurve(eyelidLowerLeftName));
                    const Eigen::Matrix<float, 2, -1> crvLeftUpper = landmarksForCamera.Points(landmarkConfig->IndicesForCurve(eyelidUpperLeftName));

                    const Eigen::Matrix<float, 2, -1> crvRightLower = landmarksForCamera.Points(landmarkConfig->IndicesForCurve(eyelidLowerRightName));
                    const Eigen::Matrix<float, 2, -1> crvRightUpper = landmarksForCamera.Points(landmarkConfig->IndicesForCurve(eyelifUpperRightName));

                    const Eigen::Matrix<float, 2, -1> irisLeft = landmarksForCamera.Points(landmarkConfig->IndicesForCurve(irisLeftName));
                    const Eigen::Matrix<float, 2, -1> irisRight = landmarksForCamera.Points(landmarkConfig->IndicesForCurve(irisRightName));

                    if (m->dataType == InputDataType::SCAN)
                    {
                        const auto currentScan = frame->Scan();
                        Eigen::VectorXf newWeights = Eigen::VectorXf::Zero(currentScan.weights.size());
                        bool leftSuccess = EyeFittingHelper<float>::UpdateScanMaskBasedOnLandmarks(crvRightLower,
                                                                                                   crvRightUpper,
                                                                                                   irisRight,
                                                                                                   camera,
                                                                                                   currentScan.mesh,
                                                                                                   newWeights);
                        bool rightSuccess = EyeFittingHelper<float>::UpdateScanMaskBasedOnLandmarks(crvLeftLower,
                                                                                                    crvLeftUpper,
                                                                                                    irisLeft,
                                                                                                    camera,
                                                                                                    currentScan.mesh,
                                                                                                    newWeights);
                        if (leftSuccess && rightSuccess)
                        {
                            frame->UpdateScanMask(newWeights);
                        }
                        else
                        {
                            LOG_WARNING("Eye mask not calculated, using default.");
                        }
                    }
                    else
                    {
                        const auto currentDepthsPerCamera = frame->DepthmapsAsMeshes();
                        std::map<std::string, Eigen::VectorXf> newWeightsPerCamera;
                        for (const auto& [cameraName, geometryData] : currentDepthsPerCamera)
                        {
                            Eigen::VectorXf newWeights = geometryData.weights;
                            if (cameraName == InCameraName)
                            {
                                newWeights.setZero();
                                bool leftSuccess = EyeFittingHelper<float>::UpdateScanMaskBasedOnLandmarks(crvRightLower,
                                                                                                           crvRightUpper,
                                                                                                           irisRight,
                                                                                                           camera,
                                                                                                           geometryData.mesh,
                                                                                                           newWeights);
                                bool rightSuccess = EyeFittingHelper<float>::UpdateScanMaskBasedOnLandmarks(crvLeftLower,
                                                                                                            crvLeftUpper,
                                                                                                            irisLeft,
                                                                                                            camera,
                                                                                                            geometryData.mesh,
                                                                                                            newWeights);
                                if (!leftSuccess || !rightSuccess)
                                {
                                    newWeights = geometryData.weights;
                                    LOG_WARNING("Eye mask not calculated, using default.");
                                }
                            }
                            newWeightsPerCamera[cameraName] = newWeights;
                        }
                        frame->UpdateDepthmapsMask(newWeightsPerCamera);
                    }
                }
            }
            else
            {
                return false;
            }
        }
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to set/calculate scan mask: {}", e.what());
    }
}

bool ActorCreationAPI::ResetInputData()
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        m->frameData.clear();
        m->dataType = InputDataType::NONE;
        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to reset input data: {}", e.what());
    }
}

bool ActorCreationAPI::FitRigid(float* OutVertexPositions,
                                float* OutStackedToScanTransforms,
                                float* OutStackedToScanScales,
                                int32_t numIters,
                                const bool InAutoMode)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->CollectFittingData(), false, "face fitting data has not been collected");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");
        TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS || m->dataType == InputDataType::SCAN, false,
                        "no input data set");

        // init rigid
        std::vector<float> localScaleVar(m->frameData.size());
        bool initSuccess = m->fittingInitializer->InitializeFace(m->currentToScanTransforms,
                                                                 localScaleVar,
                                                                 m->faceFitting->CurrentMeshLandmarks(),
                                                                 /*withScale=*/false);
        TITAN_CHECK_OR_RETURN(initSuccess, false, "failed to initialize rigid fit");

        // fit rigid
        if (InAutoMode)
        {
            m->faceFitting->RigidRegistrationConfiguration()["useDistanceThreshold"].Set(false);
            m->currentToScanTransforms =
                m->faceFitting->RegisterRigid(m->currentToScanTransforms, m->masks[FittingMaskType::RIGID], /*iter=*/10);
            m->faceFitting->RigidRegistrationConfiguration()["useDistanceThreshold"].Set(true);
            m->currentToScanTransforms =
                m->faceFitting->RegisterRigid(m->currentToScanTransforms, m->masks[FittingMaskType::RIGID], /*iter=*/10);
        }
        else
        {
            m->currentToScanTransforms =
                m->faceFitting->RegisterRigid(m->currentToScanTransforms, m->masks[FittingMaskType::RIGID], /*iter=*/numIters);
        }
        m->UpdateCurrentVerticesState();

        memcpy(OutStackedToScanTransforms,
               m->currentToScanTransforms.data(),
               int32_t(m->frameData.size()) *
               int32_t(16) *
               sizeof(float));

        memcpy(OutStackedToScanScales,
               m->currentToScanScales.data(),
               int32_t(m->frameData.size()) *
               sizeof(float));

        memcpy(OutVertexPositions,
               m->currentFaceVertices.data(),
               int32_t(m->currentFaceVertices.cols() * m->currentFaceVertices.rows()) *
               sizeof(float));

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to rigid align: {}", e.what());
    }
}

bool ActorCreationAPI::FitNonRigid(float* OutVertexPositions,
                                   float* OutStackedToScanTransforms,
                                   float* OutStackedToScanScales,
                                   int32_t numIters,
                                   const bool autoMode)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->CollectFittingData(), false, "face fitting data has not been collected");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");
        TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS || m->dataType == InputDataType::SCAN, false,
                        "no input data set");

        // fit non-rigid
        if (autoMode)
        {
            for (float modelRegularization : { 10.0f, 1.0f, 0.1f })
            {
                m->faceFitting->ModelRegistrationConfiguration()["modelRegularization"].Set(modelRegularization);
                m->currentToScanTransforms =
                    m->faceFitting->RegisterNonRigid(m->currentToScanTransforms, m->masks[FittingMaskType::NONRIGID], /*iter=*/5);
            }
        }
        else
        {
            m->currentToScanTransforms =
                m->faceFitting->RegisterNonRigid(m->currentToScanTransforms, m->masks[FittingMaskType::NONRIGID], /*iter=*/numIters);
        }
        m->UpdateCurrentVerticesState();

        // ensures proper interface between head and eye meshes if eye fitting not enabled, but at a cost of not fully conforming to the input data
        if (m->identityModelType == IdentityModelType::COMBINED)
        {
            m->faceFitting->SetupEyeballConstraint(m->currentEyeLeftVertices, m->currentEyeRightVertices);
        }

        memcpy(OutStackedToScanTransforms,
               m->currentToScanTransforms.data(),
               int32_t(m->frameData.size()) *
               int32_t(16) *
               sizeof(float));

        memcpy(OutStackedToScanScales,
               m->currentToScanScales.data(),
               int32_t(m->frameData.size()) *
               sizeof(float));

        memcpy(OutVertexPositions,
               m->currentFaceVertices.data(),
               int32_t(m->currentFaceVertices.cols() * m->currentFaceVertices.rows()) *
               sizeof(float));

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to non-rigid align: {}", e.what());
    }
}

bool ActorCreationAPI::FitPerVertex(float* OutVertexPositions,
                                    float* OutStackedToScanTransforms,
                                    float* OutStackedToScanScales,
                                    int32_t numIters,
                                    const std::string& InDebugDataDirectory)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->CollectFittingData(), false, "face fitting data has not been collected");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");
        TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS || m->dataType == InputDataType::SCAN, false,
                        "no input data set");

        // fit per-vertex
        m->currentToScanTransforms =
            m->faceFitting->RegisterFine(m->currentToScanTransforms, m->masks[FittingMaskType::FINE], /*iter=*/numIters);

        m->UpdateCurrentVerticesState();

        memcpy(OutStackedToScanTransforms,
               m->currentToScanTransforms.data(),
               int32_t(m->frameData.size()) *
               int32_t(16) *
               sizeof(float));

        memcpy(OutStackedToScanScales,
               m->currentToScanScales.data(),
               int32_t(m->frameData.size()) *
               sizeof(float));

        memcpy(OutVertexPositions,
               m->currentFaceVertices.data(),
               int32_t(m->currentFaceVertices.cols() * m->currentFaceVertices.rows()) *
               sizeof(float));

        // save the conformed head mesh if we are saving Debug info
        if (!InDebugDataDirectory.empty())
        {
            ObjFileWriter<float> writer;
            std::filesystem::path headMeshObjPath = InDebugDataDirectory;
            headMeshObjPath /= "face_conformed.obj";
            m->faceTopology.SetVertices(m->currentFaceVertices);
            writer.writeObj(m->faceTopology, headMeshObjPath.string());
        }

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to per-vertex align: {}", e.what());
    }
}

bool ActorCreationAPI::FitRigLogic(dna::Reader* InDnaStream,
                                   float* OutVertexPositions,
                                   float* OutStackedToScanTransforms,
                                   float* OutStackedToScanScales,
                                   int32_t numIters)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InDnaStream, false, "input dna stream not valid");
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->rigLogicFitting, false, "riglogic fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->CollectFittingData(), false, "face fitting data has not been collected");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");
        TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS || m->dataType == InputDataType::SCAN, false,
                        "no input data set");

        m->rigLogicFitting->LoadRig(InDnaStream);

        // fit RigLogic
        m->currentToScanTransforms =
            m->rigLogicFitting->RegisterRigLogic(m->currentToScanTransforms, m->masks[FittingMaskType::NONRIGID], /*iter=*/numIters);
        m->currentFaceVertices = m->rigLogicFitting->CurrentVertices(/*meshId=*/0);
        m->currentTeethVertices = m->rigLogicFitting->CurrentVertices(/*meshId=*/1);
        m->currentEyeLeftVertices = m->rigLogicFitting->CurrentVertices(/*meshId=*/3);
        m->currentEyeRightVertices = m->rigLogicFitting->CurrentVertices(/*meshId=*/4);

        memcpy(OutStackedToScanTransforms,
               m->currentToScanTransforms.data(),
               int32_t(m->frameData.size()) *
               int32_t(16) *
               sizeof(float));

        memcpy(OutStackedToScanScales,
               m->currentToScanScales.data(),
               int32_t(m->frameData.size()) *
               sizeof(float));

        memcpy(OutVertexPositions,
               m->currentFaceVertices.data(),
               int32_t(m->currentFaceVertices.cols() * m->currentFaceVertices.rows()) *
               sizeof(float));

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to fit rig logic: {}", e.what());
    }
}

bool ActorCreationAPI::FitPcaRig(dna::Reader* InDnaStream,
                                 float* OutVertexPositions,
                                 float* OutStackedToScanTransforms,
                                 float* OutStackedToScanScales,
                                 const float* OptionalInNeutralVertexPositions,
                                 int32_t numIters,
                                 const std::string& InDebugDataDirectory)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InDnaStream, false, "input dna stream not valid");
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->pcaRigFitting, false, "pca rig fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->CollectFittingData(), false, "face fitting data has not been collected");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");
        TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS || m->dataType == InputDataType::SCAN, false,
                        "no input data set");

        m->pcaRigFitting->LoadRig(InDnaStream);

        // fit pca rig
        m->currentToScanTransforms =
            m->pcaRigFitting->RegisterPcaRig(m->currentToScanTransforms, m->masks[FittingMaskType::NONRIGID], VertexWeights<float>(), /*iter=*/numIters);
        auto neutralVertices = m->currentFaceVertices;

        if (OptionalInNeutralVertexPositions)
        {
            neutralVertices = Eigen::Map<const Eigen::Matrix<float, 3, -1, Eigen::ColMajor>>(
                (const float*)OptionalInNeutralVertexPositions,
                3,
                m->faceTopology.NumVertices())
                .template cast<float>();

            std::vector<int> pointsToInclude = m->masks[FittingMaskType::STABILIZATION].NonzeroVertices();
            const auto exprVertices = m->pcaRigFitting->CurrentVertices(/*meshId=*/0);

            Eigen::Matrix3Xf src = Eigen::Matrix3Xf::Zero(3, pointsToInclude.size());
            Eigen::Matrix3Xf tgt = Eigen::Matrix3Xf::Zero(3, pointsToInclude.size());

            for (size_t i = 0; i < pointsToInclude.size(); ++i)
            {
                tgt.col(i) = neutralVertices.col(pointsToInclude[i]);
                src.col(i) = exprVertices.col(pointsToInclude[i]);
            }

            const auto affRes = Procrustes<float, 3>::AlignRigid(src, tgt);
            for (size_t i = 0; i < m->currentToScanTransforms.size(); ++i)
            {
                m->currentToScanTransforms[i] = m->currentToScanTransforms[i] * affRes.Inverse();
            }

            // repeat the fit but without rigid transform
            m->pcaRigFitting->PcaRigFittingRegistrationConfiguration()["optimizePose"].Set(false);
            m->currentToScanTransforms =
                m->pcaRigFitting->RegisterPcaRig(m->currentToScanTransforms, m->masks[FittingMaskType::NONRIGID], VertexWeights<float>(), /*iter=*/numIters);
        }

        m->currentFaceVertices = m->pcaRigFitting->CurrentVertices(/*meshId=*/0);
        m->currentTeethVertices = m->pcaRigFitting->CurrentVertices(/*meshId=*/1);
        m->currentEyeLeftVertices = m->pcaRigFitting->CurrentVertices(/*meshId=*/2);
        m->currentEyeRightVertices = m->pcaRigFitting->CurrentVertices(/*meshId=*/3);


        memcpy(OutStackedToScanTransforms,
               m->currentToScanTransforms.data(),
               int32_t(m->frameData.size()) * int32_t(16) * sizeof(float));

        memcpy(OutStackedToScanScales,
               m->currentToScanScales.data(),
               int32_t(m->frameData.size()) * sizeof(float));

        memcpy(OutVertexPositions,
               m->currentFaceVertices.data(),
               int32_t(m->currentFaceVertices.cols() * m->currentFaceVertices.rows()) * sizeof(float));

        // save the head mesh and PCA rig if we are saving Debug info
        if (!InDebugDataDirectory.empty())
        {
            std::filesystem::path headMeshFittedObjPath = InDebugDataDirectory;
            headMeshFittedObjPath /= "face_fitted.obj";
            m->faceTopology.SetVertices(m->currentFaceVertices);
            ObjFileWriter<float> writer;
            writer.writeObj(m->faceTopology, headMeshFittedObjPath.string());

            std::filesystem::path headMeshNeutralObjPath = InDebugDataDirectory;
            headMeshNeutralObjPath /= "face_neutral.obj";
            m->faceTopology.SetVertices(neutralVertices);
            writer.writeObj(m->faceTopology, headMeshNeutralObjPath.string());

            std::filesystem::path pcaRigPath(InDebugDataDirectory);
            pcaRigPath /= "pca_rig.dna";
            pma::ScopedPtr<dna::FileStream> outputStream = pma::makeScoped<dna::FileStream>(pcaRigPath.string().c_str(),
                                                                                            dna::FileStream::AccessMode::Write,
                                                                                            dna::FileStream::OpenMode::Binary);
            pma::ScopedPtr<dna::BinaryStreamWriter> pcaRigWriter = pma::makeScoped<dna::BinaryStreamWriter>(outputStream.get());
            m->pcaRigFitting->SaveRig(pcaRigWriter.operator->());
            pcaRigWriter->write();
        }


        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to fit rig logic: {}", e.what());
    }
}

bool ActorCreationAPI::CheckPcaModelFromDnaRigConfig(const char* InConfigurationFileOrJson, dna::Reader* InDnaStream)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InDnaStream, false, "input dna stream not valid");
        TITAN_CHECK_OR_RETURN(InConfigurationFileOrJson, false, "config file not valid");
        pma::PolyAllocator<rt::PCARigCreator> polyAllocator{ MEM_RESOURCE };

        std::shared_ptr<Rig<float>> dnaRig = std::allocate_shared<Rig<float>>(polyAllocator);
        dnaRig->LoadRig(InDnaStream);
        std::shared_ptr<rt::PCARigCreator> pcaRigCreator = std::allocate_shared<rt::PCARigCreator>(polyAllocator, dnaRig);

        TITAN_CHECK_OR_RETURN(pcaRigCreator->LoadConfig(InConfigurationFileOrJson), false, "failed to load pca to dna configuration");

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("PCA model from DNA rig config is not valid: {}", e.what());
    }
}


bool ActorCreationAPI::CalculatePcaModelFromDnaRig(const char* InConfigurationFileOrJson,
                                                   dna::Reader* InDnaStream,
                                                   dna::Writer* OutDnaStream,
                                                   const std::string& InDebugDataDirectory)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(InDnaStream, false, "input dna stream not valid");
        TITAN_CHECK_OR_RETURN(OutDnaStream, false, "output dna stream not valid");
        TITAN_CHECK_OR_RETURN(InConfigurationFileOrJson, false, "config file not valid");
        pma::PolyAllocator<rt::PCARigCreator> polyAllocator { MEM_RESOURCE };

        std::shared_ptr<Rig<float>> dnaRig = std::allocate_shared<Rig<float>>(polyAllocator);
        dnaRig->LoadRig(InDnaStream);
        std::shared_ptr<rt::PCARigCreator> pcaRigCreator = std::allocate_shared<rt::PCARigCreator>(polyAllocator, dnaRig);

        TITAN_CHECK_OR_RETURN(pcaRigCreator->LoadConfig(InConfigurationFileOrJson), false, "failed to load pca to dna configuration");
        TITAN_CHECK_OR_RETURN(pcaRigCreator->Create(), false, "failed to create pca from dna");

        pcaRigCreator->GetPCARig().SaveAsDNA(OutDnaStream);

        if (!InDebugDataDirectory.empty())
        {
            // save pca rig as DNA
            std::filesystem::path pcaRigPath(InDebugDataDirectory);
            pcaRigPath /= "pca_rig.dna";
            pcaRigCreator->GetPCARig().SaveAsDNA(pcaRigPath.string());
        }

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to create pca dna: {}", e.what());
    }
}

bool ActorCreationAPI::UpdateTeethSource(const float* InVertexPositions)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->teethFitting, false, "teeth fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");
        TITAN_CHECK_OR_RETURN(m->cameras.GetCameras().size() >= 1, false, "at least one camera have to be set");
        TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS || m->dataType == InputDataType::SCAN, false,
                        "no input data set");
        Eigen::Matrix3Xf verticesMap = Eigen::Map<const Eigen::Matrix<float, 3, -1, Eigen::ColMajor>>(
            (const float*)InVertexPositions,
            3,
            m->teethTopology.NumVertices())
            .template cast<float>();

        m->teethTopology.SetVertices(verticesMap);
        m->teethTopology.CalculateVertexNormals();
        m->teethFitting->SetSourceMesh(m->teethTopology);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to fit teeth: {}", e.what());
    }
}

bool ActorCreationAPI::UpdateHeadSource(const float* InVertexPositions)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");
        TITAN_CHECK_OR_RETURN(m->cameras.GetCameras().size() >= 1, false, "at least one camera have to be set");
        TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS || m->dataType == InputDataType::SCAN, false,
            "no input data set");
        Eigen::Matrix3Xf verticesMap = Eigen::Map<const Eigen::Matrix<float, 3, -1, Eigen::ColMajor>>(
            (const float*)InVertexPositions,
            3,
            m->faceTopology.NumVertices()).template cast<float>();

        m->faceTopology.SetVertices(verticesMap);
        m->faceTopology.CalculateVertexNormals();
        m->faceFitting->SetSourceMesh(m->faceTopology);
        m->UpdateCurrentVerticesState();

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to fit face: {}", e.what());
    }
}

bool ActorCreationAPI::CalcTeethDepthDelta(float InDeltaDistanceFromCamera, float& OutDx, float& OutDy, float& OutDz)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");
        TITAN_CHECK_OR_RETURN(m->cameras.GetCameras().size() >= 1, false, "at least one camera have to be set");
        TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS || m->dataType == InputDataType::SCAN, false,
                        "no input data set");
        TITAN_CHECK_OR_RETURN(m->currentTeethVertices.cols() == m->teethTopology.NumVertices(), false, "teeth fitting has not been performed");

        // convert teeth mean into world
        Eigen::Vector3f teethMeanWorld = m->currentToScanScales[0] * m->currentToScanTransforms[0].Transform(m->teethMean);
        Eigen::Vector3f cameraRayDeltaWorld = teethMeanWorld - m->cameras.GetCameras().begin()->second.Origin();
        cameraRayDeltaWorld.normalize();
        cameraRayDeltaWorld *= InDeltaDistanceFromCamera;
        Eigen::Vector3f teethMeanPlusDeltaWorld = teethMeanWorld + cameraRayDeltaWorld;

        // transform back into head reference frame
        Eigen::Vector3f teethMeanPlusDeltaHead = m->currentToScanTransforms[0].Inverse().Transform(teethMeanPlusDeltaWorld / m->currentToScanScales[0]);
        Eigen::Vector3f teethDeltaHead = teethMeanPlusDeltaHead - m->teethMean;
        OutDx = teethDeltaHead.x();
        OutDy = teethDeltaHead.y();
        OutDz = teethDeltaHead.z();

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to calculate teeth depth delta: {}", e.what());
    }
}

bool ActorCreationAPI::FitTeeth(float* OutVertexPositions, int32_t numIters, const std::string& InDebugDataDirectory)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->teethFitting, false, "teeth fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");
        TITAN_CHECK_OR_RETURN(m->cameras.GetCameras().size() >= 1, false, "at least one camera have to be set");
        TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS || m->dataType == InputDataType::SCAN, false,
                        "no input data set");
        TITAN_CHECK_OR_RETURN(m->CollectFittingData(), false, "face fitting data has not been collected");

        std::vector<VertexWeights<float>> teethVertexWeights(m->currentToScanTransforms.size(), m->masks[FittingMaskType::TEETH]);

        if (!m->teethFittingInitialized)
        {
            m->fittingInitializer->SetToScanTransforms(m->currentToScanTransforms);
            bool initTeethSuccess = m->fittingInitializer->InitializeTeeth(m->teethToHead,
                                                                           m->teethFitting->CurrentMeshLandmarks(),
                                                                           /*frame=*/0);
            TITAN_CHECK_OR_RETURN(initTeethSuccess, false, "failed to initialize teeth fitting");
            m->teethFittingInitialized = true;
        }


        // fit teeth
        m->teethToHead = m->teethFitting->RegisterNonRigidAsset(m->currentToScanTransforms,
                                                                m->teethToHead,
                                                                teethVertexWeights,
                                                                numIters);

        // apply teeth-to-head transformation to get teeth in head space
        m->currentTeethVertices = m->teethToHead.Transform(m->teethFitting->CurrentDeformedVertices());

        // calculate the mean of the teeth vertices for possible future use
        m->teethMean = Eigen::Vector3f::Zero();

        for (int i = 0; i < m->teethTopology.NumVertices(); i++)
        {
            m->teethMean += m->currentTeethVertices.col(i);
        }
        m->teethMean /= static_cast<float>(m->teethTopology.NumVertices());


        memcpy(OutVertexPositions,
               m->currentTeethVertices.data(),
               int32_t(m->currentTeethVertices.cols() * m->currentTeethVertices.rows()) *
               sizeof(float));

        // save the teeth mesh if we are saving Debug info
        if (!InDebugDataDirectory.empty())
        {
            std::filesystem::path teethMeshObjPath = InDebugDataDirectory;
            teethMeshObjPath /= "teeth_conformed.obj";
            m->teethTopology.SetVertices(m->currentTeethVertices);
            ObjFileWriter<float> writer;
            writer.writeObj(m->teethTopology, teethMeshObjPath.string());
        }

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to fit teeth: {}", e.what());
    }
}

bool ActorCreationAPI::FitEyes(float* OutLeftEyeVertexPositions,
                               float* OutRightEyeVertexPositions,
                               bool InSetInterfaceForFaceFitting,
                               int32_t InNumIters,
                               const bool InAutoMode,
                               const std::string& InDebugDataDirectory)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->leftEyeFitting, false, "eyes fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->rightEyeFitting, false, "eyes fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(!m->frameData.empty(), false, "frame data is empty");
        TITAN_CHECK_OR_RETURN(m->cameras.GetCameras().size() >= 1, false, "at least one camera have to be set");
        TITAN_CHECK_OR_RETURN(m->dataType == InputDataType::DEPTHS || m->dataType == InputDataType::SCAN, false,
                        "no input data set");
        TITAN_CHECK_OR_RETURN(m->CollectFittingData(), false, "face fitting data has not been collected");

        const int firstFrame = 0;

        std::vector<VertexWeights<float>> eyeVertexWeights(m->currentToScanTransforms.size(), m->masks[FittingMaskType::EYE]);
        std::vector<Affine<float, 3, 3>> leftEye2Scan(m->currentToScanTransforms.size());
        std::vector<Affine<float, 3, 3>> rightEye2Scan(m->currentToScanTransforms.size());

        if (!m->eyeFittingInitialized)
        {
            m->fittingInitializer->SetToScanTransforms(m->currentToScanTransforms);
            bool initEyesSuccess = m->fittingInitializer->InitializeEyes(m->eyeLeftToHead,
                                                                         m->eyeRightToHead,
                                                                         m->leftEyeFitting->CurrentMeshCurves(),
                                                                         m->rightEyeFitting->CurrentMeshCurves(),
                                                                         /*frame=*/0);
            TITAN_CHECK_OR_RETURN(initEyesSuccess, false, "failed to initialize eyes fitting");
            m->eyeFittingInitialized = true;
        }

        for (int i = 0; i < int(m->currentToScanTransforms.size()); ++i)
        {
            leftEye2Scan[i] = m->currentToScanTransforms[i] * m->eyeLeftToHead;
            rightEye2Scan[i] = m->currentToScanTransforms[i] * m->eyeRightToHead;
        }

        // fit eyes
        if (InAutoMode)
        {
            for (float modelRegularization : { 10.0f, 1.0f, 0.1f })
            {
                m->leftEyeFitting->ModelRegistrationConfiguration()["modelRegularization"].Set(modelRegularization);
                m->rightEyeFitting->ModelRegistrationConfiguration()["modelRegularization"].Set(modelRegularization);
                leftEye2Scan = m->leftEyeFitting->RegisterNonRigid(leftEye2Scan,
                                                                   eyeVertexWeights,
                                                                   InNumIters);
                rightEye2Scan = m->rightEyeFitting->RegisterNonRigid(rightEye2Scan,
                                                                     eyeVertexWeights,
                                                                     InNumIters);
            }
        }
        else
        {
            leftEye2Scan = m->leftEyeFitting->RegisterNonRigid(leftEye2Scan,
                                                               eyeVertexWeights,
                                                               InNumIters);
            rightEye2Scan = m->rightEyeFitting->RegisterNonRigid(rightEye2Scan,
                                                                 eyeVertexWeights,
                                                                 InNumIters);
        }

        // get the relative eye to head position using estimation from first frame
        m->eyeLeftToHead = m->currentToScanTransforms[firstFrame].Inverse() * leftEye2Scan[firstFrame];
        m->eyeRightToHead = m->currentToScanTransforms[firstFrame].Inverse() * rightEye2Scan[firstFrame];
        m->currentEyeLeftVertices = m->eyeLeftToHead.Transform(m->leftEyeFitting->CurrentDeformedVertices());
        m->currentEyeRightVertices = m->eyeRightToHead.Transform(m->rightEyeFitting->CurrentDeformedVertices());

        if (InSetInterfaceForFaceFitting)
        {
            m->faceFitting->SetupEyeballConstraint(m->currentEyeLeftVertices, m->currentEyeRightVertices);
        }

        memcpy(OutLeftEyeVertexPositions,
               m->currentEyeLeftVertices.data(),
               int32_t(m->currentEyeLeftVertices.cols() * m->currentEyeLeftVertices.rows()) *
               sizeof(float));

        memcpy(OutRightEyeVertexPositions,
               m->currentEyeRightVertices.data(),
               int32_t(m->currentEyeRightVertices.cols() * m->currentEyeRightVertices.rows()) * sizeof(float));

        // save the conformed eye meshes if we are saving Debug info
        if (!InDebugDataDirectory.empty())
        {
            std::filesystem::path eyeMeshObjPath = InDebugDataDirectory;
            eyeMeshObjPath /= "left_eye_conformed.obj";
            m->eyeTopology.SetVertices(m->currentEyeLeftVertices);
            ObjFileWriter<float> writer;
            writer.writeObj(m->eyeTopology, eyeMeshObjPath.string());
            eyeMeshObjPath = InDebugDataDirectory;
            eyeMeshObjPath /= "right_eye_conformed.obj";
            m->eyeTopology.SetVertices(m->currentEyeRightVertices);
            writer.writeObj(m->eyeTopology, eyeMeshObjPath.string());
        }


        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to fit eyes: {}", e.what());
    }
}

bool ActorCreationAPI::GetIdentityModelType(IdentityModelType& identityType)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");

        m->identityModelType = identityType;

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to fit eyes: {}", e.what());
    }
}

bool ActorCreationAPI::GetFittingState(float* OutStackedToScanTransforms,
                                       float* OutStackedToScanScales,
                                       float* OutFaceMeshVertices,
                                       float* OutTeethMeshVertices,
                                       float* OutLeftEyeMeshVertices,
                                       float* OutRightEyeMeshVertices)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->leftEyeFitting, false, "eyes fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->rightEyeFitting, false, "eyes fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->currentFaceVertices.size() > 0, false, "no state set for head vertices");
        TITAN_CHECK_OR_RETURN(m->currentTeethVertices.size() > 0, false, "no state set for teeth vertices");
        TITAN_CHECK_OR_RETURN(m->currentEyeLeftVertices.size() > 0, false, "no state set for eye left vertices");
        TITAN_CHECK_OR_RETURN(m->currentEyeRightVertices.size() > 0, false, "no state set for eye right vertices");

        memcpy(OutStackedToScanTransforms,
               m->currentToScanTransforms.data(),
               int32_t(m->frameData.size()) * int32_t(16) * sizeof(float));

        memcpy(OutStackedToScanScales,
               m->currentToScanScales.data(),
               int32_t(m->frameData.size()) * sizeof(float));

        memcpy(OutFaceMeshVertices,
               m->currentFaceVertices.data(),
               int32_t(m->currentFaceVertices.cols() * m->currentFaceVertices.rows()) * sizeof(float));

        memcpy(OutTeethMeshVertices,
               m->currentTeethVertices.data(),
               int32_t(m->currentTeethVertices.cols() * m->currentTeethVertices.rows()) * sizeof(float));

        memcpy(OutLeftEyeMeshVertices,
               m->currentEyeLeftVertices.data(),
               int32_t(m->currentEyeLeftVertices.cols() * m->currentEyeLeftVertices.rows()) * sizeof(float));

        memcpy(OutRightEyeMeshVertices,
               m->currentEyeRightVertices.data(),
               int32_t(m->currentEyeRightVertices.cols() * m->currentEyeRightVertices.rows()) * sizeof(float));

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to fit eyes: {}", e.what());
    }
}

bool ActorCreationAPI::GenerateBrowMeshLandmarks(const std::string& InCameraName, std::string& OutJsonStream, bool concatenate) const
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->CollectFittingData(), false, "face fitting data has not been collected");
        m->browLandmarksGenerator->SetLandmarks(Extract2DLandmarksForCamera(m->frameData[0], m->cameras, InCameraName));

        OutJsonStream.clear();

        const auto headMeshLandmarks = m->browLandmarksGenerator->Generate(m->faceFitting->CurrentDeformedVertices(),
                                                                           m->currentToScanTransforms[0],
                                                                           m->currentToScanScales[0],
                                                                           concatenate);

        std::string localStream = "{}";
        localStream = headMeshLandmarks.SerializeJson(localStream, "head_lod0_mesh");
        if (concatenate)
        {
            localStream = m->meshLandmarks["teeth"].SerializeJson(localStream, "teeth_lod0_mesh");
            localStream = m->meshLandmarks["eye_left"].SerializeJson(localStream, "eyeLeft_lod0_mesh");
            localStream = m->meshLandmarks["eye_right"].SerializeJson(localStream, "eyeRight_lod0_mesh");
        }

        OutJsonStream = localStream;

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to generate brow landmarks: {}", e.what());
    }
}

bool ActorCreationAPI::LoadFittingConfigurations(const std::string& InJsonString)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->teethFitting, false, "teeth fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->leftEyeFitting, false, "left eye fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->rightEyeFitting, false, "right eye fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->rigLogicFitting, false, "riglogic fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->pcaRigFitting, false, "pca rig fitting has not been initialized");

        TITAN_NAMESPACE::JsonElement jsonConfig = TITAN_NAMESPACE::ReadJson(InJsonString);
        std::vector<Configuration*> allConfigs = { &m->faceFitting->ModelRegistrationConfiguration(),
                                                   &m->faceFitting->FineRegistrationConfiguration(),
                                                   &m->faceFitting->RigidRegistrationConfiguration(),
                                                   &m->rigLogicFitting->RigLogicRegistrationConfiguration(),
                                                   &m->teethFitting->RigidRegistrationConfiguration(),
                                                   &m->teethFitting->ModelRegistrationConfiguration(),
                                                   &m->teethFitting->FineRegistrationConfiguration(),
                                                   &m->leftEyeFitting->RigidRegistrationConfiguration(),
                                                   &m->leftEyeFitting->ModelRegistrationConfiguration(),
                                                   &m->leftEyeFitting->FineRegistrationConfiguration(),
                                                   &m->rightEyeFitting->RigidRegistrationConfiguration(),
                                                   &m->rightEyeFitting->ModelRegistrationConfiguration(),
                                                   &m->rightEyeFitting->FineRegistrationConfiguration(),
                                                   &m->pcaRigFitting->PcaRigFittingRegistrationConfiguration()
        };

        if (jsonConfig.Contains("landmark and curve weights"))
        {
            m->landmarkAndCurveWeights = jsonConfig["landmark and curve weights"].Get<std::map<std::string, float>>();
            m->faceFitting->SetGlobalUserDefinedLandmarkAndCurveWeights(m->landmarkAndCurveWeights);
        }
        else
        {
            LOG_WARNING("configuration is missing the landmark and curve weights");
        }

        if (jsonConfig.Contains("face fitting configuration"))
        {
            for (size_t i = 0; i < allConfigs.size(); ++i)
            {
                std::vector<std::string> unspecifiedKeys;
                std::vector<std::string> unknownKeys;

                if (jsonConfig["face fitting configuration"].Contains(allConfigs[i]->Name()))
                {
                    allConfigs[i]->FromJson(jsonConfig["face fitting configuration"][allConfigs[i]->Name()], unspecifiedKeys, unknownKeys);
                    for (const std::string& key : unspecifiedKeys)
                    {
                        LOG_WARNING("config is not specifying {}", key);
                    }
                    for (const std::string& key : unknownKeys)
                    {
                        LOG_WARNING("config contains unknown key {}", key);
                    }
                }
                else
                {
                    LOG_WARNING("Face fitting configuration does not contain {}", allConfigs[i]->Name());
                }
            }
        }
        else
        {
            LOG_WARNING("configuration {} is missing the optimization parameters", InJsonString);
        }

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to generate brow landmarks: {}", e.what());
    }
}

bool ActorCreationAPI::SaveFittingConfigurations(std::string& OutJsonString) const
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->teethFitting, false, "teeth fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->leftEyeFitting, false, "left eye fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->rightEyeFitting, false, "right eye fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->rigLogicFitting, false, "riglogic fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->pcaRigFitting, false, "pca rig fitting has not been initialized");

        std::vector<Configuration*> allConfigs = { &m->faceFitting->ModelRegistrationConfiguration(),
                                                   &m->faceFitting->FineRegistrationConfiguration(),
                                                   &m->faceFitting->RigidRegistrationConfiguration(),
                                                   &m->rigLogicFitting->RigLogicRegistrationConfiguration(),
                                                   &m->teethFitting->RigidRegistrationConfiguration(),
                                                   &m->teethFitting->ModelRegistrationConfiguration(),
                                                   &m->teethFitting->FineRegistrationConfiguration(),
                                                   &m->pcaRigFitting->PcaRigFittingRegistrationConfiguration()
        };

        TITAN_NAMESPACE::JsonElement globalConfigs(TITAN_NAMESPACE::JsonElement::JsonType::Object);
        TITAN_NAMESPACE::JsonElement solverConfigs(TITAN_NAMESPACE::JsonElement::JsonType::Object);

        globalConfigs.Insert("landmark and curve weights", TITAN_NAMESPACE::JsonElement(m->landmarkAndCurveWeights));
        for (size_t i = 0; i < allConfigs.size(); ++i)
        {
            solverConfigs.Insert(allConfigs[i]->Name(), allConfigs[i]->ToJson());
        }
        globalConfigs.Insert("face fitting configuration", std::move(solverConfigs));
        OutJsonString = TITAN_NAMESPACE::WriteJson(globalConfigs, 1);

        return true;
    }
    catch (const std::exception& e)
    {
        TITAN_HANDLE_EXCEPTION("failure to generate brow landmarks: {}", e.what());
    }
}

// get parameters
float ActorCreationAPI::GetModelRegularization() const
{
    return m->faceFitting->ModelRegistrationConfiguration()["modelRegularization"].template Value<float>();
}

float ActorCreationAPI::GetPerVertexOffsetRegularization() const
{
    return m->faceFitting->FineRegistrationConfiguration()["vertexOffsetRegularization"].template Value<float>();
}

float ActorCreationAPI::GetPerVertexLaplacianRegularization() const
{
    return m->faceFitting->FineRegistrationConfiguration()["vertexLaplacian"].template Value<float>();
}

float ActorCreationAPI::GetMinimumDistanceThreshold() const
{
    return m->faceFitting->ModelRegistrationConfiguration()["minimumDistanceThreshold"].template Value<float>();
}

bool ActorCreationAPI::GetUseMinimumDistanceThreshold() const
{
    return m->faceFitting->ModelRegistrationConfiguration()["useDistanceThreshold"].template Value<bool>();
}

bool ActorCreationAPI::GetAutoMultiViewLandmarkMasking() const
{
    return m->multiViewLandmarkMasking;
}

float ActorCreationAPI::GetLandmarksWeight() const
{
    return m->faceFitting->ModelRegistrationConfiguration()["landmarksWeight"].template Value<float>();
}

float ActorCreationAPI::GetInnerLipsLandmarksWeight() const
{
    return m->faceFitting->ModelRegistrationConfiguration()["innerLipWeight"].template Value<float>();
}

float ActorCreationAPI::GetInnerLipsCollisionWeight() const
{
    return m->faceFitting->FineRegistrationConfiguration()["collisionWeight"].template Value<float>();
}

float ActorCreationAPI::GetRigLogicL1RegularizationWeight() const
{
    return m->rigLogicFitting->RigLogicRegistrationConfiguration()["l1regularization"].template Value<float>();
}

// set parameters
void ActorCreationAPI::SetModelRegularization(float regularization)
{
    m->faceFitting->ModelRegistrationConfiguration()["modelRegularization"].Set(regularization);
    m->teethFitting->ModelRegistrationConfiguration()["modelRegularization"].Set(regularization);
    m->leftEyeFitting->ModelRegistrationConfiguration()["modelRegularization"].Set(regularization);
    m->rightEyeFitting->ModelRegistrationConfiguration()["modelRegularization"].Set(regularization);
}

void ActorCreationAPI::SetPerVertexOffsetRegularization(float regularization)
{
    m->faceFitting->FineRegistrationConfiguration()["vertexOffsetRegularization"].Set(regularization);
    m->teethFitting->FineRegistrationConfiguration()["vertexOffsetRegularization"].Set(regularization);
}

void ActorCreationAPI::SetPerVertexLaplacianRegularization(float regularization)
{
    m->faceFitting->FineRegistrationConfiguration()["vertexLaplacian"].Set(regularization);
    m->teethFitting->FineRegistrationConfiguration()["vertexLaplacian"].Set(regularization);
}

void ActorCreationAPI::SetMinimumDistanceThreshold(float threshold)
{
    m->faceFitting->ModelRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->faceFitting->FineRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->faceFitting->RigidRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->rigLogicFitting->RigLogicRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->pcaRigFitting->PcaRigFittingRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->teethFitting->ModelRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->teethFitting->RigidRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->teethFitting->FineRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->leftEyeFitting->ModelRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->leftEyeFitting->RigidRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->leftEyeFitting->FineRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->rightEyeFitting->ModelRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->rightEyeFitting->RigidRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
    m->rightEyeFitting->FineRegistrationConfiguration()["minimumDistanceThreshold"].Set(threshold);
}

void ActorCreationAPI::SetUseMinimumDistanceThreshold(bool useThreshold)
{
    m->faceFitting->ModelRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->faceFitting->FineRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->faceFitting->RigidRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->rigLogicFitting->RigLogicRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->pcaRigFitting->PcaRigFittingRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->teethFitting->ModelRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->teethFitting->RigidRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->teethFitting->FineRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->leftEyeFitting->ModelRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->leftEyeFitting->RigidRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->leftEyeFitting->FineRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->rightEyeFitting->ModelRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->rightEyeFitting->RigidRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
    m->rightEyeFitting->FineRegistrationConfiguration()["useDistanceThreshold"].Set(useThreshold);
}

void ActorCreationAPI::SetAutoMultiViewLandmarkMasking(bool useMultiViewLandmarkMasking)
{
    m->multiViewLandmarkMasking = useMultiViewLandmarkMasking;
    m->fittingDataCollected = false;
}

void ActorCreationAPI::SetLandmarksWeight(float weight)
{
    m->faceFitting->ModelRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->faceFitting->FineRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->faceFitting->RigidRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->rigLogicFitting->RigLogicRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->pcaRigFitting->PcaRigFittingRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->teethFitting->ModelRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->teethFitting->RigidRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->teethFitting->FineRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->leftEyeFitting->ModelRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->leftEyeFitting->RigidRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->leftEyeFitting->FineRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->rightEyeFitting->ModelRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->rightEyeFitting->RigidRegistrationConfiguration()["landmarksWeight"].Set(weight);
    m->rightEyeFitting->FineRegistrationConfiguration()["landmarksWeight"].Set(weight);
}

void ActorCreationAPI::SetInnerLipsLandmarksWeight(float weight)
{
    m->faceFitting->ModelRegistrationConfiguration()["innerLipWeight"].Set(weight);
    m->faceFitting->FineRegistrationConfiguration()["innerLipWeight"].Set(weight);
    m->faceFitting->RigidRegistrationConfiguration()["innerLipWeight"].Set(weight);
    m->rigLogicFitting->RigLogicRegistrationConfiguration()["innerLipWeight"].Set(weight);
    m->pcaRigFitting->PcaRigFittingRegistrationConfiguration()["innerLipWeight"].Set(weight);
}

void ActorCreationAPI::SetInnerLipsCollisionWeight(float weight) { m->faceFitting->FineRegistrationConfiguration()["collisionWeight"].Set(weight); }

void ActorCreationAPI::SetRigLogicL1RegularizationWeight(float weight) {
    m->rigLogicFitting->RigLogicRegistrationConfiguration()["l1regularization"].Set(weight);
}

bool ActorCreationAPI::GetFittingMask(float* OutVertexWeights, FittingMaskType InMaskType) const
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

bool ActorCreationAPI::SetFittingMask(float* InVertexWeights, FittingMaskType InMaskType)
{
    try
    {
        TITAN_RESET_ERROR;
        TITAN_CHECK_OR_RETURN(m->faceFitting, false, "face fitting has not been initialized");
        TITAN_CHECK_OR_RETURN(m->teethFitting, false, "teeth fitting has not been initialized");

        int32_t numVertices = (int32_t)m->faceFitting->CurrentDeformedVertices().cols();
        if (InMaskType == FittingMaskType::TEETH)
        {
            numVertices = (int32_t)m->teethFitting->CurrentDeformedVertices().cols();
        }
        if (InMaskType == FittingMaskType::EYE)
        {
            numVertices = (int32_t)m->leftEyeFitting->CurrentDeformedVertices().cols();
        }

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

} // namespace TITAN_API_NAMESPACE
