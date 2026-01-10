/**
 * @file DNAIntegrationTests.cpp
 * @brief Comprehensive unit tests for MetaHuman DNA integration systems
 *
 * Tests cover:
 * - PythonDNACalibWrapper blend shape and joint operations
 * - MetaHumanDNABridge DNA loading and manipulation
 * - DNABodySchemaBinding motor control feedback
 * - CognitiveVisualizationComponent visualization methods
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <array>
#include <cmath>
#include <string>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <set>

// ============================================================================
// Mock Types for DNA Integration System
// ============================================================================

struct FVector3 {
    float X = 0.0f, Y = 0.0f, Z = 0.0f;

    FVector3() = default;
    FVector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

    float Length() const { return std::sqrt(X*X + Y*Y + Z*Z); }

    static float Dist(const FVector3& a, const FVector3& b) {
        FVector3 diff(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        return diff.Length();
    }

    FVector3 operator+(const FVector3& other) const {
        return FVector3(X + other.X, Y + other.Y, Z + other.Z);
    }
    FVector3 operator-(const FVector3& other) const {
        return FVector3(X - other.X, Y - other.Y, Z - other.Z);
    }
    FVector3 operator*(float scalar) const {
        return FVector3(X * scalar, Y * scalar, Z * scalar);
    }

    static FVector3 ZeroVector() { return FVector3(0, 0, 0); }
};

struct FRotator {
    float Roll = 0.0f, Pitch = 0.0f, Yaw = 0.0f;

    FRotator() = default;
    FRotator(float roll, float pitch, float yaw) : Roll(roll), Pitch(pitch), Yaw(yaw) {}

    static FRotator ZeroRotator() { return FRotator(0, 0, 0); }

    FRotator operator-(const FRotator& other) const {
        return FRotator(Roll - other.Roll, Pitch - other.Pitch, Yaw - other.Yaw);
    }
};

struct FTransform {
    FVector3 Location;
    FRotator Rotation;
    FVector3 Scale = FVector3(1, 1, 1);

    FTransform() = default;
    FTransform(const FRotator& rot, const FVector3& loc) : Location(loc), Rotation(rot) {}

    static FTransform Identity() { return FTransform(); }

    FVector3 GetLocation() const { return Location; }
};

struct FLinearColor {
    float R = 1.0f, G = 1.0f, B = 1.0f, A = 1.0f;

    FLinearColor() = default;
    FLinearColor(float r, float g, float b, float a = 1.0f) : R(r), G(g), B(b), A(a) {}

    static FLinearColor White() { return FLinearColor(1, 1, 1, 1); }

    static FLinearColor LerpUsingHSV(const FLinearColor& a, const FLinearColor& b, float t) {
        return FLinearColor(
            a.R + (b.R - a.R) * t,
            a.G + (b.G - a.G) * t,
            a.B + (b.B - a.B) * t,
            a.A + (b.A - a.A) * t
        );
    }
};

// ============================================================================
// Mock PythonDNACalibWrapper
// ============================================================================

class MockPythonDNACalibWrapper {
public:
    MockPythonDNACalibWrapper() : bInitialized(false) {}

    bool Initialize(const std::string& dnaCalibPath) {
        DNACalibRootPath = dnaCalibPath;
        bInitialized = true;
        return true;
    }

    void Shutdown() { bInitialized = false; }
    bool IsInitialized() const { return bInitialized; }

    bool LoadDNA(const std::string& dnaPath) {
        if (!bInitialized) return false;
        LoadedDNAPath = dnaPath;
        bDNALoaded = true;
        InitializeSimulatedData();
        return true;
    }

    bool SaveDNA(const std::string& dnaPath) {
        if (!bInitialized || !bDNALoaded) return false;
        return true;
    }

    std::string GetDNADatabaseName() const {
        return DatabaseName;
    }

    std::vector<std::string> GetJointNames() const {
        return JointNames;
    }

    std::vector<std::string> GetBlendShapeNames() const {
        return BlendShapeNames;
    }

    std::vector<std::string> GetMeshNames() const {
        return MeshNames;
    }

    int GetJointCount() const { return (int)JointNames.size(); }
    int GetBlendShapeCount() const { return (int)BlendShapeNames.size(); }
    int GetMeshCount() const { return (int)MeshNames.size(); }

    // Joint operations
    bool RenameJoint(const std::string& oldName, const std::string& newName) {
        if (!bInitialized || !bDNALoaded) return false;
        for (auto& name : JointNames) {
            if (name == oldName) {
                name = newName;
                return true;
            }
        }
        return false;
    }

    bool RemoveJoint(const std::string& jointName) {
        if (!bInitialized || !bDNALoaded) return false;
        auto it = std::find(JointNames.begin(), JointNames.end(), jointName);
        if (it != JointNames.end()) {
            JointNames.erase(it);
            return true;
        }
        return false;
    }

    // Blend shape operations (NEW)
    bool RenameBlendShape(const std::string& oldName, const std::string& newName) {
        if (!bInitialized || !bDNALoaded) {
            LastError = "No DNA loaded";
            return false;
        }
        for (auto& name : BlendShapeNames) {
            if (name == oldName) {
                name = newName;
                return true;
            }
        }
        return false;
    }

    bool RemoveBlendShape(const std::string& blendShapeName) {
        if (!bInitialized || !bDNALoaded) {
            LastError = "No DNA loaded";
            return false;
        }
        auto it = std::find(BlendShapeNames.begin(), BlendShapeNames.end(), blendShapeName);
        if (it != BlendShapeNames.end()) {
            BlendShapeNames.erase(it);
            return true;
        }
        return false;
    }

    bool ModifyBlendShapeDeltas(const std::string& blendShapeName, const std::string& meshName,
                                 const std::vector<FVector3>& deltas) {
        if (!bInitialized || !bDNALoaded) {
            LastError = "No DNA loaded";
            return false;
        }
        if (deltas.empty()) {
            LastError = "No deltas provided";
            return false;
        }
        BlendShapeDeltas[blendShapeName] = deltas;
        return true;
    }

    bool ClearBlendShapes() {
        if (!bInitialized || !bDNALoaded) return false;
        BlendShapeNames.clear();
        BlendShapeDeltas.clear();
        return true;
    }

    bool RemoveLOD(int lodIndex) {
        if (!bInitialized || !bDNALoaded) return false;
        if (lodIndex < 0 || lodIndex > 7) return false;

        // Remove meshes with this LOD
        std::string lodSuffix = "_lod" + std::to_string(lodIndex);
        MeshNames.erase(
            std::remove_if(MeshNames.begin(), MeshNames.end(),
                [&lodSuffix](const std::string& name) {
                    return name.size() >= lodSuffix.size() &&
                           name.substr(name.size() - lodSuffix.size()) == lodSuffix;
                }),
            MeshNames.end()
        );
        return true;
    }

    // Joint transform operations (NEW)
    bool GetJointNeutralTransform(const std::string& jointName, FTransform& outTransform) {
        if (!bInitialized || !bDNALoaded) {
            outTransform = FTransform::Identity();
            return false;
        }

        auto it = JointTransforms.find(jointName);
        if (it != JointTransforms.end()) {
            outTransform = it->second;
            return true;
        }

        outTransform = FTransform::Identity();
        return true;
    }

    int GetJointParentIndex(const std::string& jointName) {
        if (!bInitialized || !bDNALoaded) return -1;

        auto it = JointParentIndices.find(jointName);
        if (it != JointParentIndices.end()) {
            return it->second;
        }
        return -1;
    }

    std::string GetBlendShapeTargetMesh(const std::string& blendShapeName) {
        if (!bInitialized || !bDNALoaded) return "";

        // All facial blend shapes target head_lod0
        if (blendShapeName.find("brow") != std::string::npos ||
            blendShapeName.find("eye") != std::string::npos ||
            blendShapeName.find("mouth") != std::string::npos ||
            blendShapeName.find("jaw") != std::string::npos) {
            return "head_lod0";
        }
        return "head_lod0";
    }

    int GetBlendShapeVertexCount(const std::string& blendShapeName) {
        if (!bInitialized || !bDNALoaded) return 0;

        auto it = BlendShapeVertexCounts.find(blendShapeName);
        if (it != BlendShapeVertexCounts.end()) {
            return it->second;
        }
        return 500; // Default
    }

    std::string GetLastError() const { return LastError; }

    void SetDatabaseName(const std::string& name) { DatabaseName = name; }

private:
    void InitializeSimulatedData() {
        // Joint names
        JointNames = {"root", "pelvis", "spine_01", "spine_02", "spine_03",
                      "neck_01", "neck_02", "head", "clavicle_l", "upperarm_l",
                      "lowerarm_l", "hand_l", "clavicle_r", "upperarm_r",
                      "lowerarm_r", "hand_r", "thigh_l", "calf_l", "foot_l",
                      "thigh_r", "calf_r", "foot_r", "FACIAL_C_FacialRoot"};

        // Blend shape names
        BlendShapeNames = {"brow_down_L", "brow_down_R", "brow_inner_up_L", "brow_inner_up_R",
                           "eye_blink_L", "eye_blink_R", "eye_wide_L", "eye_wide_R",
                           "mouth_smile_L", "mouth_smile_R", "mouth_frown_L", "mouth_frown_R",
                           "jaw_open", "jaw_forward", "nose_sneer_L", "nose_sneer_R"};

        // Mesh names
        MeshNames = {"head_lod0", "head_lod1", "body_lod0", "body_lod1", "teeth_lod0"};

        // Joint transforms
        JointTransforms["root"] = FTransform(FRotator::ZeroRotator(), FVector3(0, 0, 0));
        JointTransforms["pelvis"] = FTransform(FRotator::ZeroRotator(), FVector3(0, 0, 100));
        JointTransforms["spine_01"] = FTransform(FRotator::ZeroRotator(), FVector3(0, 0, 10));
        JointTransforms["neck_01"] = FTransform(FRotator::ZeroRotator(), FVector3(0, 0, 8));
        JointTransforms["head"] = FTransform(FRotator::ZeroRotator(), FVector3(0, 0, 8));

        // Joint parent indices
        JointParentIndices["root"] = -1;
        JointParentIndices["pelvis"] = 0;
        JointParentIndices["spine_01"] = 1;
        JointParentIndices["spine_02"] = 2;
        JointParentIndices["neck_01"] = 4;
        JointParentIndices["head"] = 6;
        JointParentIndices["FACIAL_C_FacialRoot"] = 7;

        // Blend shape vertex counts
        BlendShapeVertexCounts["brow_down_L"] = 450;
        BlendShapeVertexCounts["eye_blink_L"] = 580;
        BlendShapeVertexCounts["mouth_smile_L"] = 720;
        BlendShapeVertexCounts["jaw_open"] = 1500;

        // Only set DatabaseName if not already set by SetDatabaseName()
        if (DatabaseName.empty()) {
            DatabaseName = "DHI";
        }
    }

    bool bInitialized;
    bool bDNALoaded = false;
    std::string DNACalibRootPath;
    std::string LoadedDNAPath;
    std::string LastError;
    std::string DatabaseName;

    std::vector<std::string> JointNames;
    std::vector<std::string> BlendShapeNames;
    std::vector<std::string> MeshNames;

    std::unordered_map<std::string, FTransform> JointTransforms;
    std::unordered_map<std::string, int> JointParentIndices;
    std::unordered_map<std::string, int> BlendShapeVertexCounts;
    std::unordered_map<std::string, std::vector<FVector3>> BlendShapeDeltas;
};

// ============================================================================
// Mock MetaHumanDNABridge
// ============================================================================

struct FDNAJointInfo {
    std::string JointName;
    int JointIndex = -1;
    int ParentIndex = -1;
    FTransform NeutralTransform;
};

struct FDNABlendShapeInfo {
    std::string BlendShapeName;
    int BlendShapeIndex = -1;
    std::string TargetMeshName;
    int VertexCount = 0;
};

struct FDNAMeshInfo {
    std::string MeshName;
    int MeshIndex = -1;
    int LODIndex = 0;
};

enum class EDNAVersion { Unknown, DHI, MH4 };

class MockMetaHumanDNABridge {
public:
    MockMetaHumanDNABridge() : bDNALoaded(false), DNAVersion(EDNAVersion::Unknown) {
        PythonWrapper = std::make_unique<MockPythonDNACalibWrapper>();
    }

    bool Initialize(const std::string& dnaCalibPath) {
        return PythonWrapper->Initialize(dnaCalibPath);
    }

    bool LoadDNA(const std::string& dnaPath) {
        if (!PythonWrapper->LoadDNA(dnaPath)) return false;
        LoadDNAData();
        ParseDNAVersion();
        BuildCacheMaps();
        bDNALoaded = true;
        return true;
    }

    bool IsDNALoaded() const { return bDNALoaded; }
    EDNAVersion GetDNAVersion() const { return DNAVersion; }

    // Blend shape operations
    bool RenameBlendShape(const std::string& oldName, const std::string& newName) {
        if (!bDNALoaded) return false;
        if (BlendShapeNameToIndexMap.find(oldName) == BlendShapeNameToIndexMap.end()) return false;
        if (BlendShapeNameToIndexMap.find(newName) != BlendShapeNameToIndexMap.end()) return false;

        if (!PythonWrapper->RenameBlendShape(oldName, newName)) return false;

        LoadDNAData();
        BuildCacheMaps();
        return true;
    }

    bool RemoveBlendShape(const std::string& blendShapeName) {
        if (!bDNALoaded) return false;
        if (BlendShapeNameToIndexMap.find(blendShapeName) == BlendShapeNameToIndexMap.end()) return false;

        if (!PythonWrapper->RemoveBlendShape(blendShapeName)) return false;

        LoadDNAData();
        BuildCacheMaps();
        return true;
    }

    bool ModifyBlendShapeDeltas(const std::string& blendShapeName, const std::vector<FVector3>& deltas) {
        if (!bDNALoaded) return false;

        std::string targetMesh = PythonWrapper->GetBlendShapeTargetMesh(blendShapeName);
        if (targetMesh.empty()) targetMesh = "head_lod0";

        if (!PythonWrapper->ModifyBlendShapeDeltas(blendShapeName, targetMesh, deltas)) return false;

        // Update cache
        auto it = BlendShapeNameToIndexMap.find(blendShapeName);
        if (it != BlendShapeNameToIndexMap.end() && it->second < (int)BlendShapeInfoCache.size()) {
            BlendShapeInfoCache[it->second].VertexCount = (int)deltas.size();
        }
        return true;
    }

    bool ClearAllBlendShapes() {
        if (!bDNALoaded) return false;
        if (!PythonWrapper->ClearBlendShapes()) return false;

        BlendShapeInfoCache.clear();
        BlendShapeNameToIndexMap.clear();
        return true;
    }

    bool RemoveLOD(int lodIndex) {
        if (!bDNALoaded) return false;
        if (lodIndex < 0 || lodIndex > 7) return false;

        if (!PythonWrapper->RemoveLOD(lodIndex)) return false;

        // Update mesh cache
        std::string lodSuffix = "_lod" + std::to_string(lodIndex);
        MeshInfoCache.erase(
            std::remove_if(MeshInfoCache.begin(), MeshInfoCache.end(),
                [&lodSuffix](const FDNAMeshInfo& mesh) {
                    return mesh.MeshName.size() >= lodSuffix.size() &&
                           mesh.MeshName.substr(mesh.MeshName.size() - lodSuffix.size()) == lodSuffix;
                }),
            MeshInfoCache.end()
        );

        // Rebuild mesh index map
        MeshNameToIndexMap.clear();
        for (size_t i = 0; i < MeshInfoCache.size(); i++) {
            MeshNameToIndexMap[MeshInfoCache[i].MeshName] = (int)i;
        }
        return true;
    }

    // Accessors
    int GetJointCount() const { return (int)JointInfoCache.size(); }
    int GetBlendShapeCount() const { return (int)BlendShapeInfoCache.size(); }
    int GetMeshCount() const { return (int)MeshInfoCache.size(); }

    bool GetJointInfo(const std::string& jointName, FDNAJointInfo& outInfo) const {
        auto it = JointNameToIndexMap.find(jointName);
        if (it == JointNameToIndexMap.end()) return false;
        if (it->second < 0 || it->second >= (int)JointInfoCache.size()) return false;
        outInfo = JointInfoCache[it->second];
        return true;
    }

    bool GetBlendShapeInfo(const std::string& blendShapeName, FDNABlendShapeInfo& outInfo) const {
        auto it = BlendShapeNameToIndexMap.find(blendShapeName);
        if (it == BlendShapeNameToIndexMap.end()) return false;
        if (it->second < 0 || it->second >= (int)BlendShapeInfoCache.size()) return false;
        outInfo = BlendShapeInfoCache[it->second];
        return true;
    }

    void SetDatabaseNameForTesting(const std::string& name) {
        PythonWrapper->SetDatabaseName(name);
    }

private:
    void LoadDNAData() {
        JointInfoCache.clear();
        BlendShapeInfoCache.clear();
        MeshInfoCache.clear();

        // Load joints
        auto jointNames = PythonWrapper->GetJointNames();
        for (size_t i = 0; i < jointNames.size(); i++) {
            FDNAJointInfo info;
            info.JointName = jointNames[i];
            info.JointIndex = (int)i;

            FTransform transform;
            PythonWrapper->GetJointNeutralTransform(jointNames[i], transform);
            info.NeutralTransform = transform;
            info.ParentIndex = PythonWrapper->GetJointParentIndex(jointNames[i]);

            JointInfoCache.push_back(info);
        }

        // Load blend shapes
        auto blendShapeNames = PythonWrapper->GetBlendShapeNames();
        for (size_t i = 0; i < blendShapeNames.size(); i++) {
            FDNABlendShapeInfo info;
            info.BlendShapeName = blendShapeNames[i];
            info.BlendShapeIndex = (int)i;
            info.TargetMeshName = PythonWrapper->GetBlendShapeTargetMesh(blendShapeNames[i]);
            info.VertexCount = PythonWrapper->GetBlendShapeVertexCount(blendShapeNames[i]);

            BlendShapeInfoCache.push_back(info);
        }

        // Load meshes
        auto meshNames = PythonWrapper->GetMeshNames();
        for (size_t i = 0; i < meshNames.size(); i++) {
            FDNAMeshInfo info;
            info.MeshName = meshNames[i];
            info.MeshIndex = (int)i;

            // Parse LOD from name
            for (int lod = 0; lod < 8; lod++) {
                std::string lodSuffix = "_lod" + std::to_string(lod);
                if (meshNames[i].size() >= lodSuffix.size() &&
                    meshNames[i].substr(meshNames[i].size() - lodSuffix.size()) == lodSuffix) {
                    info.LODIndex = lod;
                    break;
                }
            }

            MeshInfoCache.push_back(info);
        }
    }

    void ParseDNAVersion() {
        std::string dbName = PythonWrapper->GetDNADatabaseName();
        if (dbName == "MH.4" || dbName.find("MH4") != std::string::npos) {
            DNAVersion = EDNAVersion::MH4;
        } else if (dbName == "DHI" || dbName.find("DHI") != std::string::npos) {
            DNAVersion = EDNAVersion::DHI;
        } else {
            DNAVersion = EDNAVersion::DHI; // Default
        }
    }

    void BuildCacheMaps() {
        JointNameToIndexMap.clear();
        BlendShapeNameToIndexMap.clear();
        MeshNameToIndexMap.clear();

        for (size_t i = 0; i < JointInfoCache.size(); i++) {
            JointNameToIndexMap[JointInfoCache[i].JointName] = (int)i;
        }
        for (size_t i = 0; i < BlendShapeInfoCache.size(); i++) {
            BlendShapeNameToIndexMap[BlendShapeInfoCache[i].BlendShapeName] = (int)i;
        }
        for (size_t i = 0; i < MeshInfoCache.size(); i++) {
            MeshNameToIndexMap[MeshInfoCache[i].MeshName] = (int)i;
        }
    }

    bool bDNALoaded;
    EDNAVersion DNAVersion;
    std::unique_ptr<MockPythonDNACalibWrapper> PythonWrapper;

    std::vector<FDNAJointInfo> JointInfoCache;
    std::vector<FDNABlendShapeInfo> BlendShapeInfoCache;
    std::vector<FDNAMeshInfo> MeshInfoCache;

    std::unordered_map<std::string, int> JointNameToIndexMap;
    std::unordered_map<std::string, int> BlendShapeNameToIndexMap;
    std::unordered_map<std::string, int> MeshNameToIndexMap;
};

// ============================================================================
// Mock Body Schema for Motor Control Testing
// ============================================================================

struct FBodySchema {
    std::unordered_map<std::string, FVector3> BodyPartPositions;
    std::unordered_map<std::string, FRotator> BodyPartOrientations;
    std::unordered_map<std::string, float> JointAngles;
    float PeripersonalRadius = 100.0f;
    float SchemaCoherence = 1.0f;
};

struct FProprioceptiveState {
    std::string BodyPart;
    FVector3 Position;
    FRotator Orientation;
    FVector3 AngularVelocity;
    FVector3 LinearVelocity;
    float MuscleTension = 0.0f;
    float Fatigue = 0.0f;
    bool bIsInContact = false;
    float ContactForce = 0.0f;
};

struct FSensorimotorContingency {
    std::string Action;
    std::string ExpectedOutcome;
    std::string ActualOutcome;
    float PredictionError = 0.0f;
};

// Mock Motor Control Component
class MockMotorControlBinding {
public:
    struct FJointBinding {
        std::string DNAJointName;
        std::string BodySchemaPart;
        int SkeletonBoneIndex = -1;
        FRotator RotationMin = FRotator(-180, -180, -180);
        FRotator RotationMax = FRotator(180, 180, 180);
    };

    void Initialize() {
        bInitialized = true;
        SetupDefaultBindings();
    }

    void SyncFromBodySchema(const FBodySchema& schema) {
        if (!bInitialized) return;

        for (const auto& [jointName, binding] : JointBindings) {
            // Check for target orientation
            auto orientIt = schema.BodyPartOrientations.find(binding.BodySchemaPart);
            auto angleIt = schema.JointAngles.find(binding.DNAJointName);

            FRotator predictedOrientation = FRotator::ZeroRotator();

            if (orientIt != schema.BodyPartOrientations.end()) {
                // Clamp to joint limits
                FRotator target = orientIt->second;
                predictedOrientation.Roll = std::clamp(target.Roll, binding.RotationMin.Roll, binding.RotationMax.Roll);
                predictedOrientation.Pitch = std::clamp(target.Pitch, binding.RotationMin.Pitch, binding.RotationMax.Pitch);
                predictedOrientation.Yaw = std::clamp(target.Yaw, binding.RotationMin.Yaw, binding.RotationMax.Yaw);
            }

            if (angleIt != schema.JointAngles.end()) {
                float angle = angleIt->second;
                // Apply based on joint type
                if (jointName.find("arm") != std::string::npos || jointName.find("calf") != std::string::npos) {
                    predictedOrientation.Pitch = std::clamp(angle, binding.RotationMin.Pitch, binding.RotationMax.Pitch);
                } else if (jointName.find("spine") != std::string::npos || jointName.find("neck") != std::string::npos) {
                    predictedOrientation.Yaw = std::clamp(angle * 0.5f, binding.RotationMin.Yaw, binding.RotationMax.Yaw);
                } else if (jointName.find("head") != std::string::npos) {
                    predictedOrientation.Yaw = std::clamp(angle, binding.RotationMin.Yaw, binding.RotationMax.Yaw);
                }
            }

            // Update proprioceptive state
            FProprioceptiveState& propState = ProprioceptiveStates[jointName];
            propState.BodyPart = binding.BodySchemaPart;

            // Update muscle tension
            if (orientIt != schema.BodyPartOrientations.end() || angleIt != schema.JointAngles.end()) {
                propState.MuscleTension = std::min(propState.MuscleTension + 0.1f, 1.0f);
                if (propState.MuscleTension > 0.5f) {
                    propState.Fatigue = std::min(propState.Fatigue + 0.01f, 1.0f);
                }
            } else {
                propState.MuscleTension = std::max(propState.MuscleTension - 0.05f, 0.0f);
            }

            // Record sensorimotor contingency
            if (orientIt != schema.BodyPartOrientations.end() || angleIt != schema.JointAngles.end()) {
                FSensorimotorContingency contingency;
                contingency.Action = "Move_" + binding.BodySchemaPart;
                contingency.ExpectedOutcome = "Orientation:" + std::to_string(predictedOrientation.Yaw);
                contingency.ActualOutcome = "Orientation:" + std::to_string(propState.Orientation.Yaw);
                LearnedContingencies.push_back(contingency);
            }
        }

        bSyncedFromBodySchema = true;
    }

    bool IsSynced() const { return bSyncedFromBodySchema; }

    FProprioceptiveState GetProprioceptiveState(const std::string& jointName) const {
        auto it = ProprioceptiveStates.find(jointName);
        if (it != ProprioceptiveStates.end()) return it->second;
        return FProprioceptiveState();
    }

    size_t GetLearnedContingencyCount() const { return LearnedContingencies.size(); }

    void AddJointBinding(const std::string& jointName, const FJointBinding& binding) {
        JointBindings[jointName] = binding;
    }

    size_t GetBindingCount() const { return JointBindings.size(); }

private:
    void SetupDefaultBindings() {
        // Head binding with rotation limits
        FJointBinding headBinding;
        headBinding.DNAJointName = "head";
        headBinding.BodySchemaPart = "Head";
        headBinding.SkeletonBoneIndex = 7;
        headBinding.RotationMin = FRotator(-45, -80, -45);
        headBinding.RotationMax = FRotator(45, 80, 35);
        JointBindings["head"] = headBinding;

        // Neck binding
        FJointBinding neckBinding;
        neckBinding.DNAJointName = "neck_01";
        neckBinding.BodySchemaPart = "Neck";
        neckBinding.SkeletonBoneIndex = 5;
        neckBinding.RotationMin = FRotator(-30, -45, -20);
        neckBinding.RotationMax = FRotator(30, 45, 20);
        JointBindings["neck_01"] = neckBinding;

        // Spine binding
        FJointBinding spineBinding;
        spineBinding.DNAJointName = "spine_01";
        spineBinding.BodySchemaPart = "Spine";
        spineBinding.SkeletonBoneIndex = 2;
        spineBinding.RotationMin = FRotator(-20, -30, -15);
        spineBinding.RotationMax = FRotator(20, 30, 15);
        JointBindings["spine_01"] = spineBinding;

        // Arm bindings
        FJointBinding armBinding;
        armBinding.DNAJointName = "upperarm_l";
        armBinding.BodySchemaPart = "LeftArm";
        armBinding.SkeletonBoneIndex = 9;
        armBinding.RotationMin = FRotator(-90, -180, -90);
        armBinding.RotationMax = FRotator(90, 60, 90);
        JointBindings["upperarm_l"] = armBinding;
    }

    bool bInitialized = false;
    bool bSyncedFromBodySchema = false;
    std::unordered_map<std::string, FJointBinding> JointBindings;
    std::unordered_map<std::string, FProprioceptiveState> ProprioceptiveStates;
    std::vector<FSensorimotorContingency> LearnedContingencies;
};

// ============================================================================
// Mock Cognitive Visualization Component
// ============================================================================

struct FNeuralNode {
    FVector3 Position;
    float Activation = 0.0f;
    FLinearColor Color = FLinearColor::White();
    float Size = 5.0f;
    int Layer = 0;
};

struct FNeuralConnection {
    int FromNodeIndex = 0;
    int ToNodeIndex = 0;
    float Weight = 1.0f;
    float ActivityLevel = 0.0f;
};

struct FThoughtParticle {
    FVector3 Position;
    FVector3 Velocity;
    FLinearColor Color = FLinearColor::White();
    float Lifetime = 1.0f;
    float Size = 3.0f;
};

class MockCognitiveVisualizationComponent {
public:
    void Initialize() {
        bInitialized = true;
        InitializeNeuralNetwork();
    }

    void SetVisualizationIntensity(float intensity) {
        VisualizationIntensity = std::clamp(intensity, 0.0f, 2.0f);

        for (auto& node : NeuralNodes) {
            node.Activation *= VisualizationIntensity;
            node.Size = 5.0f + node.Activation * 5.0f * VisualizationIntensity;
        }
    }

    float GetVisualizationIntensity() const { return VisualizationIntensity; }

    void TriggerResonanceEffect(float intensity, float duration) {
        bResonanceActive = true;
        ResonanceIntensity = std::clamp(intensity, 0.0f, 2.0f);
        ResonanceDuration = duration;
        ResonanceTimer = 0.0f;

        // Activate neural nodes with resonance pattern
        for (size_t i = 0; i < NeuralNodes.size(); i++) {
            float distance = std::sin(i * 0.5f) * 0.5f + 0.5f;
            NeuralNodes[i].Activation = ResonanceIntensity * distance;
            NeuralNodes[i].Color = FLinearColor::LerpUsingHSV(
                FLinearColor(0.3f, 0.7f, 1.0f, 1.0f),  // Cyan
                FLinearColor(0.7f, 0.3f, 1.0f, 1.0f),  // Purple
                distance
            );
        }

        // Activate connections
        for (auto& conn : NeuralConnections) {
            conn.ActivityLevel = ResonanceIntensity * 0.8f;
        }

        // Generate burst particles
        int burstCount = (int)(50 * ResonanceIntensity);
        for (int i = 0; i < burstCount; i++) {
            FThoughtParticle particle;
            float angle = (float)i / burstCount * 2.0f * 3.14159f;
            particle.Position = FVector3(0, 0, 200);
            particle.Velocity = FVector3(std::cos(angle), std::sin(angle), 0) * 100.0f * ResonanceIntensity;
            particle.Color = FLinearColor(0.5f, 0.8f, 1.0f, 1.0f);
            particle.Lifetime = duration + (float)(rand() % 100) / 200.0f;
            ThoughtParticles.push_back(particle);
        }
    }

    bool IsResonanceActive() const { return bResonanceActive; }
    float GetResonanceIntensity() const { return ResonanceIntensity; }

    void VisualizeMemoryConstellation(const std::vector<FVector3>& memoryNodes) {
        MemoryConstellationNodes = memoryNodes;

        // Create particles at each node
        for (const auto& nodePos : memoryNodes) {
            for (int i = 0; i < 5; i++) {
                FThoughtParticle particle;
                particle.Position = nodePos;
                particle.Velocity = FVector3(
                    (float)(rand() % 40 - 20),
                    (float)(rand() % 40 - 20),
                    (float)(rand() % 20 - 10)
                );
                particle.Color = FLinearColor(0.8f, 0.9f, 1.0f, 1.0f);
                particle.Lifetime = 2.0f + (float)(rand() % 200) / 100.0f;
                ThoughtParticles.push_back(particle);
            }
        }
    }

    size_t GetMemoryConstellationNodeCount() const { return MemoryConstellationNodes.size(); }
    size_t GetNeuralNodeCount() const { return NeuralNodes.size(); }
    size_t GetParticleCount() const { return ThoughtParticles.size(); }
    size_t GetConnectionCount() const { return NeuralConnections.size(); }

    const std::vector<FNeuralNode>& GetNeuralNodes() const { return NeuralNodes; }

private:
    void InitializeNeuralNetwork() {
        // Create multi-layer neural network for visualization
        int nodesPerLayer = 8;
        int layers = 4;

        for (int layer = 0; layer < layers; layer++) {
            for (int i = 0; i < nodesPerLayer; i++) {
                FNeuralNode node;
                float angle = (float)i / nodesPerLayer * 2.0f * 3.14159f;
                float radius = 30.0f + layer * 20.0f;
                node.Position = FVector3(std::cos(angle) * radius, std::sin(angle) * radius, layer * 10.0f);
                node.Layer = layer;
                node.Size = 5.0f;
                NeuralNodes.push_back(node);
            }
        }

        // Create connections between layers
        for (int layer = 0; layer < layers - 1; layer++) {
            for (int i = 0; i < nodesPerLayer; i++) {
                for (int j = 0; j < nodesPerLayer; j++) {
                    if (rand() % 3 == 0) { // Sparse connections
                        FNeuralConnection conn;
                        conn.FromNodeIndex = layer * nodesPerLayer + i;
                        conn.ToNodeIndex = (layer + 1) * nodesPerLayer + j;
                        conn.Weight = (float)(rand() % 100) / 100.0f;
                        NeuralConnections.push_back(conn);
                    }
                }
            }
        }
    }

    bool bInitialized = false;
    float VisualizationIntensity = 1.0f;
    bool bResonanceActive = false;
    float ResonanceIntensity = 0.0f;
    float ResonanceDuration = 0.0f;
    float ResonanceTimer = 0.0f;

    std::vector<FNeuralNode> NeuralNodes;
    std::vector<FNeuralConnection> NeuralConnections;
    std::vector<FThoughtParticle> ThoughtParticles;
    std::vector<FVector3> MemoryConstellationNodes;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class PythonDNACalibWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        Wrapper = std::make_unique<MockPythonDNACalibWrapper>();
        Wrapper->Initialize("/path/to/dnacalib");
        Wrapper->LoadDNA("/path/to/test.dna");
    }

    void TearDown() override {
        Wrapper.reset();
    }

    std::unique_ptr<MockPythonDNACalibWrapper> Wrapper;
};

class MetaHumanDNABridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        Bridge = std::make_unique<MockMetaHumanDNABridge>();
        Bridge->Initialize("/path/to/dnacalib");
        Bridge->LoadDNA("/path/to/test.dna");
    }

    void TearDown() override {
        Bridge.reset();
    }

    std::unique_ptr<MockMetaHumanDNABridge> Bridge;
};

class MotorControlBindingTest : public ::testing::Test {
protected:
    void SetUp() override {
        MotorControl = std::make_unique<MockMotorControlBinding>();
        MotorControl->Initialize();
    }

    void TearDown() override {
        MotorControl.reset();
    }

    std::unique_ptr<MockMotorControlBinding> MotorControl;
};

class CognitiveVisualizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        Visualization = std::make_unique<MockCognitiveVisualizationComponent>();
        Visualization->Initialize();
    }

    void TearDown() override {
        Visualization.reset();
    }

    std::unique_ptr<MockCognitiveVisualizationComponent> Visualization;
};

// ============================================================================
// PythonDNACalibWrapper Tests
// ============================================================================

TEST_F(PythonDNACalibWrapperTest, Initialization) {
    EXPECT_TRUE(Wrapper->IsInitialized());
}

TEST_F(PythonDNACalibWrapperTest, LoadDNA) {
    EXPECT_GT(Wrapper->GetJointCount(), 0);
    EXPECT_GT(Wrapper->GetBlendShapeCount(), 0);
    EXPECT_GT(Wrapper->GetMeshCount(), 0);
}

TEST_F(PythonDNACalibWrapperTest, GetJointNames) {
    auto names = Wrapper->GetJointNames();
    EXPECT_GT(names.size(), 0);
    EXPECT_TRUE(std::find(names.begin(), names.end(), "head") != names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "pelvis") != names.end());
}

TEST_F(PythonDNACalibWrapperTest, GetBlendShapeNames) {
    auto names = Wrapper->GetBlendShapeNames();
    EXPECT_GT(names.size(), 0);
    EXPECT_TRUE(std::find(names.begin(), names.end(), "mouth_smile_L") != names.end());
}

TEST_F(PythonDNACalibWrapperTest, RenameBlendShape) {
    int initialCount = Wrapper->GetBlendShapeCount();
    EXPECT_TRUE(Wrapper->RenameBlendShape("mouth_smile_L", "mouth_smile_left"));

    auto names = Wrapper->GetBlendShapeNames();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "mouth_smile_left") != names.end());
    EXPECT_FALSE(std::find(names.begin(), names.end(), "mouth_smile_L") != names.end());
    EXPECT_EQ(Wrapper->GetBlendShapeCount(), initialCount);
}

TEST_F(PythonDNACalibWrapperTest, RemoveBlendShape) {
    int initialCount = Wrapper->GetBlendShapeCount();
    EXPECT_TRUE(Wrapper->RemoveBlendShape("mouth_smile_L"));
    EXPECT_EQ(Wrapper->GetBlendShapeCount(), initialCount - 1);

    auto names = Wrapper->GetBlendShapeNames();
    EXPECT_FALSE(std::find(names.begin(), names.end(), "mouth_smile_L") != names.end());
}

TEST_F(PythonDNACalibWrapperTest, ModifyBlendShapeDeltas) {
    std::vector<FVector3> deltas = {
        FVector3(1, 0, 0), FVector3(0, 1, 0), FVector3(0, 0, 1)
    };
    EXPECT_TRUE(Wrapper->ModifyBlendShapeDeltas("mouth_smile_L", "head_lod0", deltas));
}

TEST_F(PythonDNACalibWrapperTest, ModifyBlendShapeDeltasEmptyFails) {
    std::vector<FVector3> emptyDeltas;
    EXPECT_FALSE(Wrapper->ModifyBlendShapeDeltas("mouth_smile_L", "head_lod0", emptyDeltas));
    EXPECT_EQ(Wrapper->GetLastError(), "No deltas provided");
}

TEST_F(PythonDNACalibWrapperTest, ClearBlendShapes) {
    EXPECT_GT(Wrapper->GetBlendShapeCount(), 0);
    EXPECT_TRUE(Wrapper->ClearBlendShapes());
    EXPECT_EQ(Wrapper->GetBlendShapeCount(), 0);
}

TEST_F(PythonDNACalibWrapperTest, RemoveLOD) {
    int initialMeshCount = Wrapper->GetMeshCount();
    EXPECT_TRUE(Wrapper->RemoveLOD(1));

    auto meshNames = Wrapper->GetMeshNames();
    for (const auto& name : meshNames) {
        EXPECT_FALSE(name.find("_lod1") != std::string::npos);
    }
}

TEST_F(PythonDNACalibWrapperTest, RemoveLODInvalidIndex) {
    EXPECT_FALSE(Wrapper->RemoveLOD(-1));
    EXPECT_FALSE(Wrapper->RemoveLOD(8));
}

TEST_F(PythonDNACalibWrapperTest, GetJointNeutralTransform) {
    FTransform transform;
    EXPECT_TRUE(Wrapper->GetJointNeutralTransform("pelvis", transform));
    EXPECT_FLOAT_EQ(transform.Location.Z, 100.0f);
}

TEST_F(PythonDNACalibWrapperTest, GetJointParentIndex) {
    EXPECT_EQ(Wrapper->GetJointParentIndex("root"), -1);
    EXPECT_EQ(Wrapper->GetJointParentIndex("pelvis"), 0);
    EXPECT_EQ(Wrapper->GetJointParentIndex("spine_01"), 1);
}

TEST_F(PythonDNACalibWrapperTest, GetBlendShapeTargetMesh) {
    EXPECT_EQ(Wrapper->GetBlendShapeTargetMesh("brow_down_L"), "head_lod0");
    EXPECT_EQ(Wrapper->GetBlendShapeTargetMesh("mouth_smile_L"), "head_lod0");
}

TEST_F(PythonDNACalibWrapperTest, GetBlendShapeVertexCount) {
    EXPECT_EQ(Wrapper->GetBlendShapeVertexCount("brow_down_L"), 450);
    EXPECT_EQ(Wrapper->GetBlendShapeVertexCount("jaw_open"), 1500);
    EXPECT_EQ(Wrapper->GetBlendShapeVertexCount("unknown_shape"), 500); // Default
}

TEST_F(PythonDNACalibWrapperTest, GetDNADatabaseName) {
    EXPECT_EQ(Wrapper->GetDNADatabaseName(), "DHI");
}

// ============================================================================
// MetaHumanDNABridge Tests
// ============================================================================

TEST_F(MetaHumanDNABridgeTest, Initialization) {
    EXPECT_TRUE(Bridge->IsDNALoaded());
}

TEST_F(MetaHumanDNABridgeTest, DNAVersionDetectionDHI) {
    EXPECT_EQ(Bridge->GetDNAVersion(), EDNAVersion::DHI);
}

TEST_F(MetaHumanDNABridgeTest, DNAVersionDetectionMH4) {
    MockMetaHumanDNABridge bridge;
    bridge.Initialize("/path/to/dnacalib");
    bridge.SetDatabaseNameForTesting("MH.4");
    bridge.LoadDNA("/path/to/test.dna");
    EXPECT_EQ(bridge.GetDNAVersion(), EDNAVersion::MH4);
}

TEST_F(MetaHumanDNABridgeTest, GetJointInfo) {
    FDNAJointInfo info;
    EXPECT_TRUE(Bridge->GetJointInfo("pelvis", info));
    EXPECT_EQ(info.JointName, "pelvis");
    EXPECT_EQ(info.ParentIndex, 0);
    EXPECT_FLOAT_EQ(info.NeutralTransform.Location.Z, 100.0f);
}

TEST_F(MetaHumanDNABridgeTest, GetBlendShapeInfo) {
    FDNABlendShapeInfo info;
    EXPECT_TRUE(Bridge->GetBlendShapeInfo("brow_down_L", info));
    EXPECT_EQ(info.BlendShapeName, "brow_down_L");
    EXPECT_EQ(info.TargetMeshName, "head_lod0");
    EXPECT_EQ(info.VertexCount, 450);
}

TEST_F(MetaHumanDNABridgeTest, RenameBlendShape) {
    int initialCount = Bridge->GetBlendShapeCount();
    EXPECT_TRUE(Bridge->RenameBlendShape("mouth_smile_L", "mouth_smile_left"));
    EXPECT_EQ(Bridge->GetBlendShapeCount(), initialCount);

    FDNABlendShapeInfo info;
    EXPECT_TRUE(Bridge->GetBlendShapeInfo("mouth_smile_left", info));
    EXPECT_FALSE(Bridge->GetBlendShapeInfo("mouth_smile_L", info));
}

TEST_F(MetaHumanDNABridgeTest, RenameBlendShapeNonexistent) {
    EXPECT_FALSE(Bridge->RenameBlendShape("nonexistent", "newname"));
}

TEST_F(MetaHumanDNABridgeTest, RenameBlendShapeConflict) {
    EXPECT_FALSE(Bridge->RenameBlendShape("mouth_smile_L", "mouth_smile_R"));
}

TEST_F(MetaHumanDNABridgeTest, RemoveBlendShape) {
    int initialCount = Bridge->GetBlendShapeCount();
    EXPECT_TRUE(Bridge->RemoveBlendShape("mouth_smile_L"));
    EXPECT_EQ(Bridge->GetBlendShapeCount(), initialCount - 1);
}

TEST_F(MetaHumanDNABridgeTest, ModifyBlendShapeDeltas) {
    std::vector<FVector3> deltas(100, FVector3(1, 0, 0));
    EXPECT_TRUE(Bridge->ModifyBlendShapeDeltas("mouth_smile_L", deltas));

    FDNABlendShapeInfo info;
    Bridge->GetBlendShapeInfo("mouth_smile_L", info);
    EXPECT_EQ(info.VertexCount, 100);
}

TEST_F(MetaHumanDNABridgeTest, ClearAllBlendShapes) {
    EXPECT_GT(Bridge->GetBlendShapeCount(), 0);
    EXPECT_TRUE(Bridge->ClearAllBlendShapes());
    EXPECT_EQ(Bridge->GetBlendShapeCount(), 0);
}

TEST_F(MetaHumanDNABridgeTest, RemoveLOD) {
    int initialCount = Bridge->GetMeshCount();
    EXPECT_TRUE(Bridge->RemoveLOD(1));
    EXPECT_LT(Bridge->GetMeshCount(), initialCount);
}

TEST_F(MetaHumanDNABridgeTest, RemoveLODInvalidIndex) {
    EXPECT_FALSE(Bridge->RemoveLOD(-1));
    EXPECT_FALSE(Bridge->RemoveLOD(8));
}

// ============================================================================
// Motor Control Binding Tests (SyncFromBodySchema)
// ============================================================================

TEST_F(MotorControlBindingTest, Initialization) {
    EXPECT_GT(MotorControl->GetBindingCount(), 0);
}

TEST_F(MotorControlBindingTest, SyncFromBodySchemaWithOrientation) {
    FBodySchema schema;
    schema.BodyPartOrientations["Head"] = FRotator(0, 30, 0);

    MotorControl->SyncFromBodySchema(schema);

    EXPECT_TRUE(MotorControl->IsSynced());
    auto state = MotorControl->GetProprioceptiveState("head");
    EXPECT_GT(state.MuscleTension, 0.0f);
}

TEST_F(MotorControlBindingTest, SyncFromBodySchemaWithJointAngle) {
    FBodySchema schema;
    schema.JointAngles["head"] = 45.0f;

    MotorControl->SyncFromBodySchema(schema);

    EXPECT_TRUE(MotorControl->IsSynced());
    EXPECT_GT(MotorControl->GetLearnedContingencyCount(), 0);
}

TEST_F(MotorControlBindingTest, RotationClamping) {
    FBodySchema schema;
    // Try to set rotation beyond limits (head yaw max is 80)
    schema.BodyPartOrientations["Head"] = FRotator(0, 100, 0);

    MotorControl->SyncFromBodySchema(schema);

    // The rotation should be clamped - muscle tension still increases
    auto state = MotorControl->GetProprioceptiveState("head");
    EXPECT_GT(state.MuscleTension, 0.0f);
}

TEST_F(MotorControlBindingTest, MuscleTensionIncrease) {
    FBodySchema schema;
    schema.BodyPartOrientations["Head"] = FRotator(0, 20, 0);

    // Multiple syncs should increase muscle tension
    MotorControl->SyncFromBodySchema(schema);
    auto state1 = MotorControl->GetProprioceptiveState("head");

    MotorControl->SyncFromBodySchema(schema);
    auto state2 = MotorControl->GetProprioceptiveState("head");

    EXPECT_GT(state2.MuscleTension, state1.MuscleTension);
}

TEST_F(MotorControlBindingTest, FatigueAccumulation) {
    FBodySchema schema;
    schema.BodyPartOrientations["Head"] = FRotator(0, 20, 0);

    // Many syncs to accumulate fatigue
    for (int i = 0; i < 100; i++) {
        MotorControl->SyncFromBodySchema(schema);
    }

    auto state = MotorControl->GetProprioceptiveState("head");
    EXPECT_GT(state.Fatigue, 0.0f);
}

TEST_F(MotorControlBindingTest, SensorimotorLearning) {
    FBodySchema schema;
    schema.BodyPartOrientations["Head"] = FRotator(0, 30, 0);
    schema.BodyPartOrientations["Neck"] = FRotator(0, 15, 0);

    MotorControl->SyncFromBodySchema(schema);

    EXPECT_GT(MotorControl->GetLearnedContingencyCount(), 0);
}

TEST_F(MotorControlBindingTest, SpineJointAngleMapping) {
    FBodySchema schema;
    schema.JointAngles["spine_01"] = 30.0f;

    MotorControl->SyncFromBodySchema(schema);

    auto state = MotorControl->GetProprioceptiveState("spine_01");
    EXPECT_GT(state.MuscleTension, 0.0f);
}

TEST_F(MotorControlBindingTest, ArmJointAngleMapping) {
    FBodySchema schema;
    schema.JointAngles["upperarm_l"] = 45.0f;

    MotorControl->SyncFromBodySchema(schema);

    auto state = MotorControl->GetProprioceptiveState("upperarm_l");
    EXPECT_GT(state.MuscleTension, 0.0f);
}

// ============================================================================
// Cognitive Visualization Tests
// ============================================================================

TEST_F(CognitiveVisualizationTest, Initialization) {
    EXPECT_GT(Visualization->GetNeuralNodeCount(), 0);
    EXPECT_GT(Visualization->GetConnectionCount(), 0);
}

TEST_F(CognitiveVisualizationTest, SetVisualizationIntensity) {
    Visualization->SetVisualizationIntensity(1.5f);
    EXPECT_FLOAT_EQ(Visualization->GetVisualizationIntensity(), 1.5f);
}

TEST_F(CognitiveVisualizationTest, SetVisualizationIntensityClamping) {
    Visualization->SetVisualizationIntensity(3.0f);
    EXPECT_FLOAT_EQ(Visualization->GetVisualizationIntensity(), 2.0f);

    Visualization->SetVisualizationIntensity(-1.0f);
    EXPECT_FLOAT_EQ(Visualization->GetVisualizationIntensity(), 0.0f);
}

TEST_F(CognitiveVisualizationTest, TriggerResonanceEffect) {
    Visualization->TriggerResonanceEffect(1.0f, 2.0f);

    EXPECT_TRUE(Visualization->IsResonanceActive());
    EXPECT_FLOAT_EQ(Visualization->GetResonanceIntensity(), 1.0f);
    EXPECT_GT(Visualization->GetParticleCount(), 0);
}

TEST_F(CognitiveVisualizationTest, TriggerResonanceEffectIntensityClamping) {
    Visualization->TriggerResonanceEffect(3.0f, 1.0f);
    EXPECT_FLOAT_EQ(Visualization->GetResonanceIntensity(), 2.0f);
}

TEST_F(CognitiveVisualizationTest, ResonanceNodeActivation) {
    Visualization->TriggerResonanceEffect(1.0f, 2.0f);

    const auto& nodes = Visualization->GetNeuralNodes();
    bool hasActivation = false;
    for (const auto& node : nodes) {
        if (node.Activation > 0.0f) {
            hasActivation = true;
            break;
        }
    }
    EXPECT_TRUE(hasActivation);
}

TEST_F(CognitiveVisualizationTest, ResonanceNodeColorChange) {
    Visualization->TriggerResonanceEffect(1.0f, 2.0f);

    const auto& nodes = Visualization->GetNeuralNodes();
    bool hasColorChange = false;
    for (const auto& node : nodes) {
        if (node.Color.R != 1.0f || node.Color.G != 1.0f || node.Color.B != 1.0f) {
            hasColorChange = true;
            break;
        }
    }
    EXPECT_TRUE(hasColorChange);
}

TEST_F(CognitiveVisualizationTest, VisualizeMemoryConstellation) {
    std::vector<FVector3> memoryNodes = {
        FVector3(0, 0, 100),
        FVector3(50, 0, 150),
        FVector3(-50, 50, 120)
    };

    Visualization->VisualizeMemoryConstellation(memoryNodes);

    EXPECT_EQ(Visualization->GetMemoryConstellationNodeCount(), 3);
    EXPECT_GT(Visualization->GetParticleCount(), 0);
}

TEST_F(CognitiveVisualizationTest, MemoryConstellationParticleGeneration) {
    std::vector<FVector3> memoryNodes = {
        FVector3(0, 0, 100),
        FVector3(50, 0, 150)
    };

    size_t initialParticles = Visualization->GetParticleCount();
    Visualization->VisualizeMemoryConstellation(memoryNodes);

    // Should generate particles for each node (5 per node)
    EXPECT_GE(Visualization->GetParticleCount(), initialParticles + 10);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(DNABridgePerformanceTest, BlendShapeOperationPerformance) {
    MockMetaHumanDNABridge bridge;
    bridge.Initialize("/path/to/dnacalib");
    bridge.LoadDNA("/path/to/test.dna");

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<FVector3> deltas(1000, FVector3(1, 0, 0));
    for (int i = 0; i < 100; i++) {
        bridge.ModifyBlendShapeDeltas("mouth_smile_L", deltas);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100); // 100 operations in under 100ms
}

TEST(MotorControlPerformanceTest, SyncPerformance) {
    MockMotorControlBinding motorControl;
    motorControl.Initialize();

    FBodySchema schema;
    schema.BodyPartOrientations["Head"] = FRotator(0, 30, 0);
    schema.BodyPartOrientations["Neck"] = FRotator(0, 15, 0);
    schema.JointAngles["head"] = 45.0f;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        motorControl.SyncFromBodySchema(schema);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100); // 1000 syncs in under 100ms
}

TEST(VisualizationPerformanceTest, ResonanceEffectPerformance) {
    MockCognitiveVisualizationComponent viz;
    viz.Initialize();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        viz.TriggerResonanceEffect(1.0f, 2.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 50); // 100 effects in under 50ms
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(EdgeCaseTest, WrapperOperationsWithoutLoad) {
    MockPythonDNACalibWrapper wrapper;
    wrapper.Initialize("/path/to/dnacalib");
    // Don't load DNA

    EXPECT_FALSE(wrapper.RenameBlendShape("test", "test2"));
    EXPECT_FALSE(wrapper.RemoveBlendShape("test"));
    EXPECT_FALSE(wrapper.ClearBlendShapes());

    FTransform transform;
    EXPECT_FALSE(wrapper.GetJointNeutralTransform("test", transform));
    EXPECT_EQ(wrapper.GetJointParentIndex("test"), -1);
}

TEST(EdgeCaseTest, BridgeOperationsWithoutLoad) {
    MockMetaHumanDNABridge bridge;
    bridge.Initialize("/path/to/dnacalib");
    // Don't load DNA

    EXPECT_FALSE(bridge.RenameBlendShape("test", "test2"));
    EXPECT_FALSE(bridge.RemoveBlendShape("test"));
    EXPECT_FALSE(bridge.ClearAllBlendShapes());
    EXPECT_FALSE(bridge.RemoveLOD(0));
}

TEST(EdgeCaseTest, EmptyBodySchema) {
    MockMotorControlBinding motorControl;
    motorControl.Initialize();

    FBodySchema emptySchema;
    motorControl.SyncFromBodySchema(emptySchema);

    EXPECT_TRUE(motorControl.IsSynced());
    EXPECT_EQ(motorControl.GetLearnedContingencyCount(), 0);
}

TEST(EdgeCaseTest, EmptyMemoryConstellation) {
    MockCognitiveVisualizationComponent viz;
    viz.Initialize();

    std::vector<FVector3> emptyNodes;
    viz.VisualizeMemoryConstellation(emptyNodes);

    EXPECT_EQ(viz.GetMemoryConstellationNodeCount(), 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(IntegrationTest, DNAToBodySchemaFlow) {
    // Load DNA
    MockMetaHumanDNABridge bridge;
    bridge.Initialize("/path/to/dnacalib");
    bridge.LoadDNA("/path/to/test.dna");

    // Get joint info
    FDNAJointInfo headInfo;
    EXPECT_TRUE(bridge.GetJointInfo("head", headInfo));

    // Create body schema with that joint
    FBodySchema schema;
    schema.BodyPartOrientations["Head"] = FRotator(0, headInfo.NeutralTransform.Location.Z * 0.1f, 0);

    // Sync to motor control
    MockMotorControlBinding motorControl;
    motorControl.Initialize();
    motorControl.SyncFromBodySchema(schema);

    EXPECT_TRUE(motorControl.IsSynced());
}

TEST(IntegrationTest, FullVisualizationPipeline) {
    MockCognitiveVisualizationComponent viz;
    viz.Initialize();

    // Set intensity
    viz.SetVisualizationIntensity(1.5f);

    // Trigger resonance
    viz.TriggerResonanceEffect(0.8f, 3.0f);

    // Add memory constellation
    std::vector<FVector3> memories = {
        FVector3(0, 0, 100),
        FVector3(100, 0, 100)
    };
    viz.VisualizeMemoryConstellation(memories);

    // Verify all systems active
    EXPECT_TRUE(viz.IsResonanceActive());
    EXPECT_EQ(viz.GetMemoryConstellationNodeCount(), 2);
    EXPECT_GT(viz.GetParticleCount(), 0);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
