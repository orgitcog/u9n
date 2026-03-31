/**
 * @file AvatarCognitionTests.cpp
 * @brief Comprehensive unit tests for Avatar and 4E Cognition systems
 * 
 * Tests cover:
 * - EmbodiedAvatarComponent lifecycle
 * - UnrealAvatarCognition processing
 * - DNABodySchemaBinding integration
 * - EmbodiedCognitionComponent 4E framework
 * - ExpressiveAnimationSystem emotion mapping
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <array>
#include <cmath>
#include <random>

namespace {

// ============================================================================
// Mock Types for Avatar System
// ============================================================================

struct FVector3 {
    float X = 0.0f, Y = 0.0f, Z = 0.0f;
    
    FVector3() = default;
    FVector3(float x, float y, float z) : X(x), Y(y), Z(z) {}
    
    float Length() const { return std::sqrt(X*X + Y*Y + Z*Z); }
    FVector3 Normalized() const {
        float len = Length();
        if (len > 0.0001f) return FVector3(X/len, Y/len, Z/len);
        return *this;
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
};

struct FQuat4 {
    float X = 0.0f, Y = 0.0f, Z = 0.0f, W = 1.0f;
    
    FQuat4() = default;
    FQuat4(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}
    
    static FQuat4 Identity() { return FQuat4(0, 0, 0, 1); }
    
    FQuat4 operator*(const FQuat4& other) const {
        return FQuat4(
            W * other.X + X * other.W + Y * other.Z - Z * other.Y,
            W * other.Y - X * other.Z + Y * other.W + Z * other.X,
            W * other.Z + X * other.Y - Y * other.X + Z * other.W,
            W * other.W - X * other.X - Y * other.Y - Z * other.Z
        );
    }
};

struct FTransform {
    FVector3 Location;
    FQuat4 Rotation;
    FVector3 Scale = FVector3(1, 1, 1);
};

struct FBoneTransform {
    std::string BoneName;
    FTransform Transform;
    int ParentIndex = -1;
};

struct FEmotionState {
    float Joy = 0.0f;
    float Sadness = 0.0f;
    float Anger = 0.0f;
    float Fear = 0.0f;
    float Surprise = 0.0f;
    float Disgust = 0.0f;
    float Contempt = 0.0f;
    
    float GetDominantIntensity() const {
        return std::max({Joy, Sadness, Anger, Fear, Surprise, Disgust, Contempt});
    }
    
    std::string GetDominantEmotion() const {
        float maxVal = Joy;
        std::string dominant = "Joy";
        if (Sadness > maxVal) { maxVal = Sadness; dominant = "Sadness"; }
        if (Anger > maxVal) { maxVal = Anger; dominant = "Anger"; }
        if (Fear > maxVal) { maxVal = Fear; dominant = "Fear"; }
        if (Surprise > maxVal) { maxVal = Surprise; dominant = "Surprise"; }
        if (Disgust > maxVal) { maxVal = Disgust; dominant = "Disgust"; }
        if (Contempt > maxVal) { maxVal = Contempt; dominant = "Contempt"; }
        return dominant;
    }
    
    void Normalize() {
        float total = Joy + Sadness + Anger + Fear + Surprise + Disgust + Contempt;
        if (total > 0.0001f) {
            Joy /= total;
            Sadness /= total;
            Anger /= total;
            Fear /= total;
            Surprise /= total;
            Disgust /= total;
            Contempt /= total;
        }
    }
};

struct FBodySchema {
    std::vector<FBoneTransform> Skeleton;
    std::unordered_map<std::string, int> BoneNameToIndex;
    FVector3 CenterOfMass;
    float TotalMass = 70.0f;
    
    int GetBoneIndex(const std::string& name) const {
        auto it = BoneNameToIndex.find(name);
        return (it != BoneNameToIndex.end()) ? it->second : -1;
    }
};

struct FSensorimotorState {
    std::vector<float> ProprioceptiveState;  // Joint angles
    std::vector<float> ExteroceptiveState;   // External sensors
    std::vector<float> InteroceptiveState;   // Internal body state
    FVector3 Velocity;
    FVector3 Acceleration;
    
    FSensorimotorState() : 
        ProprioceptiveState(64, 0.0f),
        ExteroceptiveState(128, 0.0f),
        InteroceptiveState(32, 0.0f) {}
};

// ============================================================================
// Mock Embodied Avatar Component
// ============================================================================

class MockEmbodiedAvatarComponent {
public:
    enum class EAvatarState {
        Uninitialized,
        Initializing,
        Ready,
        Active,
        Paused,
        Error
    };
    
    void Initialize(const FBodySchema& schema) {
        BodySchema = schema;
        CurrentState = EAvatarState::Initializing;
        
        // Initialize sensorimotor state
        SensorimotorState.ProprioceptiveState.resize(schema.Skeleton.size() * 3);
        
        CurrentState = EAvatarState::Ready;
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    EAvatarState GetState() const { return CurrentState; }
    
    void Activate() {
        if (CurrentState == EAvatarState::Ready || CurrentState == EAvatarState::Paused) {
            CurrentState = EAvatarState::Active;
        }
    }
    
    void Pause() {
        if (CurrentState == EAvatarState::Active) {
            CurrentState = EAvatarState::Paused;
        }
    }
    
    void UpdateSensorimotorState(const FSensorimotorState& state) {
        if (CurrentState != EAvatarState::Active) return;
        SensorimotorState = state;
        ProcessProprioception();
    }
    
    void SetBoneTransform(const std::string& boneName, const FTransform& transform) {
        int index = BodySchema.GetBoneIndex(boneName);
        if (index >= 0 && index < (int)BodySchema.Skeleton.size()) {
            BodySchema.Skeleton[index].Transform = transform;
        }
    }
    
    FTransform GetBoneTransform(const std::string& boneName) const {
        int index = BodySchema.GetBoneIndex(boneName);
        if (index >= 0 && index < (int)BodySchema.Skeleton.size()) {
            return BodySchema.Skeleton[index].Transform;
        }
        return FTransform();
    }
    
    void SetEmotionState(const FEmotionState& emotion) {
        CurrentEmotion = emotion;
        ApplyEmotionToPosture();
    }
    
    FEmotionState GetEmotionState() const { return CurrentEmotion; }
    
    FVector3 GetCenterOfMass() const { return BodySchema.CenterOfMass; }
    
    const FSensorimotorState& GetSensorimotorState() const { return SensorimotorState; }
    
    int GetBoneCount() const { return (int)BodySchema.Skeleton.size(); }
    
private:
    void ProcessProprioception() {
        // Update center of mass based on bone positions
        FVector3 com(0, 0, 0);
        for (const auto& bone : BodySchema.Skeleton) {
            com = com + bone.Transform.Location;
        }
        if (!BodySchema.Skeleton.empty()) {
            com = com * (1.0f / BodySchema.Skeleton.size());
        }
        BodySchema.CenterOfMass = com;
    }
    
    void ApplyEmotionToPosture() {
        // Simplified emotion-to-posture mapping
        // Joy: upright, open posture
        // Sadness: slouched, closed posture
        float postureModifier = CurrentEmotion.Joy - CurrentEmotion.Sadness;
        
        // Apply to spine bones
        for (auto& bone : BodySchema.Skeleton) {
            if (bone.BoneName.find("spine") != std::string::npos) {
                bone.Transform.Location.Z += postureModifier * 2.0f;
            }
        }
    }
    
    bool bInitialized = false;
    EAvatarState CurrentState = EAvatarState::Uninitialized;
    FBodySchema BodySchema;
    FSensorimotorState SensorimotorState;
    FEmotionState CurrentEmotion;
};

// ============================================================================
// Mock 4E Cognition Component
// ============================================================================

class Mock4ECognitionComponent {
public:
    // 4E Cognition: Embodied, Embedded, Enacted, Extended
    struct F4EState {
        // Embodied: body-based cognition
        float BodyAwareness = 0.5f;
        float ProprioceptiveAccuracy = 0.8f;
        float InteroceptiveSensitivity = 0.6f;
        
        // Embedded: environmental coupling
        float EnvironmentalCoupling = 0.5f;
        float SituatedAwareness = 0.5f;
        float AffordanceDetection = 0.5f;
        
        // Enacted: action-based cognition
        float SensorimotorEngagement = 0.5f;
        float ActionReadiness = 0.5f;
        float SkillFluency = 0.5f;
        
        // Extended: cognitive extension
        float ToolIntegration = 0.0f;
        float SocialCognition = 0.5f;
        float DistributedProcessing = 0.0f;
    };
    
    void Initialize() {
        bInitialized = true;
        State4E = F4EState();
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void ProcessEmbodiedCognition(const FSensorimotorState& sensorimotor) {
        // Update embodied state based on sensorimotor input
        float proprioSum = 0.0f;
        for (float val : sensorimotor.ProprioceptiveState) {
            proprioSum += std::abs(val);
        }
        State4E.BodyAwareness = std::min(1.0f, proprioSum / sensorimotor.ProprioceptiveState.size());
        
        // Interoceptive processing
        float interoSum = 0.0f;
        for (float val : sensorimotor.InteroceptiveState) {
            interoSum += std::abs(val);
        }
        State4E.InteroceptiveSensitivity = std::min(1.0f, interoSum / sensorimotor.InteroceptiveState.size());
    }
    
    void ProcessEmbeddedCognition(const std::vector<float>& environmentState) {
        // Analyze environmental affordances
        float affordanceSum = 0.0f;
        for (float val : environmentState) {
            if (val > 0.5f) affordanceSum += 1.0f;
        }
        State4E.AffordanceDetection = std::min(1.0f, affordanceSum / std::max(1.0f, (float)environmentState.size()));
        State4E.EnvironmentalCoupling = 0.5f + 0.5f * State4E.AffordanceDetection;
    }
    
    void ProcessEnactedCognition(const std::vector<float>& actionHistory) {
        // Analyze action patterns for skill fluency
        if (actionHistory.empty()) return;
        
        float consistency = 0.0f;
        for (size_t i = 1; i < actionHistory.size(); i++) {
            consistency += 1.0f - std::abs(actionHistory[i] - actionHistory[i-1]);
        }
        State4E.SkillFluency = consistency / std::max(1.0f, (float)(actionHistory.size() - 1));
        State4E.SensorimotorEngagement = 0.5f + 0.5f * State4E.SkillFluency;
    }
    
    void ProcessExtendedCognition(bool hasToolInHand, int socialAgentCount) {
        State4E.ToolIntegration = hasToolInHand ? 1.0f : 0.0f;
        State4E.SocialCognition = std::min(1.0f, (float)socialAgentCount * 0.2f);
        State4E.DistributedProcessing = (State4E.ToolIntegration + State4E.SocialCognition) / 2.0f;
    }
    
    F4EState GetState() const { return State4E; }
    
    float GetOverall4EScore() const {
        return (State4E.BodyAwareness + State4E.EnvironmentalCoupling + 
                State4E.SensorimotorEngagement + State4E.DistributedProcessing) / 4.0f;
    }
    
private:
    bool bInitialized = false;
    F4EState State4E;
};

// ============================================================================
// Mock DNA Body Schema Binding
// ============================================================================

class MockDNABodySchemaBinding {
public:
    struct FDNAMapping {
        std::string DNAControlName;
        std::string BoneName;
        float Weight = 1.0f;
        FVector3 Axis = FVector3(1, 0, 0);
    };
    
    void Initialize(const FBodySchema& schema) {
        BodySchema = schema;
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void AddDNAMapping(const FDNAMapping& mapping) {
        DNAMappings[mapping.DNAControlName] = mapping;
    }
    
    void ApplyDNAControl(const std::string& controlName, float value) {
        auto it = DNAMappings.find(controlName);
        if (it == DNAMappings.end()) return;
        
        const FDNAMapping& mapping = it->second;
        int boneIndex = BodySchema.GetBoneIndex(mapping.BoneName);
        if (boneIndex < 0) return;
        
        // Apply weighted transformation
        FVector3 delta = mapping.Axis * (value * mapping.Weight);
        BodySchema.Skeleton[boneIndex].Transform.Location = 
            BodySchema.Skeleton[boneIndex].Transform.Location + delta;
        
        AppliedControls[controlName] = value;
    }
    
    float GetAppliedControlValue(const std::string& controlName) const {
        auto it = AppliedControls.find(controlName);
        return (it != AppliedControls.end()) ? it->second : 0.0f;
    }
    
    void ApplyBlendShape(const std::string& shapeName, float weight) {
        BlendShapeWeights[shapeName] = std::clamp(weight, 0.0f, 1.0f);
    }
    
    float GetBlendShapeWeight(const std::string& shapeName) const {
        auto it = BlendShapeWeights.find(shapeName);
        return (it != BlendShapeWeights.end()) ? it->second : 0.0f;
    }
    
    void ResetToBindPose() {
        AppliedControls.clear();
        BlendShapeWeights.clear();
        // Reset skeleton to bind pose (simplified)
        for (auto& bone : BodySchema.Skeleton) {
            bone.Transform.Location = FVector3(0, 0, 0);
            bone.Transform.Rotation = FQuat4::Identity();
        }
    }
    
    size_t GetMappingCount() const { return DNAMappings.size(); }
    
private:
    bool bInitialized = false;
    FBodySchema BodySchema;
    std::unordered_map<std::string, FDNAMapping> DNAMappings;
    std::unordered_map<std::string, float> AppliedControls;
    std::unordered_map<std::string, float> BlendShapeWeights;
};

// ============================================================================
// Mock Expressive Animation System
// ============================================================================

class MockExpressiveAnimationSystem {
public:
    struct FExpressionPose {
        std::string Name;
        std::unordered_map<std::string, float> BlendShapes;
        FEmotionState AssociatedEmotion;
    };
    
    void Initialize() {
        bInitialized = true;
        SetupDefaultExpressions();
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void RegisterExpression(const FExpressionPose& pose) {
        Expressions[pose.Name] = pose;
    }
    
    std::unordered_map<std::string, float> ComputeBlendShapes(const FEmotionState& emotion) {
        std::unordered_map<std::string, float> result;
        
        // Blend expressions based on emotion weights
        for (const auto& [name, pose] : Expressions) {
            float weight = GetEmotionWeight(emotion, pose.AssociatedEmotion);
            for (const auto& [shapeName, shapeWeight] : pose.BlendShapes) {
                result[shapeName] += shapeWeight * weight;
            }
        }
        
        // Clamp all values
        for (auto& [name, weight] : result) {
            weight = std::clamp(weight, 0.0f, 1.0f);
        }
        
        return result;
    }
    
    void SetBlendSpeed(float speed) {
        BlendSpeed = std::clamp(speed, 0.1f, 10.0f);
    }
    
    float GetBlendSpeed() const { return BlendSpeed; }
    
    std::unordered_map<std::string, float> InterpolateBlendShapes(
        const std::unordered_map<std::string, float>& from,
        const std::unordered_map<std::string, float>& to,
        float alpha) {
        
        std::unordered_map<std::string, float> result;
        
        // Get all keys
        std::set<std::string> allKeys;
        for (const auto& [k, v] : from) allKeys.insert(k);
        for (const auto& [k, v] : to) allKeys.insert(k);
        
        for (const auto& key : allKeys) {
            float fromVal = 0.0f, toVal = 0.0f;
            auto itFrom = from.find(key);
            auto itTo = to.find(key);
            if (itFrom != from.end()) fromVal = itFrom->second;
            if (itTo != to.end()) toVal = itTo->second;
            
            result[key] = fromVal + (toVal - fromVal) * alpha;
        }
        
        return result;
    }
    
    size_t GetExpressionCount() const { return Expressions.size(); }
    
private:
    void SetupDefaultExpressions() {
        // Joy expression
        FExpressionPose joy;
        joy.Name = "Joy";
        joy.BlendShapes["smile"] = 1.0f;
        joy.BlendShapes["eyeWiden"] = 0.3f;
        joy.AssociatedEmotion.Joy = 1.0f;
        Expressions["Joy"] = joy;
        
        // Sadness expression
        FExpressionPose sadness;
        sadness.Name = "Sadness";
        sadness.BlendShapes["frown"] = 1.0f;
        sadness.BlendShapes["browDown"] = 0.5f;
        sadness.AssociatedEmotion.Sadness = 1.0f;
        Expressions["Sadness"] = sadness;
        
        // Anger expression
        FExpressionPose anger;
        anger.Name = "Anger";
        anger.BlendShapes["browFurrow"] = 1.0f;
        anger.BlendShapes["jawClench"] = 0.7f;
        anger.AssociatedEmotion.Anger = 1.0f;
        Expressions["Anger"] = anger;
        
        // Surprise expression
        FExpressionPose surprise;
        surprise.Name = "Surprise";
        surprise.BlendShapes["eyeWiden"] = 1.0f;
        surprise.BlendShapes["browRaise"] = 1.0f;
        surprise.BlendShapes["jawDrop"] = 0.5f;
        surprise.AssociatedEmotion.Surprise = 1.0f;
        Expressions["Surprise"] = surprise;
    }
    
    float GetEmotionWeight(const FEmotionState& current, const FEmotionState& target) {
        // Compute similarity between emotion states
        float similarity = 0.0f;
        similarity += current.Joy * target.Joy;
        similarity += current.Sadness * target.Sadness;
        similarity += current.Anger * target.Anger;
        similarity += current.Fear * target.Fear;
        similarity += current.Surprise * target.Surprise;
        similarity += current.Disgust * target.Disgust;
        similarity += current.Contempt * target.Contempt;
        return similarity;
    }
    
    bool bInitialized = false;
    float BlendSpeed = 1.0f;
    std::unordered_map<std::string, FExpressionPose> Expressions;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class EmbodiedAvatarTest : public ::testing::Test {
protected:
    void SetUp() override {
        Avatar = std::make_unique<MockEmbodiedAvatarComponent>();
        
        // Create test body schema
        FBodySchema schema;
        std::vector<std::string> boneNames = {
            "root", "pelvis", "spine_01", "spine_02", "spine_03",
            "neck", "head", "clavicle_l", "upperarm_l", "lowerarm_l",
            "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
            "thigh_l", "calf_l", "foot_l", "thigh_r", "calf_r", "foot_r"
        };
        
        for (size_t i = 0; i < boneNames.size(); i++) {
            FBoneTransform bone;
            bone.BoneName = boneNames[i];
            bone.ParentIndex = (i > 0) ? (int)(i - 1) : -1;
            schema.Skeleton.push_back(bone);
            schema.BoneNameToIndex[boneNames[i]] = (int)i;
        }
        
        Avatar->Initialize(schema);
    }
    
    void TearDown() override {
        Avatar.reset();
    }
    
    std::unique_ptr<MockEmbodiedAvatarComponent> Avatar;
};

class Cognition4ETest : public ::testing::Test {
protected:
    void SetUp() override {
        Cognition = std::make_unique<Mock4ECognitionComponent>();
        Cognition->Initialize();
    }
    
    void TearDown() override {
        Cognition.reset();
    }
    
    std::unique_ptr<Mock4ECognitionComponent> Cognition;
};

class DNABindingTest : public ::testing::Test {
protected:
    void SetUp() override {
        Binding = std::make_unique<MockDNABodySchemaBinding>();
        
        FBodySchema schema;
        std::vector<std::string> boneNames = {"head", "jaw", "eyeL", "eyeR"};
        for (size_t i = 0; i < boneNames.size(); i++) {
            FBoneTransform bone;
            bone.BoneName = boneNames[i];
            schema.Skeleton.push_back(bone);
            schema.BoneNameToIndex[boneNames[i]] = (int)i;
        }
        
        Binding->Initialize(schema);
    }
    
    void TearDown() override {
        Binding.reset();
    }
    
    std::unique_ptr<MockDNABodySchemaBinding> Binding;
};

class ExpressiveAnimationTest : public ::testing::Test {
protected:
    void SetUp() override {
        Animation = std::make_unique<MockExpressiveAnimationSystem>();
        Animation->Initialize();
    }
    
    void TearDown() override {
        Animation.reset();
    }
    
    std::unique_ptr<MockExpressiveAnimationSystem> Animation;
};

// ============================================================================
// Embodied Avatar Tests
// ============================================================================

TEST_F(EmbodiedAvatarTest, Initialization) {
    EXPECT_TRUE(Avatar->IsInitialized());
    EXPECT_EQ(Avatar->GetState(), MockEmbodiedAvatarComponent::EAvatarState::Ready);
    EXPECT_EQ(Avatar->GetBoneCount(), 21);
}

TEST_F(EmbodiedAvatarTest, StateTransitions) {
    EXPECT_EQ(Avatar->GetState(), MockEmbodiedAvatarComponent::EAvatarState::Ready);
    
    Avatar->Activate();
    EXPECT_EQ(Avatar->GetState(), MockEmbodiedAvatarComponent::EAvatarState::Active);
    
    Avatar->Pause();
    EXPECT_EQ(Avatar->GetState(), MockEmbodiedAvatarComponent::EAvatarState::Paused);
    
    Avatar->Activate();
    EXPECT_EQ(Avatar->GetState(), MockEmbodiedAvatarComponent::EAvatarState::Active);
}

TEST_F(EmbodiedAvatarTest, BoneTransformAccess) {
    FTransform transform;
    transform.Location = FVector3(10, 20, 30);
    
    Avatar->SetBoneTransform("head", transform);
    FTransform retrieved = Avatar->GetBoneTransform("head");
    
    EXPECT_FLOAT_EQ(retrieved.Location.X, 10.0f);
    EXPECT_FLOAT_EQ(retrieved.Location.Y, 20.0f);
    EXPECT_FLOAT_EQ(retrieved.Location.Z, 30.0f);
}

TEST_F(EmbodiedAvatarTest, SensorimotorUpdate) {
    Avatar->Activate();
    
    FSensorimotorState state;
    state.Velocity = FVector3(1, 0, 0);
    state.Acceleration = FVector3(0.1f, 0, 0);
    
    Avatar->UpdateSensorimotorState(state);
    
    const FSensorimotorState& retrieved = Avatar->GetSensorimotorState();
    EXPECT_FLOAT_EQ(retrieved.Velocity.X, 1.0f);
}

TEST_F(EmbodiedAvatarTest, EmotionStateManagement) {
    FEmotionState emotion;
    emotion.Joy = 0.8f;
    emotion.Sadness = 0.1f;
    
    Avatar->SetEmotionState(emotion);
    FEmotionState retrieved = Avatar->GetEmotionState();
    
    EXPECT_FLOAT_EQ(retrieved.Joy, 0.8f);
    EXPECT_FLOAT_EQ(retrieved.Sadness, 0.1f);
}

TEST_F(EmbodiedAvatarTest, CenterOfMassCalculation) {
    Avatar->Activate();
    
    // Set some bone positions
    FTransform t1, t2;
    t1.Location = FVector3(0, 0, 100);
    t2.Location = FVector3(0, 0, 50);
    
    Avatar->SetBoneTransform("head", t1);
    Avatar->SetBoneTransform("pelvis", t2);
    
    FSensorimotorState state;
    Avatar->UpdateSensorimotorState(state);
    
    FVector3 com = Avatar->GetCenterOfMass();
    // COM should be somewhere between the bones
    EXPECT_GE(com.Z, 0.0f);
}

// ============================================================================
// 4E Cognition Tests
// ============================================================================

TEST_F(Cognition4ETest, Initialization) {
    EXPECT_TRUE(Cognition->IsInitialized());
    EXPECT_GT(Cognition->GetOverall4EScore(), 0.0f);
}

TEST_F(Cognition4ETest, EmbodiedCognitionProcessing) {
    FSensorimotorState state;
    for (auto& val : state.ProprioceptiveState) val = 0.5f;
    for (auto& val : state.InteroceptiveState) val = 0.3f;
    
    Cognition->ProcessEmbodiedCognition(state);
    
    auto result = Cognition->GetState();
    EXPECT_GT(result.BodyAwareness, 0.0f);
    EXPECT_GT(result.InteroceptiveSensitivity, 0.0f);
}

TEST_F(Cognition4ETest, EmbeddedCognitionProcessing) {
    std::vector<float> environment(50, 0.7f);
    
    Cognition->ProcessEmbeddedCognition(environment);
    
    auto result = Cognition->GetState();
    EXPECT_GT(result.AffordanceDetection, 0.0f);
    EXPECT_GT(result.EnvironmentalCoupling, 0.5f);
}

TEST_F(Cognition4ETest, EnactedCognitionProcessing) {
    std::vector<float> actionHistory = {0.5f, 0.52f, 0.51f, 0.53f, 0.52f};
    
    Cognition->ProcessEnactedCognition(actionHistory);
    
    auto result = Cognition->GetState();
    EXPECT_GT(result.SkillFluency, 0.8f);  // High consistency
}

TEST_F(Cognition4ETest, ExtendedCognitionProcessing) {
    Cognition->ProcessExtendedCognition(true, 3);
    
    auto result = Cognition->GetState();
    EXPECT_FLOAT_EQ(result.ToolIntegration, 1.0f);
    EXPECT_GT(result.SocialCognition, 0.0f);
}

TEST_F(Cognition4ETest, Overall4EScore) {
    FSensorimotorState sensorimotor;
    for (auto& val : sensorimotor.ProprioceptiveState) val = 0.8f;
    
    Cognition->ProcessEmbodiedCognition(sensorimotor);
    Cognition->ProcessEmbeddedCognition(std::vector<float>(20, 0.6f));
    Cognition->ProcessEnactedCognition({0.5f, 0.5f, 0.5f});
    Cognition->ProcessExtendedCognition(true, 2);
    
    float score = Cognition->GetOverall4EScore();
    EXPECT_GT(score, 0.3f);
    EXPECT_LE(score, 1.0f);
}

// ============================================================================
// DNA Binding Tests
// ============================================================================

TEST_F(DNABindingTest, Initialization) {
    EXPECT_TRUE(Binding->IsInitialized());
    EXPECT_EQ(Binding->GetMappingCount(), 0);
}

TEST_F(DNABindingTest, AddAndApplyMapping) {
    MockDNABodySchemaBinding::FDNAMapping mapping;
    mapping.DNAControlName = "jawOpen";
    mapping.BoneName = "jaw";
    mapping.Weight = 1.0f;
    mapping.Axis = FVector3(0, 0, -1);
    
    Binding->AddDNAMapping(mapping);
    EXPECT_EQ(Binding->GetMappingCount(), 1);
    
    Binding->ApplyDNAControl("jawOpen", 0.5f);
    EXPECT_FLOAT_EQ(Binding->GetAppliedControlValue("jawOpen"), 0.5f);
}

TEST_F(DNABindingTest, BlendShapeApplication) {
    Binding->ApplyBlendShape("smile", 0.8f);
    EXPECT_FLOAT_EQ(Binding->GetBlendShapeWeight("smile"), 0.8f);
    
    // Test clamping
    Binding->ApplyBlendShape("frown", 1.5f);
    EXPECT_FLOAT_EQ(Binding->GetBlendShapeWeight("frown"), 1.0f);
}

TEST_F(DNABindingTest, ResetToBindPose) {
    Binding->ApplyBlendShape("smile", 0.8f);
    Binding->ResetToBindPose();
    
    EXPECT_FLOAT_EQ(Binding->GetBlendShapeWeight("smile"), 0.0f);
}

// ============================================================================
// Expressive Animation Tests
// ============================================================================

TEST_F(ExpressiveAnimationTest, Initialization) {
    EXPECT_TRUE(Animation->IsInitialized());
    EXPECT_EQ(Animation->GetExpressionCount(), 4);  // Default expressions
}

TEST_F(ExpressiveAnimationTest, ComputeBlendShapesForJoy) {
    FEmotionState emotion;
    emotion.Joy = 1.0f;
    
    auto blendShapes = Animation->ComputeBlendShapes(emotion);
    
    EXPECT_GT(blendShapes["smile"], 0.0f);
}

TEST_F(ExpressiveAnimationTest, ComputeBlendShapesForMixedEmotion) {
    FEmotionState emotion;
    emotion.Joy = 0.5f;
    emotion.Surprise = 0.5f;
    
    auto blendShapes = Animation->ComputeBlendShapes(emotion);
    
    // Should have contributions from both expressions
    EXPECT_GT(blendShapes["smile"], 0.0f);
    EXPECT_GT(blendShapes["eyeWiden"], 0.0f);
}

TEST_F(ExpressiveAnimationTest, BlendSpeedConfiguration) {
    Animation->SetBlendSpeed(2.0f);
    EXPECT_FLOAT_EQ(Animation->GetBlendSpeed(), 2.0f);
    
    // Test clamping
    Animation->SetBlendSpeed(20.0f);
    EXPECT_FLOAT_EQ(Animation->GetBlendSpeed(), 10.0f);
}

TEST_F(ExpressiveAnimationTest, BlendShapeInterpolation) {
    std::unordered_map<std::string, float> from = {{"smile", 0.0f}, {"frown", 1.0f}};
    std::unordered_map<std::string, float> to = {{"smile", 1.0f}, {"frown", 0.0f}};
    
    auto mid = Animation->InterpolateBlendShapes(from, to, 0.5f);
    
    EXPECT_FLOAT_EQ(mid["smile"], 0.5f);
    EXPECT_FLOAT_EQ(mid["frown"], 0.5f);
}

// ============================================================================
// Emotion State Tests
// ============================================================================

TEST(EmotionStateTest, DefaultConstruction) {
    FEmotionState state;
    EXPECT_FLOAT_EQ(state.Joy, 0.0f);
    EXPECT_FLOAT_EQ(state.GetDominantIntensity(), 0.0f);
}

TEST(EmotionStateTest, DominantEmotionDetection) {
    FEmotionState state;
    state.Joy = 0.3f;
    state.Anger = 0.8f;
    state.Fear = 0.2f;
    
    EXPECT_EQ(state.GetDominantEmotion(), "Anger");
    EXPECT_FLOAT_EQ(state.GetDominantIntensity(), 0.8f);
}

TEST(EmotionStateTest, Normalization) {
    FEmotionState state;
    state.Joy = 0.5f;
    state.Sadness = 0.5f;
    
    state.Normalize();
    
    EXPECT_FLOAT_EQ(state.Joy + state.Sadness, 1.0f);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(AvatarPerformanceTest, BoneUpdatePerformance) {
    MockEmbodiedAvatarComponent avatar;
    
    FBodySchema schema;
    for (int i = 0; i < 100; i++) {
        FBoneTransform bone;
        bone.BoneName = "bone_" + std::to_string(i);
        schema.Skeleton.push_back(bone);
        schema.BoneNameToIndex[bone.BoneName] = i;
    }
    avatar.Initialize(schema);
    avatar.Activate();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int frame = 0; frame < 1000; frame++) {
        FSensorimotorState state;
        avatar.UpdateSensorimotorState(state);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 100);  // 1000 updates in under 100ms
}

TEST(AnimationPerformanceTest, BlendShapeComputePerformance) {
    MockExpressiveAnimationSystem animation;
    animation.Initialize();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    for (int i = 0; i < 1000; i++) {
        FEmotionState emotion;
        emotion.Joy = dist(gen);
        emotion.Sadness = dist(gen);
        emotion.Anger = dist(gen);
        animation.ComputeBlendShapes(emotion);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 50);  // 1000 computations in under 50ms
}

} // namespace
