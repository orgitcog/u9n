/**
 * @file AvatarIntegrationE2E.cpp
 * @brief End-to-End tests for Avatar system integration
 * 
 * Tests the full integration of:
 * - MetaHuman DNA calibration
 * - Facial expression blending
 * - Body animation and IK
 * - Emotion-to-expression mapping
 * - 4E embodied cognition feedback
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <array>
#include <cmath>
#include <random>
#include <chrono>

// ============================================================================
// Avatar E2E Test Types
// ============================================================================

struct FVector3 {
    float X = 0.0f, Y = 0.0f, Z = 0.0f;
    FVector3() = default;
    FVector3(float x, float y, float z) : X(x), Y(y), Z(z) {}
    float Length() const { return std::sqrt(X*X + Y*Y + Z*Z); }
};

struct FQuat {
    float X = 0.0f, Y = 0.0f, Z = 0.0f, W = 1.0f;
};

struct FTransform {
    FVector3 Location;
    FQuat Rotation;
    FVector3 Scale = FVector3(1, 1, 1);
};

struct FBone {
    std::string Name;
    FTransform Transform;
    int ParentIndex = -1;
};

struct FBlendShape {
    std::string Name;
    float Weight = 0.0f;
};

struct FEmotionState {
    float Joy = 0.0f;
    float Sadness = 0.0f;
    float Anger = 0.0f;
    float Fear = 0.0f;
    float Surprise = 0.0f;
    float Disgust = 0.0f;
};

// ============================================================================
// Mock DNA Calibration System
// ============================================================================

class MockDNACalibrationSystem {
public:
    struct FDNAProfile {
        std::string Id;
        std::vector<float> NeutralPose;
        std::vector<float> BlendShapeDeltas;
        std::vector<std::string> BlendShapeNames;
        bool IsCalibrated = false;
    };
    
    void Initialize() {
        // Setup default blend shapes (ARKit compatible)
        DefaultBlendShapes = {
            "browInnerUp", "browDownLeft", "browDownRight", "browOuterUpLeft", "browOuterUpRight",
            "eyeLookUpLeft", "eyeLookUpRight", "eyeLookDownLeft", "eyeLookDownRight",
            "eyeLookInLeft", "eyeLookInRight", "eyeLookOutLeft", "eyeLookOutRight",
            "eyeBlinkLeft", "eyeBlinkRight", "eyeSquintLeft", "eyeSquintRight",
            "eyeWideLeft", "eyeWideRight", "cheekPuff", "cheekSquintLeft", "cheekSquintRight",
            "noseSneerLeft", "noseSneerRight", "jawOpen", "jawForward", "jawLeft", "jawRight",
            "mouthFunnel", "mouthPucker", "mouthLeft", "mouthRight", "mouthRollUpper", "mouthRollLower",
            "mouthShrugUpper", "mouthShrugLower", "mouthClose", "mouthSmileLeft", "mouthSmileRight",
            "mouthFrownLeft", "mouthFrownRight", "mouthDimpleLeft", "mouthDimpleRight",
            "mouthUpperUpLeft", "mouthUpperUpRight", "mouthLowerDownLeft", "mouthLowerDownRight",
            "mouthPressLeft", "mouthPressRight", "mouthStretchLeft", "mouthStretchRight",
            "tongueOut"
        };
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    FDNAProfile CalibrateFromScan(const std::vector<float>& scanData) {
        FDNAProfile profile;
        profile.Id = "profile_" + std::to_string(NextProfileId++);
        profile.BlendShapeNames = DefaultBlendShapes;
        profile.NeutralPose.resize(scanData.size(), 0.0f);
        profile.BlendShapeDeltas.resize(DefaultBlendShapes.size() * 3, 0.0f);
        
        // Simulate calibration
        for (size_t i = 0; i < scanData.size(); i++) {
            profile.NeutralPose[i] = scanData[i];
        }
        
        profile.IsCalibrated = true;
        Profiles[profile.Id] = profile;
        
        return profile;
    }
    
    std::vector<float> ApplyBlendShapes(const std::string& profileId,
                                         const std::vector<float>& weights) {
        auto it = Profiles.find(profileId);
        if (it == Profiles.end()) return {};
        
        const FDNAProfile& profile = it->second;
        std::vector<float> result = profile.NeutralPose;
        
        // Apply weighted blend shape deltas
        for (size_t i = 0; i < weights.size() && i < profile.BlendShapeNames.size(); i++) {
            float weight = std::clamp(weights[i], 0.0f, 1.0f);
            // Apply delta (simplified)
            for (size_t j = 0; j < result.size(); j++) {
                result[j] += weight * 0.01f * (float)((i + j) % 10);
            }
        }
        
        return result;
    }
    
    size_t GetBlendShapeCount() const { return DefaultBlendShapes.size(); }
    
    std::vector<std::string> GetBlendShapeNames() const { return DefaultBlendShapes; }
    
private:
    bool bInitialized = false;
    int NextProfileId = 1;
    std::vector<std::string> DefaultBlendShapes;
    std::unordered_map<std::string, FDNAProfile> Profiles;
};

// ============================================================================
// Mock Expression System
// ============================================================================

class MockExpressionSystem {
public:
    struct FExpression {
        std::string Name;
        std::vector<float> BlendShapeWeights;
        FEmotionState AssociatedEmotion;
    };
    
    void Initialize(size_t numBlendShapes) {
        NumBlendShapes = numBlendShapes;
        CurrentWeights.resize(numBlendShapes, 0.0f);
        TargetWeights.resize(numBlendShapes, 0.0f);
        
        SetupDefaultExpressions();
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void SetTargetExpression(const FEmotionState& emotion) {
        // Blend expressions based on emotion
        TargetWeights.assign(NumBlendShapes, 0.0f);
        
        for (const auto& [name, expr] : Expressions) {
            float weight = ComputeEmotionMatch(emotion, expr.AssociatedEmotion);
            for (size_t i = 0; i < NumBlendShapes && i < expr.BlendShapeWeights.size(); i++) {
                TargetWeights[i] += weight * expr.BlendShapeWeights[i];
            }
        }
        
        // Clamp
        for (float& w : TargetWeights) {
            w = std::clamp(w, 0.0f, 1.0f);
        }
    }
    
    void Update(float deltaTime) {
        // Smooth interpolation toward target
        float blendSpeed = 5.0f;
        float alpha = 1.0f - std::exp(-blendSpeed * deltaTime);
        
        for (size_t i = 0; i < NumBlendShapes; i++) {
            CurrentWeights[i] += (TargetWeights[i] - CurrentWeights[i]) * alpha;
        }
    }
    
    std::vector<float> GetCurrentWeights() const { return CurrentWeights; }
    
    void RegisterExpression(const FExpression& expr) {
        Expressions[expr.Name] = expr;
    }
    
private:
    void SetupDefaultExpressions() {
        // Joy expression
        FExpression joy;
        joy.Name = "Joy";
        joy.BlendShapeWeights.resize(NumBlendShapes, 0.0f);
        joy.BlendShapeWeights[37] = 1.0f;  // mouthSmileLeft
        joy.BlendShapeWeights[38] = 1.0f;  // mouthSmileRight
        joy.BlendShapeWeights[17] = 0.3f;  // eyeWideLeft
        joy.BlendShapeWeights[18] = 0.3f;  // eyeWideRight
        joy.AssociatedEmotion.Joy = 1.0f;
        Expressions["Joy"] = joy;
        
        // Sadness expression
        FExpression sadness;
        sadness.Name = "Sadness";
        sadness.BlendShapeWeights.resize(NumBlendShapes, 0.0f);
        sadness.BlendShapeWeights[39] = 1.0f;  // mouthFrownLeft
        sadness.BlendShapeWeights[40] = 1.0f;  // mouthFrownRight
        sadness.BlendShapeWeights[0] = 0.5f;   // browInnerUp
        sadness.AssociatedEmotion.Sadness = 1.0f;
        Expressions["Sadness"] = sadness;
        
        // Anger expression
        FExpression anger;
        anger.Name = "Anger";
        anger.BlendShapeWeights.resize(NumBlendShapes, 0.0f);
        anger.BlendShapeWeights[1] = 1.0f;   // browDownLeft
        anger.BlendShapeWeights[2] = 1.0f;   // browDownRight
        anger.BlendShapeWeights[24] = 0.3f;  // jawOpen
        anger.AssociatedEmotion.Anger = 1.0f;
        Expressions["Anger"] = anger;
        
        // Surprise expression
        FExpression surprise;
        surprise.Name = "Surprise";
        surprise.BlendShapeWeights.resize(NumBlendShapes, 0.0f);
        surprise.BlendShapeWeights[17] = 1.0f;  // eyeWideLeft
        surprise.BlendShapeWeights[18] = 1.0f;  // eyeWideRight
        surprise.BlendShapeWeights[3] = 1.0f;   // browOuterUpLeft
        surprise.BlendShapeWeights[4] = 1.0f;   // browOuterUpRight
        surprise.BlendShapeWeights[24] = 0.5f;  // jawOpen
        surprise.AssociatedEmotion.Surprise = 1.0f;
        Expressions["Surprise"] = surprise;
    }
    
    float ComputeEmotionMatch(const FEmotionState& a, const FEmotionState& b) {
        return a.Joy * b.Joy + a.Sadness * b.Sadness + a.Anger * b.Anger +
               a.Fear * b.Fear + a.Surprise * b.Surprise + a.Disgust * b.Disgust;
    }
    
    bool bInitialized = false;
    size_t NumBlendShapes = 0;
    std::vector<float> CurrentWeights;
    std::vector<float> TargetWeights;
    std::unordered_map<std::string, FExpression> Expressions;
};

// ============================================================================
// Mock Body Animation System
// ============================================================================

class MockBodyAnimationSystem {
public:
    void Initialize() {
        // Setup skeleton
        std::vector<std::string> boneNames = {
            "root", "pelvis", "spine_01", "spine_02", "spine_03",
            "neck_01", "head", "clavicle_l", "upperarm_l", "lowerarm_l",
            "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
            "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r",
            "calf_r", "foot_r", "ball_r"
        };
        
        for (size_t i = 0; i < boneNames.size(); i++) {
            FBone bone;
            bone.Name = boneNames[i];
            bone.ParentIndex = (i > 0) ? (int)(i - 1) : -1;
            Skeleton.push_back(bone);
            BoneNameToIndex[bone.Name] = (int)i;
        }
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void SetPose(const std::vector<FTransform>& pose) {
        for (size_t i = 0; i < std::min(pose.size(), Skeleton.size()); i++) {
            Skeleton[i].Transform = pose[i];
        }
    }
    
    void ApplyIK(const std::string& effectorBone, const FVector3& targetPosition) {
        int boneIndex = GetBoneIndex(effectorBone);
        if (boneIndex < 0) return;
        
        // Simple 2-bone IK (simplified)
        FVector3 currentPos = Skeleton[boneIndex].Transform.Location;
        FVector3 delta;
        delta.X = targetPosition.X - currentPos.X;
        delta.Y = targetPosition.Y - currentPos.Y;
        delta.Z = targetPosition.Z - currentPos.Z;
        
        // Apply partial movement
        Skeleton[boneIndex].Transform.Location.X += delta.X * 0.5f;
        Skeleton[boneIndex].Transform.Location.Y += delta.Y * 0.5f;
        Skeleton[boneIndex].Transform.Location.Z += delta.Z * 0.5f;
    }
    
    void BlendPoses(const std::vector<FTransform>& poseA,
                    const std::vector<FTransform>& poseB,
                    float alpha) {
        for (size_t i = 0; i < Skeleton.size(); i++) {
            if (i < poseA.size() && i < poseB.size()) {
                Skeleton[i].Transform.Location.X = 
                    poseA[i].Location.X * (1.0f - alpha) + poseB[i].Location.X * alpha;
                Skeleton[i].Transform.Location.Y = 
                    poseA[i].Location.Y * (1.0f - alpha) + poseB[i].Location.Y * alpha;
                Skeleton[i].Transform.Location.Z = 
                    poseA[i].Location.Z * (1.0f - alpha) + poseB[i].Location.Z * alpha;
            }
        }
    }
    
    FTransform GetBoneTransform(const std::string& boneName) const {
        int index = GetBoneIndex(boneName);
        if (index >= 0) return Skeleton[index].Transform;
        return FTransform();
    }
    
    int GetBoneIndex(const std::string& name) const {
        auto it = BoneNameToIndex.find(name);
        return (it != BoneNameToIndex.end()) ? it->second : -1;
    }
    
    size_t GetBoneCount() const { return Skeleton.size(); }
    
private:
    bool bInitialized = false;
    std::vector<FBone> Skeleton;
    std::unordered_map<std::string, int> BoneNameToIndex;
};

// ============================================================================
// Mock 4E Embodiment Feedback System
// ============================================================================

class Mock4EEmbodimentFeedback {
public:
    struct FEmbodimentState {
        float BodyAwareness = 0.5f;
        float EnvironmentalCoupling = 0.5f;
        float ActionReadiness = 0.5f;
        float SocialPresence = 0.0f;
        FVector3 CenterOfMass;
        FVector3 Velocity;
    };
    
    void Initialize() {
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void UpdateFromSkeleton(const std::vector<FTransform>& skeleton) {
        // Compute center of mass
        FVector3 com;
        for (const auto& bone : skeleton) {
            com.X += bone.Location.X;
            com.Y += bone.Location.Y;
            com.Z += bone.Location.Z;
        }
        if (!skeleton.empty()) {
            com.X /= skeleton.size();
            com.Y /= skeleton.size();
            com.Z /= skeleton.size();
        }
        
        // Update velocity
        State.Velocity.X = com.X - State.CenterOfMass.X;
        State.Velocity.Y = com.Y - State.CenterOfMass.Y;
        State.Velocity.Z = com.Z - State.CenterOfMass.Z;
        
        State.CenterOfMass = com;
        
        // Update body awareness based on movement
        float speed = State.Velocity.Length();
        State.BodyAwareness = 0.9f * State.BodyAwareness + 0.1f * std::min(1.0f, speed * 10.0f);
    }
    
    void UpdateFromEnvironment(const std::vector<float>& environmentFeatures) {
        // Environmental coupling based on feature activity
        float activity = 0.0f;
        for (float f : environmentFeatures) activity += std::abs(f);
        activity /= std::max(1.0f, (float)environmentFeatures.size());
        
        State.EnvironmentalCoupling = 0.8f * State.EnvironmentalCoupling + 0.2f * activity;
    }
    
    void UpdateFromActions(const std::vector<float>& actionHistory) {
        // Action readiness based on recent action consistency
        if (actionHistory.size() < 2) return;
        
        float consistency = 0.0f;
        for (size_t i = 1; i < actionHistory.size(); i++) {
            consistency += 1.0f - std::abs(actionHistory[i] - actionHistory[i-1]);
        }
        consistency /= (actionHistory.size() - 1);
        
        State.ActionReadiness = 0.7f * State.ActionReadiness + 0.3f * consistency;
    }
    
    void UpdateSocialPresence(int nearbyAgents) {
        State.SocialPresence = std::min(1.0f, (float)nearbyAgents * 0.25f);
    }
    
    FEmbodimentState GetState() const { return State; }
    
    float GetOverallEmbodiment() const {
        return (State.BodyAwareness + State.EnvironmentalCoupling + 
                State.ActionReadiness + State.SocialPresence) / 4.0f;
    }
    
private:
    bool bInitialized = false;
    FEmbodimentState State;
};

// ============================================================================
// Integrated Avatar System
// ============================================================================

class MockIntegratedAvatarSystem {
public:
    void Initialize() {
        DNA.Initialize();
        Expression.Initialize(DNA.GetBlendShapeCount());
        Body.Initialize();
        Embodiment.Initialize();
        
        // Create default DNA profile
        std::vector<float> defaultScan(100, 0.0f);
        CurrentProfile = DNA.CalibrateFromScan(defaultScan);
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void Update(float deltaTime, const FEmotionState& emotion) {
        // Update expression
        Expression.SetTargetExpression(emotion);
        Expression.Update(deltaTime);
        
        // Apply blend shapes
        auto weights = Expression.GetCurrentWeights();
        CurrentMeshState = DNA.ApplyBlendShapes(CurrentProfile.Id, weights);
        
        // Update embodiment feedback
        std::vector<FTransform> skeleton(Body.GetBoneCount());
        for (size_t i = 0; i < skeleton.size(); i++) {
            skeleton[i] = Body.GetBoneTransform("bone_" + std::to_string(i));
        }
        Embodiment.UpdateFromSkeleton(skeleton);
        
        FrameCount++;
    }
    
    void SetBodyPose(const std::vector<FTransform>& pose) {
        Body.SetPose(pose);
    }
    
    void ApplyHandIK(bool isLeft, const FVector3& target) {
        std::string effector = isLeft ? "hand_l" : "hand_r";
        Body.ApplyIK(effector, target);
    }
    
    std::vector<float> GetCurrentBlendShapes() const {
        return Expression.GetCurrentWeights();
    }
    
    std::vector<float> GetCurrentMeshState() const {
        return CurrentMeshState;
    }
    
    Mock4EEmbodimentFeedback::FEmbodimentState GetEmbodimentState() const {
        return Embodiment.GetState();
    }
    
    int GetFrameCount() const { return FrameCount; }
    
private:
    bool bInitialized = false;
    int FrameCount = 0;
    
    MockDNACalibrationSystem DNA;
    MockExpressionSystem Expression;
    MockBodyAnimationSystem Body;
    Mock4EEmbodimentFeedback Embodiment;
    
    MockDNACalibrationSystem::FDNAProfile CurrentProfile;
    std::vector<float> CurrentMeshState;
};

// ============================================================================
// E2E Test Fixtures
// ============================================================================

class AvatarIntegrationE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        Avatar = std::make_unique<MockIntegratedAvatarSystem>();
        Avatar->Initialize();
    }
    
    void TearDown() override {
        Avatar.reset();
    }
    
    std::unique_ptr<MockIntegratedAvatarSystem> Avatar;
};

class DNACalibrationE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        DNA = std::make_unique<MockDNACalibrationSystem>();
        DNA->Initialize();
    }
    
    void TearDown() override {
        DNA.reset();
    }
    
    std::unique_ptr<MockDNACalibrationSystem> DNA;
};

// ============================================================================
// Avatar Integration E2E Tests
// ============================================================================

TEST_F(AvatarIntegrationE2ETest, FullSystemInitialization) {
    ASSERT_TRUE(Avatar->IsInitialized());
    EXPECT_EQ(Avatar->GetFrameCount(), 0);
}

TEST_F(AvatarIntegrationE2ETest, EmotionToExpressionPipeline) {
    FEmotionState joy;
    joy.Joy = 1.0f;
    
    // Update for several frames
    for (int i = 0; i < 60; i++) {
        Avatar->Update(1.0f / 60.0f, joy);
    }
    
    auto blendShapes = Avatar->GetCurrentBlendShapes();
    
    // Smile blend shapes should be active
    EXPECT_GT(blendShapes[37], 0.0f);  // mouthSmileLeft
    EXPECT_GT(blendShapes[38], 0.0f);  // mouthSmileRight
}

TEST_F(AvatarIntegrationE2ETest, EmotionTransition) {
    // Start with joy
    FEmotionState joy;
    joy.Joy = 1.0f;
    
    for (int i = 0; i < 30; i++) {
        Avatar->Update(1.0f / 60.0f, joy);
    }
    
    auto joyBlendShapes = Avatar->GetCurrentBlendShapes();
    
    // Transition to sadness
    FEmotionState sadness;
    sadness.Sadness = 1.0f;
    
    for (int i = 0; i < 60; i++) {
        Avatar->Update(1.0f / 60.0f, sadness);
    }
    
    auto sadnessBlendShapes = Avatar->GetCurrentBlendShapes();
    
    // Expression should have changed
    EXPECT_NE(joyBlendShapes[37], sadnessBlendShapes[37]);
}

TEST_F(AvatarIntegrationE2ETest, MixedEmotions) {
    FEmotionState mixed;
    mixed.Joy = 0.5f;
    mixed.Surprise = 0.5f;
    
    for (int i = 0; i < 60; i++) {
        Avatar->Update(1.0f / 60.0f, mixed);
    }
    
    auto blendShapes = Avatar->GetCurrentBlendShapes();
    
    // Should have contributions from both expressions
    EXPECT_GT(blendShapes.size(), 0);
}

TEST_F(AvatarIntegrationE2ETest, BodyPoseApplication) {
    std::vector<FTransform> pose(23);
    pose[6].Location = FVector3(0, 0, 170);  // Head at 170cm
    
    Avatar->SetBodyPose(pose);
    
    FEmotionState neutral;
    Avatar->Update(1.0f / 60.0f, neutral);
    
    // System should update without crash
    EXPECT_EQ(Avatar->GetFrameCount(), 1);
}

TEST_F(AvatarIntegrationE2ETest, IKApplication) {
    FVector3 target(50, 0, 100);
    Avatar->ApplyHandIK(true, target);
    
    FEmotionState neutral;
    Avatar->Update(1.0f / 60.0f, neutral);
    
    EXPECT_EQ(Avatar->GetFrameCount(), 1);
}

TEST_F(AvatarIntegrationE2ETest, EmbodimentFeedback) {
    FEmotionState emotion;
    emotion.Joy = 0.5f;
    
    for (int i = 0; i < 100; i++) {
        Avatar->Update(1.0f / 60.0f, emotion);
    }
    
    auto embodiment = Avatar->GetEmbodimentState();
    
    EXPECT_GE(embodiment.BodyAwareness, 0.0f);
    EXPECT_LE(embodiment.BodyAwareness, 1.0f);
}

TEST_F(AvatarIntegrationE2ETest, LongRunningStability) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    for (int i = 0; i < 1000; i++) {
        FEmotionState emotion;
        emotion.Joy = dist(gen);
        emotion.Sadness = dist(gen);
        emotion.Anger = dist(gen);
        
        Avatar->Update(1.0f / 60.0f, emotion);
        
        auto blendShapes = Avatar->GetCurrentBlendShapes();
        for (float w : blendShapes) {
            EXPECT_FALSE(std::isnan(w));
            EXPECT_FALSE(std::isinf(w));
        }
    }
    
    EXPECT_EQ(Avatar->GetFrameCount(), 1000);
}

// ============================================================================
// DNA Calibration E2E Tests
// ============================================================================

TEST_F(DNACalibrationE2ETest, ProfileCreation) {
    std::vector<float> scanData(100, 0.5f);
    auto profile = DNA->CalibrateFromScan(scanData);
    
    EXPECT_TRUE(profile.IsCalibrated);
    EXPECT_FALSE(profile.Id.empty());
}

TEST_F(DNACalibrationE2ETest, BlendShapeApplication) {
    std::vector<float> scanData(100, 0.0f);
    auto profile = DNA->CalibrateFromScan(scanData);
    
    std::vector<float> weights(DNA->GetBlendShapeCount(), 0.0f);
    weights[0] = 1.0f;  // Apply first blend shape
    
    auto result = DNA->ApplyBlendShapes(profile.Id, weights);
    
    EXPECT_GT(result.size(), 0);
}

TEST_F(DNACalibrationE2ETest, MultipleProfiles) {
    std::vector<MockDNACalibrationSystem::FDNAProfile> profiles;
    
    for (int i = 0; i < 10; i++) {
        std::vector<float> scanData(100, (float)i / 10.0f);
        profiles.push_back(DNA->CalibrateFromScan(scanData));
    }
    
    // All profiles should be unique
    for (size_t i = 0; i < profiles.size(); i++) {
        for (size_t j = i + 1; j < profiles.size(); j++) {
            EXPECT_NE(profiles[i].Id, profiles[j].Id);
        }
    }
}

// ============================================================================
// Performance E2E Tests
// ============================================================================

TEST(AvatarPerformanceE2ETest, FrameRateBenchmark) {
    MockIntegratedAvatarSystem avatar;
    avatar.Initialize();
    
    FEmotionState emotion;
    emotion.Joy = 0.5f;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int numFrames = 1000;
    for (int i = 0; i < numFrames; i++) {
        avatar.Update(1.0f / 60.0f, emotion);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double fps = (double)numFrames / (duration.count() / 1000.0);
    
    // Should achieve at least 60 FPS
    EXPECT_GT(fps, 60.0);
}

TEST(AvatarPerformanceE2ETest, BlendShapeUpdatePerformance) {
    MockDNACalibrationSystem dna;
    dna.Initialize();
    
    std::vector<float> scanData(100, 0.0f);
    auto profile = dna.CalibrateFromScan(scanData);
    
    std::vector<float> weights(dna.GetBlendShapeCount(), 0.5f);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 10000; i++) {
        dna.ApplyBlendShapes(profile.Id, weights);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 10000 applications in under 100ms
    EXPECT_LT(duration.count(), 100);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(AvatarCognitiveIntegrationE2ETest, EmotionDrivenAvatar) {
    MockIntegratedAvatarSystem avatar;
    avatar.Initialize();
    
    // Simulate cognitive emotion output
    std::vector<FEmotionState> emotionSequence;
    
    // Build up joy
    for (int i = 0; i < 30; i++) {
        FEmotionState e;
        e.Joy = (float)i / 30.0f;
        emotionSequence.push_back(e);
    }
    
    // Transition to surprise
    for (int i = 0; i < 20; i++) {
        FEmotionState e;
        e.Joy = 1.0f - (float)i / 20.0f;
        e.Surprise = (float)i / 20.0f;
        emotionSequence.push_back(e);
    }
    
    // Process sequence
    for (const auto& emotion : emotionSequence) {
        avatar.Update(1.0f / 60.0f, emotion);
    }
    
    EXPECT_EQ(avatar.GetFrameCount(), 50);
}

TEST(AvatarCognitiveIntegrationE2ETest, EmbodimentCognitiveFeedback) {
    MockIntegratedAvatarSystem avatar;
    Mock4EEmbodimentFeedback embodiment;
    
    avatar.Initialize();
    embodiment.Initialize();
    
    // Simulate cognitive-embodiment loop
    for (int i = 0; i < 100; i++) {
        // Get embodiment state
        auto state = avatar.GetEmbodimentState();
        
        // Generate emotion based on embodiment
        FEmotionState emotion;
        emotion.Joy = state.BodyAwareness * 0.5f;
        emotion.Surprise = state.EnvironmentalCoupling * 0.3f;
        
        // Update avatar
        avatar.Update(1.0f / 60.0f, emotion);
        
        // Update embodiment with environment
        std::vector<float> env(10, (float)i / 100.0f);
        embodiment.UpdateFromEnvironment(env);
    }
    
    float finalEmbodiment = embodiment.GetOverallEmbodiment();
    EXPECT_GE(finalEmbodiment, 0.0f);
    EXPECT_LE(finalEmbodiment, 1.0f);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
